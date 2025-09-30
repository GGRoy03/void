// [API]

internal size_t
GetSubRegistryFootprint(void)
{
    size_t StyleNames   = sizeof(ui_style_name)   * MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY;
    size_t CachedStyle  = sizeof(ui_cached_style) * MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY - MAXIMUM_STYLE_NAME_LENGTH;
    size_t Sentinels    = sizeof(ui_cached_style) * MAXIMUM_STYLE_NAME_LENGTH;
    size_t StringBuffer = sizeof(u8)              * MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY * MAXIMUM_STYLE_NAME_LENGTH;
    size_t Result       = StyleNames + CachedStyle + Sentinels + StringBuffer;

    return Result;
}

internal ui_style_subregistry *
CreateStyleSubregistry(byte_string FileName, memory_arena *OutputArena)
{
    read_only u64 FileSizeLimit      = Gigabyte(1);
    read_only u64 TokenBufferLimit   = MAXIMUM_STYLE_TOKEN_COUNT_PER_FILE * sizeof(style_token);
    read_only u64 StyleVarTableLimit = GetStyleVarTableFootprint(STYLE_VAR_TABLE_PARAMS);

    memory_arena *Arena = 0;
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.ReserveSize = FileSizeLimit + TokenBufferLimit + StyleVarTableLimit;
        Params.CommitSize = ArenaDefaultCommitSize;

        Arena = AllocateArena(Params);
    }

    ui_style_subregistry *Result = 0;

    os_handle FileHandle = OSFindFile(FileName);
    u64       FileSize   = OSFileSize(FileHandle);

    if (OSIsValidHandle(FileHandle) && FileSize <= FileSizeLimit)
    {
        os_read_file File = OSReadFile(FileHandle, Arena);

        tokenized_style_file TokenizedFile = TokenizeStyleFile(File, Arena);
        if (TokenizedFile.HasError)
        {
            LogStyleFileMessage(0, OSMessage_Warn, byte_string_literal("Failed to tokenize file. See error(s) above."));
            Result->HadError = 1;
            return Result;
        }

        Result = PushArray(OutputArena, ui_style_subregistry, 1);
        Result->CachedNames  = PushArray(OutputArena, ui_style_name  , MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY);
        Result->Sentinels    = PushArray(OutputArena, ui_cached_style, MAXIMUM_STYLE_NAME_LENGTH);
        Result->CachedStyles = PushArray(OutputArena, ui_cached_style, MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY - MAXIMUM_STYLE_NAME_LENGTH);
        Result->StringBuffer = PushArray(OutputArena, u8             , MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY * MAXIMUM_STYLE_NAME_LENGTH);

        b32 Success = ParseTokenizedStyleFile(&TokenizedFile, Arena, Result);
        if (!Success)
        {
            LogStyleFileMessage(0, OSMessage_Warn, byte_string_literal("Failed to parse file. See error(s) above."));
            Result->HadError = 1;
            return Result;
        }
    }

    ReleaseArena(Arena);
    if (OSIsValidHandle(FileHandle))
    {
        OSReleaseFile(FileHandle);
    }

    return Result;
}

// WARN: May 'chain' the output arena

internal ui_style_registry
CreateStyleRegistry(byte_string *FileNames, u32 Count, memory_arena *OutputArena)
{
    Assert(FileNames && Count > 0);

    ui_style_registry Result = {0};
    
    for (u32 FileIdx = 0; FileIdx < Count; FileIdx++)
    {
        ui_style_subregistry *Sub = CreateStyleSubregistry(FileNames[FileIdx], OutputArena);
        if (!Sub->HadError)
        {
            AppendToLinkedList((&Result), Sub, Result.Count);
        }
        else
        {
            // TODO: Create something for formatting? And output the file name.
            OSLogMessage(byte_string_literal("Could not load styles from file: "), OSMessage_Warn);
        }
    }

    return Result;
}

// [Tokenizer]

internal style_token *
EmitStyleToken(UIStyleToken_Type Type, tokenized_style_file *Tokenizer)
{
    style_token *Result = 0;

    if (Tokenizer->BufferSize < MAXIMUM_STYLE_TOKEN_COUNT_PER_FILE)
    {
        Result = Tokenizer->Buffer + Tokenizer->BufferSize++;
        Result->LineInFile = Tokenizer->LineCount;
        Result->Type       = Type;
    }
    else
    {
    }

    return Result;
}

internal b32
ReadUnit(os_read_file *File, u32 CurrentLineInFile, ui_unit *Result)
{
    f64 Number = 0;

    while (IsValidFile(File))
    {
        u8 Char = PeekFile(File, 0);
        if (IsDigit(Char))
        {
            Number = (Number * 10) + (Char - '0');
            AdvanceFile(File, 1);
        }
        else
        {
            break;
        }
    }

    if (IsValidFile(File))
    {
        if (PeekFile(File, 0) == UIStyleToken_Period)
        {
            AdvanceFile(File, 1); // Consumes '.'

            f64 C = 1.0 / 10.0;
            while (IsValidFile(File))
            {
                u8 Char = PeekFile(File, 0);
                if (IsDigit(Char))
                {
                    Number = Number + (C * (f64)(Char - '0'));
                    C     *= 1.0f / 10.0f;

                    AdvanceFile(File, 1);
                }
                else
                {
                    break;
                }
            }

            if (IsValidFile(File))
            {
                if (PeekFile(File, 0) == UIStyleToken_Percent)
                {
                    Result->Type = UIUnit_Percent;

                    if (Number >= 0.f && Number <= 100.f)
                    {
                        Result->Percent = (f32)Number;
                        AdvanceFile(File, 1);
                    }
                    else
                    {
                        LogStyleFileMessage(CurrentLineInFile, OSMessage_Error, byte_string_literal("Percent value must be >= 0.0 AND <= 100.0"));
                        return 0;
                    }
                }
                else
                {
                    Result->Type    = UIUnit_Float32;
                    Result->Float32 = (f32)Number;
                }
            }
            else
            {
                return 0;
            }
        }
        else if (PeekFile(File, 0) == UIStyleToken_Percent)
        {
            Result->Type = UIUnit_Percent;

            if (Number >= 0.f && Number <= 100.f)
            {
                Result->Percent = (f32)Number;
                AdvanceFile(File, 1);
            }
            else
            {
                LogStyleFileMessage(CurrentLineInFile, OSMessage_Error, byte_string_literal("Percent value must be >= 0.0 AND <= 100.0"));
                return 0;
            }
        }
        else
        {
            Result->Type    = UIUnit_Float32;
            Result->Float32 = (f32)Number;
        }
    }
    else
    {
        return 0;
    }

    return 1;
}

internal b32
ReadString(os_read_file *File, byte_string *OutString)
{
    OutString->String = PeekFilePointer(File);
    OutString->Size   = 0;

    while (IsValidFile(File))
    {
        u8 Character = PeekFile(File, 0);
        if (IsAlpha(Character) || Character == '_')
        {
            ++OutString->Size;
            AdvanceFile(File, 1);
        }
        else
        {
            break;
        }
    }

    b32 Result = (OutString->String != 0) && (OutString->Size != 0);
    return Result;
}

internal b32
ReadVector(os_read_file *File, u32 MinimumSize, u32 MaximumSize, u32 CurrentLineInFile, style_token *Result)
{   Assert(PeekFile(File, 0) == '[');

    AdvanceFile(File, 1); // Skips '['

    u32 Count = 0;
    while (Count < MaximumSize)
    {
        SkipWhiteSpaces(File);

        u8 Character = PeekFile(File, 0);
        if (!IsDigit(Character))
        {
            LogStyleFileMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Vectors must only contain digits."));
            return 0;
        }

        b32 Success = ReadUnit(File, CurrentLineInFile, &Result->Vector.Values[Count++]);
        if(!Success)
        {
            LogStyleFileMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Could not parse unit."));
            return 0;
        }

        SkipWhiteSpaces(File);

        Character = PeekFile(File, 0);
        if (Character == ',')
        {
            AdvanceFile(File, 1);
            continue;
        }
        else if (Character == ']')
        {
            AdvanceFile(File, 1);
            break;
        }
        else
        {
            LogStyleFileMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Invalid character found in vector: %c"), Character);
            return 0;
        }
    }

    b32 Valid = (Count >= MinimumSize) && (Count <= MaximumSize);
    return Valid;
}

internal style_token *
PeekStyleToken(tokenized_style_file *TokenizedFile, u32 Offset)
{
    style_token *Result = 0;

    u32 PostIndex = TokenizedFile->AtToken + Offset;
    if (PostIndex < MAXIMUM_STYLE_TOKEN_COUNT_PER_FILE)
    {
        Result = TokenizedFile->Buffer + PostIndex;
    }

    return Result;
}

internal void
ConsumeStyleTokens(tokenized_style_file *TokenizedFile, u32 Count)
{
    if (TokenizedFile->AtToken + Count < MAXIMUM_STYLE_TOKEN_COUNT_PER_FILE)
    {
        TokenizedFile->AtToken += Count;
    }
}

internal tokenized_style_file
TokenizeStyleFile(os_read_file File, memory_arena *Arena)
{
    tokenized_style_file Result = {0};
    Result.Buffer   = PushArray(Arena, style_token, MAXIMUM_STYLE_TOKEN_COUNT_PER_FILE);
    Result.HasError = 1;

    while (IsValidFile(&File))
    {
        SkipWhiteSpaces(&File);

        u8 Char = PeekFile(&File, 0);

        if (IsAlpha(Char))
        {
            byte_string Identifier;
            b32 Success = ReadString(&File, &Identifier);
            if (!Success)
            {
                LogStyleFileMessage(0, OSMessage_Error, byte_string_literal("Could not parse string. EOF?"));
                return Result;
            }

            if (ByteStringMatches(Identifier, byte_string_literal("style"), StringMatch_NoFlag))
            {
                EmitStyleToken(UIStyleToken_Style, &Result);
            }
            else if (ByteStringMatches(Identifier, byte_string_literal("for"), StringMatch_NoFlag))
            {
                EmitStyleToken(UIStyleToken_For, &Result);
            }
            else if (ByteStringMatches(Identifier, byte_string_literal("var"), StringMatch_NoFlag))
            {
                EmitStyleToken(UIStyleToken_Var, &Result);
            }
            else
            {
                style_token *Token = EmitStyleToken(UIStyleToken_Identifier, &Result);
                if (Token)
                {
                    Token->Identifier = Identifier;
                }
            }

            continue;
        }

        if (IsDigit(Char))
        {
            style_token *Token = EmitStyleToken(UIStyleToken_Unit, &Result);
            if (Token)
            {
                b32 Success = ReadUnit(&File, Result.LineCount, &Token->Unit);
                if (!Success)
                {
                    LogStyleFileMessage(Result.LineCount, OSMessage_Error, byte_string_literal("Failed to parse unit."));
                    return Result;
                }
            }

            continue;
        }

        if (Char == '\r' || Char == '\n')
        {
            u8 Next = PeekFile(&File, 1);
            if (Char == '\r' && Next == '\n')
            {
                AdvanceFile(&File, 1);
            }

            Result.LineCount += 1;
            AdvanceFile(&File, 1);

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(&File, 1) == '=')
            {
                EmitStyleToken(UIStyleToken_Assignment, &Result);
                AdvanceFile(&File, 2);
            }
            else
            {
                LogStyleFileMessage(Result.LineCount, OSMessage_Error, byte_string_literal("Stray ':'. Did you mean := ?"));
                return Result;
            }

            continue;
        }

        if (Char == '[')
        {
            style_token *Token = EmitStyleToken(UIStyleToken_Vector, &Result);
            if (Token)
            {
                read_only u32 MinimumVectorSize = 2;
                read_only u32 MaximumVectorSize = 4;

                b32 Success = ReadVector(&File, MinimumVectorSize, MaximumVectorSize, Result.LineCount, Token);
                if (!Success)
                {
                    LogStyleFileMessage(Result.LineCount, OSMessage_Error, byte_string_literal("Could not read vector. See error(s) above."));
                    return Result;
                }
            }

            continue;
        }

        if (Char == '"')
        {
            AdvanceFile(&File, 1); // Skip the first '"'

            byte_string String;
            b32 Success = ReadString(&File, &String);
            if (!Success)
            {
                LogStyleFileMessage(Result.LineCount, OSMessage_Error, byte_string_literal("Could not parse string. EOF?"));
                return Result;
            }

            if (PeekFile(&File, 0) != '"')
            {
                LogStyleFileMessage(Result.LineCount, OSMessage_Error, byte_string_literal("Could not parse string. Invalid Characters?"));
                return Result;
            }

            style_token *Token = EmitStyleToken(UIStyleToken_String, &Result);
            if (Token)
            {
                Token->Identifier = String;
            }

            AdvanceFile(&File, 1); // Skip the second '"'

            continue;
        }

        if (Char == '{' || Char == '}' || Char == ';' || Char == '.' || Char == ',' || Char == '%' || Char == '@')
        {
            AdvanceFile(&File, 1);
            EmitStyleToken((UIStyleToken_Type)Char, &Result);

            continue;
        }

        LogStyleFileMessage(Result.LineCount, OSMessage_Error, byte_string_literal("Invalid character found in file: %c"), Char);
        break;
    }

    EmitStyleToken(UIStyleToken_EndOfFile, &Result);

    Result.HasError = 0;
    return Result;
}

// [Parser]

internal b32 
ValidateVectorUnitType(vec4_unit Vec, UIUnit_Type MustBe, u32 Count, u32 Offset)
{   Assert(Count + Offset <= 4);

    for (u32 Idx = Offset; Idx < Count + Offset; Idx++)
    {
        if (Vec.Values[Idx].Type != MustBe)
        {
            return 0;
        }
    }

    return 1;
}

internal b32
ValidateColor(vec4_unit Vec)
{
    for (u32 Idx = 0; Idx < 4; Idx++)
    {
        // Validate that there is only plain floats.
        if (Vec.Values[Idx].Type != UIUnit_Float32)
        {
            return 0;
        }

        // Validate the bounds of the values [0..255]
        if (!(Vec.Values[Idx].Float32 >= 0 && Vec.Values[Idx].Float32 <= 255))
        {
            return 0;
        }
    }

    return 1;
}

internal ui_color
ToNormalizedColor(vec4_unit Vec)
{
    f32      Inverse = 1.f / 255;
    ui_color Result  = UIColor(Vec.X.Float32 * Inverse, Vec.Y.Float32 * Inverse, Vec.Z.Float32 * Inverse, Vec.W.Float32 * Inverse);
    return Result;
}

internal UIStyleAttribute_Flag
GetStyleAttributeFlag(byte_string Identifier)
{
    UIStyleAttribute_Flag Result = UIStyleAttribute_None;

    // Valid Types (Clearer as a non-table) (Can be better?)
    byte_string Size         = byte_string_literal("size");
    byte_string Color        = byte_string_literal("color");
    byte_string Padding      = byte_string_literal("padding");
    byte_string Spacing      = byte_string_literal("spacing");
    byte_string FontSize     = byte_string_literal("fontsize");
    byte_string FontName     = byte_string_literal("fontname");
    byte_string Softness     = byte_string_literal("softness");
    byte_string BorderWidth  = byte_string_literal("borderwidth");
    byte_string BorderColor  = byte_string_literal("bordercolor");
    byte_string CornerRadius = byte_string_literal("cornerradius");

    if (ByteStringMatches(Identifier, Size, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_Size;
    }
    else if (ByteStringMatches(Identifier, Color, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_Color;
    }
    else if (ByteStringMatches(Identifier, Padding, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_Padding;
    }
    else if (ByteStringMatches(Identifier, Spacing, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_Spacing;
    }
    else if (ByteStringMatches(Identifier, FontSize, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_FontSize;
    }
    else if (ByteStringMatches(Identifier, FontName, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_FontName;
    }
    else if (ByteStringMatches(Identifier, Softness, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_Softness;
    }
    else if (ByteStringMatches(Identifier, BorderWidth, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_BorderWidth;
    }
    else if (ByteStringMatches(Identifier, BorderColor, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_BorderColor;
    }
    else if (ByteStringMatches(Identifier, CornerRadius, StringMatch_NoFlag))
    {
        Result = UIStyleAttribute_CornerRadius;
    }

    return Result;
}

internal b32
IsAttributeFormattedCorrectly(UIStyleToken_Type TokenType, UIStyleAttribute_Flag AttributeFlag)
{
    b32 Result = 0;

    switch (TokenType)
    {

    case UIStyleToken_Unit:
    {
        Result = (AttributeFlag & UIStyleAttribute_BorderWidth) ||
                 (AttributeFlag & UIStyleAttribute_Softness   ) ||
                 (AttributeFlag & UIStyleAttribute_FontSize   );
    } break;

    case UIStyleToken_Vector:
    {
        Result = (AttributeFlag & UIStyleAttribute_BorderColor ) ||
                 (AttributeFlag & UIStyleAttribute_CornerRadius) ||
                 (AttributeFlag & UIStyleAttribute_Padding     ) ||
                 (AttributeFlag & UIStyleAttribute_Spacing     ) ||
                 (AttributeFlag & UIStyleAttribute_Color       ) ||
                 (AttributeFlag & UIStyleAttribute_Size        );
    } break;

    case UIStyleToken_String:
    {
        Result = (AttributeFlag & UIStyleAttribute_FontName);
    } break;

    default: break;

    }

    return Result;
}

internal b32
SaveStyleAttribute(UIStyleAttribute_Flag Attribute, style_token *Value, style_parser *Parser)
{
    b32 Result = 0;
    
    if (Parser->StyleType != UIStyle_None)
    {
        ui_style *Style = &Parser->Styles[Parser->StyleType];

        switch (Attribute)
        {

        case UIStyleAttribute_Size:
        {
            Result = ValidateVectorUnitType(Value->Vector, UIUnit_None, 2, 2);
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Size must be: [Width, Height]"));
                return Result;
            }

            Style->Size = Vec2Unit(Value->Vector.X, Value->Vector.Y);
            SetFlag(Style->Flags, UIStyleNode_HasSize);
        } break;

        case UIStyleAttribute_Color:
        {
            Result = ValidateColor(Value->Vector);
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Color values must be included in [0, 255]"));
                return Result;
            }

            Style->Color = ToNormalizedColor(Value->Vector);
            SetFlag(Style->Flags, UIStyleNode_HasColor);
        } break;

        case UIStyleAttribute_Padding:
        {
            Result = ValidateVectorUnitType(Value->Vector, UIUnit_Float32, 4, 0);
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Padding must be: [Float, Float, Float, Float]"));
                return Result;
            }

            Style->Padding = UIPadding(Value->Vector.X.Float32, Value->Vector.Y.Float32, Value->Vector.Z.Float32, Value->Vector.W.Float32);
            SetFlag(Style->Flags, UIStyleNode_HasPadding);
        } break;

        case UIStyleAttribute_Spacing:
        {
            Result = (ValidateVectorUnitType(Value->Vector, UIUnit_None, 2, 2));
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Spacing must be: [Horizontal, Vertical]"));
                return Result;
            }

            Result = ValidateVectorUnitType(Value->Vector, UIUnit_Float32, 2, 0);
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Spacing must be: [Float, Float]"));
                return Result;
            }

            Style->Spacing = UISpacing(Value->Vector.X.Float32, Value->Vector.Y.Float32);
            SetFlag(Style->Flags, UIStyleNode_HasSpacing);
        } break;

        case UIStyleAttribute_FontSize:
        {
            Style->FontSize = Value->Unit.Float32;
            SetFlag(Style->Flags, UIStyleNode_HasFontSize);
            Result = 1;
        } break;

        case UIStyleAttribute_FontName:
        {
            Style->Font.Name = Value->Identifier;
            SetFlag(Style->Flags, UIStyleNode_HasFontName);
            Result = 1;
        } break;

        case UIStyleAttribute_Softness:
        {
            Style->Softness = Value->Unit.Float32;
            SetFlag(Style->Flags, UIStyleNode_HasSoftness);
            Result = 1;
        } break;

        case UIStyleAttribute_BorderColor:
        {
            Result = ValidateColor(Value->Vector);
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Color values must be included in [0, 255]"));
                return Result;
            }

            Style->BorderColor = ToNormalizedColor(Value->Vector);
            SetFlag(Style->Flags, UIStyleNode_HasBorderColor);
        } break;

        case UIStyleAttribute_BorderWidth:
        {
            Style->BorderWidth = Value->Unit.Float32;
            SetFlag(Style->Flags, UIStyleNode_HasBorderWidth);
            Result = 1;
        } break;

        case UIStyleAttribute_CornerRadius:
        {
            Result = ValidateVectorUnitType(Value->Vector, UIUnit_Float32, 4, 0);
            if (!Result)
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Corner Radius must be: [Float, Float, Float, Float]"));
                return Result;
            }

            Style->CornerRadius = UICornerRadius(Value->Vector.X.Float32, Value->Vector.Y.Float32, Value->Vector.Z.Float32, Value->Vector.W.Float32);

            SetFlag(Style->Flags, UIStyleNode_HasCornerRadius);
        } break;

        }
    }

    return Result;
}

internal ui_cached_style *
CreateCachedStyle(ui_style Style, ui_style_subregistry *Registery)
{
    ui_cached_style *Result = 0;

    if (Registery->Count < MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY)
    {
        Result = Registery->CachedStyles + Registery->Count;
        Result->Index = Registery->Count;
        Result->Next  = 0;
        Result->Style = Style;

        ++Registery->Count;
    }

    return Result;
}

internal ui_style_name *
CreateCachedStyleName(byte_string Name, ui_cached_style *CachedStyle, ui_style_subregistry *Registry)
{
    ui_style_name *Result = 0;

    if (Registry->Count < MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY)
    {
        Result = Registry->CachedNames + CachedStyle->Index;
        Assert(!IsValidByteString(Result->Value));

        Result->Value.String = Registry->StringBuffer + Registry->StringBufferOffset;
        Result->Value.Size   = Name.Size;

        memcpy(Result->Value.String, Name.String, Name.Size);
        Registry->StringBufferOffset += Name.Size;
    }

    return Result;
}

internal ui_cached_style *
CacheStyle(ui_style Style, byte_string Name, UIStyle_Type Type, ui_cached_style *BaseStyle, ui_style_subregistry *Registery)
{
    ui_cached_style *Result = 0;

    if (Name.Size && Name.Size <= MAXIMUM_STYLE_NAME_LENGTH)
    {
        Result = CreateCachedStyle(Style, Registery);
        if (Result)
        {
            switch (Type)
            {
            case UIStyle_Base:
            {
                CreateCachedStyleName(Name, Result, Registery);

                // TODO: Change this?
                ui_cached_style *Sentinel = UIGetStyleSentinel(Name, Registery);
                if (Sentinel->Next)
                {
                    Sentinel->Next->Next = Result;
                }
                Result->Next   = Sentinel->Next;
                Sentinel->Next = Result;
            } break;

            case UIStyle_Click:
            {
                BaseStyle->Style.ClickOverride = &Result->Style;
            } break;

            case UIStyle_Hover:
            {
                BaseStyle->Style.HoverOverride = &Result->Style;
            } break;

            }
        }
    }
    else
    {
        LogStyleFileMessage(0, OSMessage_Error, byte_string_literal("Style name size must be in the range [1..%u]"), MAXIMUM_STYLE_NAME_LENGTH);
    }

    return Result;
}

internal b32
ParseStyleAttribute(style_parser *Parser, tokenized_style_file *TokenizedFile, UINode_Type ParsingFor)
{
    // Check if a new effect is set.
    {
        style_token *Effect = PeekStyleToken(TokenizedFile, 0);
        if (Effect->Type == UIStyleToken_AtSymbol)
        {
            style_token *EffectName = PeekStyleToken(TokenizedFile, 1);
            if (EffectName->Type == UIStyleToken_Identifier)
            {
                if (ByteStringMatches(EffectName->Identifier, byte_string_literal("base"), StringMatch_NoFlag))
                {
                    Parser->StyleType                 = UIStyle_Base;
                    Parser->StyleIsSet[UIStyle_Base] = 1;
                }
                else if (ByteStringMatches(EffectName->Identifier, byte_string_literal("click"), StringMatch_NoFlag))
                {
                    Parser->StyleType                 = UIStyle_Click;
                    Parser->StyleIsSet[UIStyle_Click] = 1;
                }
                else if (ByteStringMatches(EffectName->Identifier, byte_string_literal("hover"), StringMatch_NoFlag))
                {
                    Parser->StyleType                 = UIStyle_Hover;
                    Parser->StyleIsSet[UIStyle_Hover] = 1;
                }
                else
                {
                    LogStyleFileMessage(EffectName->LineInFile, OSMessage_Error, byte_string_literal("Unknown effect name."));
                    return 0;
                }

                ConsumeStyleTokens(TokenizedFile, 2);
                return 1;
            }
            else
            {
                LogStyleFileMessage(EffectName->LineInFile, OSMessage_Error, byte_string_literal("Expect identifier after trying to set an effect with @."));
                return 0;
            }
        }
    }

    // Validates the attributes.
    UIStyleAttribute_Flag Flag = UIStyleAttribute_None;
    {
        style_token *AttributeName = PeekStyleToken(TokenizedFile, 0);
        if (AttributeName->Type != UIStyleToken_Identifier)
        {
            LogStyleFileMessage(AttributeName->LineInFile, OSMessage_Error, byte_string_literal("Expected: Attribute Name"));
            return 0;
        }

        Flag = GetStyleAttributeFlag(AttributeName->Identifier);
        if (Flag == UIStyleAttribute_None)
        {
            LogStyleFileMessage(AttributeName->LineInFile, OSMessage_Error, byte_string_literal("Invalid: Attribute Name"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    // Validates the assignment
    {
        style_token *Assignment = PeekStyleToken(TokenizedFile, 0);
        if (Assignment->Type != UIStyleToken_Assignment)
        {
            LogStyleFileMessage(Assignment->LineInFile, OSMessage_Error, byte_string_literal("Expected: Assignment"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    // Validates the value assigned to the attribute
    {
        style_token *Value = PeekStyleToken(TokenizedFile, 0);
        if (Value->Type != UIStyleToken_Unit && Value->Type != UIStyleToken_String && Value->Type != UIStyleToken_Vector && Value->Type != UIStyleToken_Identifier)
        {
            LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Expected: Value"));
            return 0;
        }

        if (!(Flag & StyleTypeValidAttributesTable[ParsingFor]))
        {
            LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Invalid attribute supplied to a theme. Invalid: %s"), StyleAttributeToString(Flag));
            return 0;
        }

        if (Value->Type == UIStyleToken_Identifier)
        {
            style_var_hash   Hash  = HashStyleVarIdentifier(Value->Identifier);
            style_var_entry *Entry = FindStyleVarEntry(Hash, Parser->VarTable);
            if (Entry->ValueIsParsed)
            {
                Value = Entry->ValueToken;
            }
            else
            {
                LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Unnknown variable."));
                return 0;
            }
        }

        if (!IsAttributeFormattedCorrectly(Value->Type, Flag))
        {
            LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Invalid formatting for %s"), StyleAttributeToString(Flag));
            return 0;
        }

        b32 Saved = SaveStyleAttribute(Flag, Value, Parser);
        if (!Saved)
        {
            LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Failed to save : %s. See error(s) above."), StyleAttributeToString(Flag));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    // Validates the end of the expression.
    {
        style_token *EndOfAttribute = PeekStyleToken(TokenizedFile, 0);
        if (EndOfAttribute->Type != UIStyleToken_SemiColon)
        {
            LogStyleFileMessage(EndOfAttribute->LineInFile, OSMessage_Error, byte_string_literal("Expected: ';' after setting an attribute."));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    return 1;
}

internal b32
ParseStyleHeader(style_parser *Parser, tokenized_style_file *TokenizedFile, ui_style_subregistry *Registery)
{
    {
        style_token *Style = PeekStyleToken(TokenizedFile, 0);
        if (Style->Type != UIStyleToken_Style)
        {
            LogStyleFileMessage(Style->LineInFile, OSMessage_Error, byte_string_literal("Expected: 'Style'"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    {
        style_token *Name = PeekStyleToken(TokenizedFile, 0);
        if (Name->Type != UIStyleToken_String)
        {
            LogStyleFileMessage(Name->LineInFile, OSMessage_Error, byte_string_literal("Expected: '\"style_name\""));
            return 0;
        }

        ui_style_name CachedName = UIGetCachedNameFromSubregistry(Name->Identifier, Registery);
        if (IsValidByteString(CachedName.Value))
        {
            LogStyleFileMessage(Name->LineInFile, OSMessage_Error, byte_string_literal("A style with the same name already exists."));
            return 0;
        }

        Parser->StyleName = Name->Identifier;

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    {
        style_token *For = PeekStyleToken(TokenizedFile, 0);
        if (For->Type != UIStyleToken_For)
        {
            LogStyleFileMessage(For->LineInFile, OSMessage_Error, byte_string_literal("Expected: 'For'"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    UINode_Type ParsingFor = UINode_None;
    {
        style_token *Type = PeekStyleToken(TokenizedFile, 0);
        if (Type->Type != UIStyleToken_Identifier)
        {
            LogStyleFileMessage(0, OSMessage_Error, byte_string_literal("Expected: 'Type'"));
            return 0;
        }

        if (ByteStringMatches(Type->Identifier, byte_string_literal("window"), StringMatch_NoFlag))
        {
            ParsingFor = UINode_Window;
        }
        else if (ByteStringMatches(Type->Identifier, byte_string_literal("button"), StringMatch_NoFlag))
        {
            ParsingFor = UINode_Button;
        }
        else if (ByteStringMatches(Type->Identifier, byte_string_literal("label"), StringMatch_NoFlag))
        {
            ParsingFor = UINode_Label;
        }
        else if (ByteStringMatches(Type->Identifier, byte_string_literal("header"), StringMatch_NoFlag))
        {
            ParsingFor = UINode_Header;
        }
        else
        {
            LogStyleFileMessage(Type->LineInFile, OSMessage_Error, byte_string_literal("Expected: 'Type'"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    {
        style_token *NextToken = PeekStyleToken(TokenizedFile, 0);
        if (NextToken->Type != UIStyleToken_OpenBrace)
        {
            LogStyleFileMessage(NextToken->LineInFile, OSMessage_Error, byte_string_literal("Expect: '{' after style header."));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);

        while (NextToken->Type != UIStyleToken_CloseBrace)
        {
            if (NextToken == UIStyleToken_EndOfFile)
            {
                LogStyleFileMessage(NextToken->LineInFile, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return 0;
            }

            if (!ParseStyleAttribute(Parser, TokenizedFile, ParsingFor))
            {
                LogStyleFileMessage(NextToken->LineInFile, OSMessage_Error, byte_string_literal("Failed to parse an attribute. See error(s) above."));
                return 0;
            }

            NextToken = PeekStyleToken(TokenizedFile, 0);
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    return 1;
}

internal b32
ParseStyleVariable(style_parser *Parser, tokenized_style_file *TokenizedFile)
{
    {
        style_token *VarToken = PeekStyleToken(TokenizedFile, 0);
        if(VarToken->Type != UIStyleToken_Var)
        {
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    style_var_entry *Entry = 0;
    {
        style_token *Name = PeekStyleToken(TokenizedFile, 0);
        if(Name->Type != UIStyleToken_Identifier)
        {
            LogStyleFileMessage(Name->LineInFile, OSMessage_Error, byte_string_literal("Expected the name of the variable."));
            return 0;
        }

        style_var_hash Hash = HashStyleVarIdentifier(Name->Identifier);
        Entry = FindStyleVarEntry(Hash, Parser->VarTable);
        if (Entry->ValueIsParsed)
        {
            LogStyleFileMessage(Name->LineInFile, OSMessage_Error, byte_string_literal("Two variables cannot have the same name."));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    {
        style_token *Assignment = PeekStyleToken(TokenizedFile, 0);
        if(Assignment->Type != UIStyleToken_Assignment)
        {
            LogStyleFileMessage(Assignment->LineInFile, OSMessage_Error, byte_string_literal("Expect assignment after naming variable"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    {
        style_token *Value = PeekStyleToken(TokenizedFile, 0);
        if (Value->Type != UIStyleToken_Unit && Value->Type != UIStyleToken_String && Value->Type != UIStyleToken_Vector)
        {
            LogStyleFileMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Expected value."));
            return 0;
        }

        Entry->ValueIsParsed = 1;
        Entry->ValueToken    = Value;

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    {
        style_token *End = PeekStyleToken(TokenizedFile, 0);
        if(End->Type != UIStyleToken_SemiColon)
        {
            LogStyleFileMessage(End->LineInFile, OSMessage_Error, byte_string_literal("Expected ';'"));
            return 0;
        }

        ConsumeStyleTokens(TokenizedFile, 1);
    }

    return 1;
}

internal b32
ParseTokenizedStyleFile(tokenized_style_file *TokenizedFile, memory_arena *Arena, ui_style_subregistry *SubRegistery)
{
    style_parser Parser = {0};
    {
        size_t Footprint = GetStyleVarTableFootprint(STYLE_VAR_TABLE_PARAMS);
        void  *Memory    = PushArena(Arena, Footprint, AlignOf(style_var_table *));

        Parser.VarTable  = PlaceStyleVarTableInMemory(STYLE_VAR_TABLE_PARAMS, Memory);
        Parser.StyleType = UIStyle_None;
    }

    style_token *Next = PeekStyleToken(TokenizedFile, 0);
    while (Next->Type != UIStyleToken_EndOfFile)
    {
        while (ParseStyleVariable(&Parser, TokenizedFile)) {};

        if (!ParseStyleHeader(&Parser, TokenizedFile, SubRegistery))
        {
            LogStyleFileMessage(0, OSMessage_Warn, byte_string_literal("Failed to parse style. See error(s) above."));
            return 0;
        }
        
        ui_cached_style *CachedBaseStyle = 0;
        ForEachEnum(UIStyle_Type, Type)
        {
            if (Parser.StyleIsSet[Type])
            {
                if (Type == UIStyle_Base)
                {
                    CachedBaseStyle = CacheStyle(Parser.Styles[Type], Parser.StyleName, Type, 0, SubRegistery);
                }
                else
                {
                    if (CachedBaseStyle)
                    {
                        CacheStyle(Parser.Styles[Type], Parser.StyleName, Type, CachedBaseStyle, SubRegistery);
                    }
                    else
                    {
                        LogStyleFileMessage(0, OSMessage_Warn, byte_string_literal("Effects such as @Hover are only available if the style has a base style."));
                    }
                }

                Parser.StyleIsSet[Type] = 0;
                Parser.Styles[Type]     = (ui_style){0};
            }
        }

        if (CachedBaseStyle)
        {
            CachedBaseStyle->Style.Version = 1;
        }

        Parser.StyleName = ByteString(0, 0);
        Parser.StyleType = UIStyle_None;

        Next = PeekStyleToken(TokenizedFile, 0);
    }

    return 1;
}

// [Variables]

internal style_var_table *
PlaceStyleVarTableInMemory(style_var_table_params Params, void *Memory)
{
    Assert(Params.EntryCount > 0);
    Assert(Params.HashCount > 0);
    Assert(IsPowerOfTwo(Params.HashCount));

    style_var_table *Result = 0;

    if (Memory)
    {
        Result = (style_var_table *)Memory;
        Result->HashTable = (u32 *)(Result + 1);
        Result->Entries   = (style_var_entry *)(Result->HashTable + Params.HashCount);

        Result->HashMask   = Params.HashCount - 1;
        Result->HashCount  = Params.HashCount;
        Result->EntryCount = Params.EntryCount;

        MemorySet(Result->HashTable, 0, Result->HashCount * sizeof(Result->HashTable[0]));

        for (u32 Idx = 0; Idx < Params.EntryCount; Idx++)
        {
            style_var_entry *Entry = GetStyleVarEntry(Idx, Result);
            if ((Idx + 1) < Params.EntryCount)
            {
                Entry->NextWithSameHash = Idx + 1;
            }
            else
            {
                Entry->NextWithSameHash = 0;
            }
        }
    }

    return Result;
}

internal style_var_entry *
GetStyleVarSentinel(style_var_table *Table)
{
    style_var_entry *Result = Table->Entries;
    return Result;
}

internal style_var_entry *
GetStyleVarEntry(u32 Index, style_var_table *Table)
{
    Assert(Index < Table->EntryCount);

    style_var_entry *Result = Table->Entries + Index;
    return Result;
}

internal u32
PopFreeStyleVarEntry(style_var_table *Table)
{
    style_var_entry *Sentinel = GetStyleVarSentinel(Table);

    if (!Sentinel->NextWithSameHash)
    {
        LogStyleFileMessage(0, OSMessage_Error, byte_string_literal("Maximum amount of variables exceeded for file."));
        return 0;
    }

    u32              Result = Sentinel->NextWithSameHash;
    style_var_entry *Entry  = GetStyleVarEntry(Result, Table);

    Sentinel->NextWithSameHash = Entry->NextWithSameHash;
    Entry->NextWithSameHash    = 0;

    return Result;
}

internal style_var_entry *
FindStyleVarEntry(style_var_hash Hash, style_var_table *Table)
{
    u32 HashSlot = Hash.Value & Table->HashMask;
    u32 EntryIndex = Table->HashTable[HashSlot];

    style_var_entry *Result = 0;
    while (EntryIndex)
    {
        style_var_entry *Entry = GetStyleVarEntry(EntryIndex, Table);
        if (Hash.Value == Entry->Hash.Value)
        {
            Result = Entry;
            break;
        }

        EntryIndex = Entry->NextWithSameHash;
    }

    if (!Result)
    {
        EntryIndex = PopFreeStyleVarEntry(Table);
        if (EntryIndex)
        {
            Result = GetStyleVarEntry(EntryIndex, Table);
            Result->NextWithSameHash = Table->HashTable[HashSlot];
            Result->Hash             = Hash;

            Table->HashTable[HashSlot] = EntryIndex;
        }
    }

    Assert(Result);

    return Result;
}

internal style_var_hash
HashStyleVarIdentifier(byte_string Identifier)
{
    style_var_hash Result = { HashByteString(Identifier) };
    return Result;
}

internal size_t
GetStyleVarTableFootprint(style_var_table_params Params)
{
    size_t HashSize  = Params.HashCount * sizeof(u32);
    size_t EntrySize = Params.EntryCount * sizeof(style_var_entry);
    size_t Result    = sizeof(style_var_table) + HashSize + EntrySize;

    return Result;
}

// [Error Handling]

internal read_only char *
StyleAttributeToString(UIStyleAttribute_Flag Flag)
{
    switch (Flag)
    {

    case UIStyleAttribute_None:         return "None";
    case UIStyleAttribute_Size:         return "Size";
    case UIStyleAttribute_Color:        return "Color";
    case UIStyleAttribute_Padding:      return "Padding";
    case UIStyleAttribute_Spacing:      return "Spacing";
    case UIStyleAttribute_FontSize:     return "Font Size";
    case UIStyleAttribute_FontName:     return "Font Name";
    case UIStyleAttribute_Softness:     return "Softness";
    case UIStyleAttribute_BorderColor:  return "Border Color";
    case UIStyleAttribute_BorderWidth:  return "Border Width";
    case UIStyleAttribute_CornerRadius: return "Corner Radius";
    default:                            return "Unknown";

    }
}

internal void
LogStyleFileMessage(u32 LineInFile, OSMessage_Severity Severity, byte_string Format, ...)
{
    va_list Args;
    __crt_va_start(Args, Format);
    __va_start(&Args, Format);

    u8 Buffer[Kilobyte(4)];
    byte_string ErrorString = ByteString(Buffer, 0);

    if (LineInFile > 0)
    {
        ErrorString.Size = snprintf((char *)Buffer, sizeof(Buffer), "[Style File At Line %u] -> ", LineInFile);
    }
    else
    {
        ErrorString.Size = snprintf((char *)Buffer, sizeof(Buffer), "[Style File] -> ");
    }

    ErrorString.Size += vsnprintf((char *)(Buffer + ErrorString.Size), sizeof(Buffer), (char *)Format.String, Args);

    OSLogMessage(ErrorString, Severity);

    __crt_va_end(Args);
}