// [Globals]

// [Public API Implementation]

internal void
LoadThemeFiles(byte_string *Files, u32 FileCount, ui_style_registery *Registery, render_handle RendererHandle)
{
    if (!Files)
    {
        WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("'Files' must be a valid pointer."));

        return;
    }

    style_parser    Parser    = {0};
    style_tokenizer Tokenizer = {0};
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = Kilobyte(64);
        Params.ReserveSize       = Megabyte(10);


        Tokenizer.Arena    = AllocateArena(Params);
        Tokenizer.Capacity = 10'000;
        Tokenizer.Buffer   = PushArray(Tokenizer.Arena, style_token, Tokenizer.Capacity);
        Tokenizer.AtLine   = 1;
    }

    // NOTE: Basically an initialization routine... Probably move this?
    if (!Registery->Arena)
    {
        u32 SentinelCount    = ThemeNameLength;
        u32 CachedStyleCount = ThemeMaxCount - ThemeNameLength;

        // NOTE: While this is better than what we did previously, I still think we "waste" memory.
        // because this is worst case allocation. We might let the arena chain and then consolidate into
        // a single one.

        size_t Footprint = sizeof(ui_style_registery);
        Footprint += SentinelCount    * sizeof(ui_cached_style);
        Footprint += CachedStyleCount * sizeof(ui_cached_style);
        Footprint += CachedStyleCount * sizeof(ui_style_name);

        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = OSGetSystemInfo()->PageSize;
        Params.ReserveSize       = Footprint;

        Registery->Arena        = AllocateArena(Params);
        Registery->Sentinels    = PushArena(Registery->Arena, SentinelCount    * sizeof(ui_cached_style), AlignOf(ui_cached_style));
        Registery->CachedStyles = PushArena(Registery->Arena, CachedStyleCount * sizeof(ui_cached_style), AlignOf(ui_cached_style));
        Registery->CachedName   = PushArena(Registery->Arena, CachedStyleCount * sizeof(ui_style_name)  , AlignOf(ui_style_name));
        Registery->Capacity     = CachedStyleCount;
        Registery->Count        = 0;
    }

    for (u32 FileIdx = 0; FileIdx < FileCount; FileIdx++)
    {
        byte_string FileName     = Files[FileIdx];
        os_handle   OSFileHandle = OSFindFile(FileName);
        os_file     OSFile       = OSReadFile(OSFileHandle, Tokenizer.Arena); // BUG: Can easily overflow.
  
        if (!OSFile.FullyRead)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("File with path: %s does not exist on disk."), FileName);
            continue;
        }

        b32 TokenSuccess = TokenizeStyleFile(&OSFile, &Tokenizer);
        if (!TokenSuccess)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("Failed to tokenize file. See error(s) above."));
            continue;
        }

        b32 ParseSuccess = ParseStyleTokens(&Parser, Tokenizer.Buffer, Tokenizer.Count, Registery, RendererHandle);
        if (!ParseSuccess)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("Failed to parse file. See error(s) above."));
            continue;
        }

        Tokenizer.AtLine = 1;
        Tokenizer.Count  = 0;
        PopArenaTo(Tokenizer.Arena, 0); // BUG: Wrong pop?
    }

    ReleaseArena(Tokenizer.Arena);
}

// [Internal Implementation]

internal style_token *
CreateStyleToken(UIStyleToken_Type Type, style_tokenizer *Tokenizer)
{
    style_token *Result = 0;

    if (Tokenizer->Count == Tokenizer->Capacity)
    {
        WriteStyleErrorMessage(0, OSMessage_Fatal, byte_string_literal("TOKEN LIMIT EXCEEDED."));
    }
    else
    {
        Result             = Tokenizer->Buffer + Tokenizer->Count++;
        Result->LineInFile = Tokenizer->AtLine;
        Result->Type       = Type;
    }

    return Result;
}

// [Tokenizer]

internal u32
ReadNumber(os_file *File)
{   Assert(IsDigit(PeekFile(File, 0)));

    u32 Number    = 0;
    u8  Character = PeekFile(File, 0);

    while (IsDigit(Character))
    {
        Number = Number * 10 + (Character - '0');
        AdvanceFile(File, 1);
        Character = PeekFile(File, 0);
    }

    return Number;
}


internal b32
ReadString(os_file *File, byte_string *OutString)
{
    OutString->String = PeekFilePointer(File);
    OutString->Size   = 0;

    u8 Character = *OutString->String;

    while (IsAlpha(Character))
    {
        OutString->Size += 1;
        AdvanceFile(File, 1);
        Character = PeekFile(File, 0);
    }

    b32 Result = (OutString->String != 0) && (OutString->Size != 0);
    return Result;
}

internal b32
ReadVector(os_file *File, u32 MinimumSize, u32 MaximumSize, style_token *Result)
{   Assert(PeekFile(File, 0) == '[');

    AdvanceFile(File, 1); // Skips '['

    u32 Count = 0;
    while (Count < MaximumSize)
    {
        SkipWhiteSpaces(File);

        u8 Character = PeekFile(File, 0);
        if (!IsDigit(Character))
        {
            WriteStyleErrorMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Vectors must only contain digits."));
            break;
        }

        Result->Vector.Values[Count++] = (f32)ReadNumber(File);

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
            WriteStyleErrorMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Invalid character found in vector %c"), Character);
            break;
        }
    }

    b32 Valid = (Count >= MinimumSize) && (Count <= MaximumSize);
    return Valid;
}

internal b32
TokenizeStyleFile(os_file *File, style_tokenizer *Tokenizer)
{
    b32 Success = 0;

    while (IsValidFile(File))
    {
        SkipWhiteSpaces(File);

        u8 Char = PeekFile(File, 0);

        if (IsAlpha(Char))
        {
            byte_string Identifier;
            Success = ReadString(File, &Identifier);
            if (!Success)
            {
                WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Could not parse string. EOF?"));
                return Success;
            }

            if (ByteStringMatches(Identifier, byte_string_literal("style")))
            {
                CreateStyleToken(UIStyleToken_Style, Tokenizer);
            }
            else if (ByteStringMatches(Identifier, byte_string_literal("for")))
            {
                CreateStyleToken(UIStyleToken_For, Tokenizer);
            }
            else
            {
                CreateStyleToken(UIStyleToken_Identifier, Tokenizer)->Identifier = Identifier;
            }

            continue;
        }

        if (IsDigit(Char))
        {
            CreateStyleToken(UIStyleToken_Number, Tokenizer)->UInt32 = ReadNumber(File);
            continue;
        }

        if (Char == '\r' || Char == '\n')
        {
            u8 Next = PeekFile(File, 1);
            if (Char == '\r' && Next == '\n')
            {
                Tokenizer->AtLine += 1;
                AdvanceFile(File, 1);
            }

            Tokenizer->AtLine += 1;
            AdvanceFile(File, 1);

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(File, 1) == '=')
            {
                CreateStyleToken(UIStyleToken_Assignment, Tokenizer);
                AdvanceFile(File, 2);
            }
            else
            {
                WriteStyleErrorMessage(Tokenizer->AtLine, OSMessage_Error, byte_string_literal("Stray ':'. Did you mean := ?"));
                return Success;
            }

            continue;
        }

        if (Char == '[')
        {
            read_only u32 MinimumVectorSize = 2;
            read_only u32 MaximumVectorSize = 4;
            Success = ReadVector(File, MinimumVectorSize, MaximumVectorSize, CreateStyleToken(UIStyleToken_Vector, Tokenizer));
            if (!Success)
            {
                WriteStyleErrorMessage(Tokenizer->AtLine, OSMessage_Error, byte_string_literal("Could not read vector. See error(s) above."));
                return Success;
            }

            continue;
        }

        if (Char == '"')
        {
            AdvanceFile(File, 1); // Skip the first '"'

            byte_string String;
            Success = ReadString(File, &String);
            if (!Success)
            {
                WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Could not parse string. EOF?"));
                return Success;
            }

            CreateStyleToken(UIStyleToken_String, Tokenizer)->Identifier = String;

            AdvanceFile(File, 1); // Skip the second '"'

            continue;
        }

        CreateStyleToken((UIStyleToken_Type)Char, Tokenizer);
        AdvanceFile(File, 1);
    }

    return Success;
}

internal void
WriteStyleAttribute(UIStyleAttribute_Flag Attribute, style_token ValueToken, style_parser *Parser)
{
    switch (Attribute)
    {

    default:
    {
        WriteStyleErrorMessage(ValueToken.LineInFile, OSMessage_Error, byte_string_literal("Invalid style supplied."));
    } break;

    case UIStyleAttribute_Size:         Parser->CurrentStyle.Size         = Vec2F32(ValueToken.Vector.X, ValueToken.Vector.Y); break;
    case UIStyleAttribute_Color:        Parser->CurrentStyle.Color        = NormalizedColor(ValueToken.Vector);                break;
    case UIStyleAttribute_Padding:      Parser->CurrentStyle.Padding      = ValueToken.Vector;                                 break;
    case UIStyleAttribute_Spacing:      Parser->CurrentStyle.Spacing      = Vec2F32(ValueToken.Vector.X, ValueToken.Vector.Y); break;
    case UIStyleAttribute_FontSize:     Parser->CurrentStyle.FontSize     = (f32)ValueToken.UInt32;                            break;
    case UIStyleAttribute_FontName:     Parser->CurrentStyle.Font.Name    = ValueToken.Identifier;                             break;
    case UIStyleAttribute_Softness:     Parser->CurrentStyle.Softness     = (f32)ValueToken.UInt32;                            break;
    case UIStyleAttribute_BorderColor:  Parser->CurrentStyle.BorderColor  = NormalizedColor(ValueToken.Vector);                break;
    case UIStyleAttribute_BorderWidth:  Parser->CurrentStyle.BorderWidth  = ValueToken.UInt32;                                 break;
    case UIStyleAttribute_BorderRadius: Parser->CurrentStyle.CornerRadius = ValueToken.Vector;                                 break;
    }
}

internal b32
IsAttributeFormattedCorrectly(UIStyleToken_Type TokenType, UIStyleAttribute_Flag AttributeFlag)
{
    b32 Result = 0;

    switch (TokenType)
    {

    case UIStyleToken_Number:
    {
        Result = (AttributeFlag & UIStyleAttribute_BorderWidth) ||
                 (AttributeFlag & UIStyleAttribute_Softness   ) ||
                 (AttributeFlag & UIStyleAttribute_FontSize   );
    } break;

    case UIStyleToken_Vector:
    {
        Result = (AttributeFlag & UIStyleAttribute_BorderColor ) ||
                 (AttributeFlag & UIStyleAttribute_BorderRadius) ||
                 (AttributeFlag & UIStyleAttribute_Padding     ) ||
                 (AttributeFlag & UIStyleAttribute_Spacing     ) ||
                 (AttributeFlag & UIStyleAttribute_Color       ) ||
                 (AttributeFlag & UIStyleAttribute_Size        );
    } break;

    case UIStyleToken_String:
    {
        Result = (AttributeFlag & UIStyleAttribute_FontName);
    } break;

    default:
    {

    } break;

    }

    return Result;
}

// TODO: Perfect Hash? Or something similar?

internal UIStyleAttribute_Flag
GetStyleAttributeFlagFromIdentifier(byte_string Identifier)
{
    UIStyleAttribute_Flag Result = UIStyleAttribute_None;

    // Valid Types (Clearer as a non-table)
    byte_string Size           = byte_string_literal("size");
    byte_string Color          = byte_string_literal("color");
    byte_string Padding        = byte_string_literal("padding");
    byte_string Spacing        = byte_string_literal("spacing");
    byte_string FontSize       = byte_string_literal("fontsize");
    byte_string FontName       = byte_string_literal("fontname");
    byte_string Softness       = byte_string_literal("softness");
    byte_string BorderWidth    = byte_string_literal("borderwidth");
    byte_string BorderColor    = byte_string_literal("bordercolor");
    byte_string BorderRadius   = byte_string_literal("borderradius");

    if (ByteStringMatches(Identifier, Size))
    {
        Result = UIStyleAttribute_Size;
    }
    else if (ByteStringMatches(Identifier, Color))
    {
        Result = UIStyleAttribute_Color;
    }
    else if (ByteStringMatches(Identifier, Padding))
    {
        Result = UIStyleAttribute_Padding;
    }
    else if (ByteStringMatches(Identifier, Spacing))
    {
        Result = UIStyleAttribute_Spacing;
    }
    else if (ByteStringMatches(Identifier, FontSize))
    {
        Result = UIStyleAttribute_FontSize;
    }
    else if (ByteStringMatches(Identifier, FontName))
    {
        Result = UIStyleAttribute_FontName;
    }
    else if (ByteStringMatches(Identifier, Softness))
    {
        Result = UIStyleAttribute_Softness;
    }
    else if (ByteStringMatches(Identifier, BorderWidth))
    {
        Result = UIStyleAttribute_BorderWidth;
    }
    else if (ByteStringMatches(Identifier, BorderColor))
    {
        Result = UIStyleAttribute_BorderColor;
    }
    else if (ByteStringMatches(Identifier, BorderRadius))
    {
        Result = UIStyleAttribute_BorderRadius;
    }

    return Result;
}

internal void
CacheStyle(ui_style Style, byte_string Name, ui_style_registery *Registery, render_handle RendererHandle)
{
    if (Name.Size <= ThemeNameLength)
    {
        ui_style_name CachedName = UIGetCachedNameFromStyleName(Name, Registery);

        if (!IsValidByteString(CachedName.Value))
        {
            // BUG: Doesn't check if it's an already referenced font.

            // Load deferred data
            if(IsValidByteString(Style.Font.Name))
            {
                Style.Font.Ref = UILoadFont(Style.Font.Name, Style.FontSize, RendererHandle, UIFontCoverage_ASCIIOnly);
            }

            ui_cached_style *Sentinel = UIGetStyleSentinel(Name, Registery);

            ui_cached_style *CachedStyle = Registery->CachedStyles + Registery->Count;
            CachedStyle->Style = Style;
            CachedStyle->Index = Registery->Count;
            CachedStyle->Next  = Sentinel->Next;

            ui_style_name *NewName = Registery->CachedName + CachedStyle->Index;
            NewName->Value.String = PushArena(Registery->Arena, Name.Size + 1, AlignOf(u8));
            NewName->Value.Size   = Name.Size;
            memcpy(NewName->Value.String, Name.String, Name.Size);

            if (Sentinel->Next)
            {
                Sentinel->Next->Next = CachedStyle;
            }
            Sentinel->Next = CachedStyle;

            Registery->Count += 1;
        }
        else
        {
            WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Two different styles cannot have the same name."));
        }
    }
    else
    {
        WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Style name exceeds maximum length of 64"));
    }
}

internal b32
ParseStyleTokens(style_parser *Parser, style_token *Tokens, u32 TokenCount, ui_style_registery *Registery, render_handle RendererHandle)
{
    b32 Success = {0};

    while (Parser->AtToken < TokenCount)
    {
        style_token *Token = Tokens + Parser->AtToken;

        switch (Token->Type)
        {

        case UIStyleToken_Style:
        {
            if (Parser->CurrentType != UIStyle_None)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Unexpected style token. You must close a styling block before commencing a new one"));
                return Success;
            }

            if (Parser->AtToken + 3 >= TokenCount)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return Success;
            }

            style_token PeekTokens[3] = { Token[1], Token[2], Token[3] }; // (String), (For), (Type)
            if (PeekTokens[0].Type != UIStyleToken_String || PeekTokens[1].Type != UIStyleToken_For || PeekTokens[2].Type != UIStyleToken_Identifier)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("A style must be set like this: Theme \"NameOfTheme\" for ComponentType"));
                return Success;
            }

            // Valid types (Function this?)
            byte_string WindowString = byte_string_literal("window");
            byte_string ButtonString = byte_string_literal("button");
            byte_string LabelString  = byte_string_literal("label");

            if (ByteStringMatches(PeekTokens[2].Identifier, WindowString))
            {
                Parser->CurrentType = UIStyle_Window;
            }
            else if (ByteStringMatches(PeekTokens[2].Identifier, ButtonString))
            {
                Parser->CurrentType = UIStyle_Button;
            }
            else if (ByteStringMatches(PeekTokens[2].Identifier, LabelString))
            {
                Parser->CurrentType = UIStyle_Label;
            }
            else
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Invalid component type."));
                return Success;
            }

            Parser->CurrentUserStyleName = PeekTokens[0].Identifier;
            Parser->AtToken             += ArrayCount(PeekTokens) + 1;
        } break;

        case UIStyleToken_Identifier:
        {
            if (Parser->CurrentType == UIStyle_None)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Found identifier outside of a styling block."));
                return Success;
            }

            if (Parser->AtToken + 3 >= TokenCount)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return Success;
            }

            UIStyleAttribute_Flag AttributeFlag = GetStyleAttributeFlagFromIdentifier(Token->Identifier);
            if (AttributeFlag == UIStyleAttribute_None)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Invalid attribute name."));
                return Success;
            }

            if (!(AttributeFlag & StyleTypeValidAttributesTable[Parser->CurrentType]))
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Invalid attribute supplied to a theme. Invalid: %s"), UIStyleAttributeToString(AttributeFlag));
                return Success;
            }

            style_token PeekTokens[3] = {Token[1], Token[2], Token[3]}; // (Assignment), (Value), (;)
            if (PeekTokens[0].Type != UIStyleToken_Assignment || !(PeekTokens[1].Type & StyleTokenValueMask) ||PeekTokens[2].Type != ';')
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Invalid formatting. Should be: {Attribute} := {Value}{;} ."));
                return Success;
            }

            if (!IsAttributeFormattedCorrectly(PeekTokens[1].Type, AttributeFlag))
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Invalid formatting for attribute."));
                return Success;
            }

            WriteStyleAttribute(AttributeFlag, PeekTokens[1], Parser);

            Parser->AtToken += ArrayCount(PeekTokens) + 1;
        } break;

        DisableWarning(4063)
        case '{':
        {
            Parser->AtToken++;

            if (Parser->CurrentType == UIStyle_None)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("You must end a block with '}' before beginning a new one with '{'."));
                return Success;
            }
        } break;

        DisableWarning(4063)
        case '}':
        {
            Parser->AtToken++;

            if (Parser->CurrentType == UIStyle_None)
            {
                WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("You must end a block with '}' before beginning a new one with '{'."));
                return Success;
            }

            CacheStyle(Parser->CurrentStyle, Parser->CurrentUserStyleName, Registery, RendererHandle);

            Parser->CurrentType          = UIStyle_None;
            Parser->CurrentStyle         = (ui_style){ 0 };
            Parser->CurrentUserStyleName = ByteString(0, 0);
        } break;

        default:
        {
            WriteStyleErrorMessage(Token->LineInFile, OSMessage_Error, byte_string_literal("Found invalid token in file: %c"), (char)Token->Type);
            return Success;
        } break;

        }
    }

    Success = 1;
    return Success;
}

// [Success Handling]

internal read_only char *
UIStyleAttributeToString(UIStyleAttribute_Flag Flag)
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
    case UIStyleAttribute_Softness:     return "BorderSoftness";
    case UIStyleAttribute_BorderColor:  return "BorderColor";
    case UIStyleAttribute_BorderWidth:  return "BorderWidth";
    case UIStyleAttribute_BorderRadius: return "BorderRadius";
    default:                            return "Unknown";

    }
}

internal void
WriteStyleErrorMessage(u32 LineInFile, OSMessage_Severity Severity, byte_string Format, ...)
{
    va_list Args;
    __crt_va_start(Args, Format);
    __va_start(&Args, Format);

    u8  Buffer[Kilobyte(2)];

    byte_string ErrorString = ByteString(Buffer, 0);
    ErrorString.Size  = snprintf ((char *) Buffer, sizeof(Buffer), "[Parsing Error: Line=%u] -> ", LineInFile);
    ErrorString.Size += vsnprintf((char *)(Buffer + ErrorString.Size), sizeof(Buffer), (char *)Format.String, Args);

    OSLogMessage(ErrorString, Severity);

    __crt_va_end(Args);
}