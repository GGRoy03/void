// [Globals]

// [Public API Implementation]

internal void
LoadThemeFiles(byte_string *Files, u32 FileCount, ui_style_registery *Registery)
{
    if (!Files)
    {
        WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("'Files' must be a valid pointer."));

        return;
    }

    style_parser Parser = {0};

    {
        Parser.TokenCapacity = 10'000;

        u64 ReserveSize = AlignPow2(Parser.TokenCapacity * sizeof(style_token), OSGetSystemInfo()->PageSize);

        style_token *Tokens  = OSReserveMemory(ReserveSize);
        b32          Success = OSCommitMemory(Tokens, ReserveSize);

        if (Success)
        {
            Parser.TokenBuffer = Tokens;
        }
        else
        {
            WriteStyleErrorMessage(0, OSMessage_Fatal, byte_string_literal("FAILED TO ALLOCATE TOKEN BUFFER"));
        }
    }

    {
        memory_arena_params Params = {0};
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = ArenaDefaultCommitSize;
        Params.ReserveSize       = Megabyte(10);

        Parser.Arena = AllocateArena(Params);
    }

    // NOTE: Basically an initialization routine... Probably move this?
    if (!Registery->Arena)
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize = ArenaDefaultCommitSize;
        Params.ReserveSize = ArenaDefaultReserveSize;

        u32 SentinelCount = ThemeNameLength;
        u32 CachedStyleCount = ThemeMaxCount - ThemeNameLength;

        Registery->Arena        = AllocateArena(Params);
        Registery->Sentinels    = PushArena(Registery->Arena, SentinelCount * sizeof(ui_cached_style)   , AlignOf(ui_cached_style));
        Registery->CachedStyles = PushArena(Registery->Arena, CachedStyleCount * sizeof(ui_cached_style), AlignOf(ui_cached_style));
        Registery->CachedName   = PushArena(Registery->Arena, CachedStyleCount * sizeof(ui_style_name)  , AlignOf(ui_style_name));
        Registery->Capacity     = CachedStyleCount;
        Registery->Count        = 0;
    }

    for (u32 FileIdx = 0; FileIdx < FileCount; FileIdx++)
    {
        Parser.AtLine     = 1;
        Parser.TokenCount = 0;

        byte_string FileName     = Files[FileIdx];
        os_handle   OSFileHandle = OSFindFile(FileName);
        os_file     OSFile       = OSReadFile(OSFileHandle, Parser.Arena);
  
        if (!OSFile.FullyRead)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("File with path: %s does not exist on disk."), FileName);
            continue;
        }

        b32 TokenSuccess = TokenizeStyleFile(&OSFile, &Parser);
        if (!TokenSuccess)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("Failed to tokenize file. See error(s) above."));
            continue;
        }

        b32 ParseSuccess = ParseStyleTokens(&Parser, Registery);
        if (!ParseSuccess)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("Failed to parse file. See error(s) above."));
            continue;
        }

        PopArenaTo(Parser.Arena, 0);
    }

    ReleaseArena(Parser.Arena);
    OSRelease(Parser.TokenBuffer);
}

// [Internal Implementation]

internal style_token *
CreateStyleToken(UIStyleToken_Type Type, style_parser *Parser)
{
    style_token *Result = 0;

    if (Parser->TokenCount == Parser->TokenCapacity)
    {
        WriteStyleErrorMessage(0, OSMessage_Fatal, byte_string_literal("TOKEN LIMIT EXCEEDED."));
    }
    else
    {
        Result             = Parser->TokenBuffer + Parser->TokenCount++;
        Result->LineInFile = Parser->AtLine;
        Result->Type       = Type;
    }

    return Result;
}

internal b32
TokenizeStyleFile(os_file *File, style_parser *Parser)
{
    b32 Success = 0;

    while(OSIsValidFile(File))
    {
        OSIgnoreWhiteSpaces(File);

        u8  Char    = OSGetFileChar(File);
        u64 StartAt = File->At;

        if (IsAlpha(Char))
        {
            while (OSIsValidFile(File) && (IsAlpha(Char) || IsNumber(Char)))
            {
                Char = OSGetNextFileChar(File);
            }

            if(!OSIsValidFile(File))
            {
                WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return Success;
            }

            byte_string StyleString = byte_string_literal("style");
            byte_string ForString   = byte_string_literal("for");

            u8         *IdentifierStart  = File->Content.String + StartAt;
            u64         IdentifierLength = File->At - StartAt;
            byte_string IdString         = ByteString(IdentifierStart, IdentifierLength);

            if (ByteStringMatches(IdString, StyleString))
            {
                CreateStyleToken(UIStyleToken_Style, Parser);
            }
            else if (ByteStringMatches(IdString, ForString))
            {
                CreateStyleToken(UIStyleToken_For, Parser);
            }
            else
            {
                style_token *Token = CreateStyleToken(UIStyleToken_Identifier, Parser);
                Token->Identifier = IdString;
            }

        }
        else if (IsNumber(Char))
        {
            style_token *Token = CreateStyleToken(UIStyleToken_Number, Parser);

            Char = OSGetFileChar(File);
            while (OSIsValidFile(File) && IsNumber(Char))
            {
                Token->UInt32 = (Token->UInt32 * 10) + (Char - '0');

                Char = OSGetNextFileChar(File);
            }
        }
        else if (Char == '\r' || Char == '\n')
        {
            u8 NextChar = OSGetNextFileChar(File);
            Char = OSGetFileChar(File);
            if (Char == '\r' && !NextChar == '\n')
            {
                --File->At; // We peeked when we shouldn't have.
            }

            Parser->AtLine += 1;
            File->At       += 1;
        }
        else if (Char == ':')
        {
            ++File->At;

            if (OSIsValidFile(File) && OSGetFileChar(File) == '=')
            {
                CreateStyleToken(UIStyleToken_Assignment, Parser);
                ++File->At;
            }
            else
            {
                WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Stray ':'. Did you mean := ?"));
                return Success;
            }
        }
        else if (Char == '[')
        {
            ++File->At;

            style_token *Token = CreateStyleToken(UIStyleToken_Vector, Parser);

            u32 DigitCount   = 0;
            u32 MaximumDigit = 4;
            while (DigitCount < MaximumDigit && OSIsValidFile(File))
            {
                OSIgnoreWhiteSpaces(File);

                u32 Idx = DigitCount;
                for (Char = OSGetFileChar(File); OSIsValidFile(File) && IsNumber(Char); Char = OSGetNextFileChar(File))
                {
                    Token->Vector.Values[Idx] = (Token->Vector.Values[Idx] * 10) + (Char - '0');
                }

                if (!OSIsValidFile(File))
                {
                    WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                    return Success;
                }

                OSIgnoreWhiteSpaces(File);

                if (Char == ',')
                {
                    ++File->At;
                    ++DigitCount;
                }
                else if (Char == ']')
                {
                    break;
                }
                else
                {
                    WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Found invalid character in vector: '%c' ."), Char);
                    return Success;
                }
            }

            if (Char != ']')
            {
                WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Vector exceeds maximum size (4)."));
                return Success;
            }

            ++File->At;
        }
        else if (Char == '"')
        {
            style_token *Token = CreateStyleToken(UIStyleToken_String, Parser);
            Token->Identifier = ByteString(File->Content.String + File->At + 1, 0);

            while (OSIsValidFile(File) && OSGetNextFileChar(File) != '\"') 
            {
                ++Token->Identifier.Size;
            };

            if (!OSIsValidFile(File))
            {
                WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return Success;
            }

            ++File->At;
        }
        else if (Char == '@')
        {
            byte_string EffectString = ByteString(File->Content.String + File->At + 1, 0);
            while (OSIsValidFile(File) && IsAlpha(OSGetNextFileChar(File)))
            {
                EffectString.Size += 1;
            }

            if (EffectString.Size)
            {
                byte_string HoverString = byte_string_literal("hover");
                byte_string ClickString = byte_string_literal("click");

                if (ByteStringMatches(EffectString, HoverString))
                {
                    CreateStyleToken(UIStyleToken_HoverEffect, Parser);
                    File->At += HoverString.Size - 1;
                }
                else if (ByteStringMatches(EffectString, ClickString))
                {
                    CreateStyleToken(UIStyleToken_ClickEffect, Parser);
                    File->At += ClickString.Size - 1;
                }
                else
                {
                    WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("'@' Special Marker must only be used when specifying effects for a style. Example: @Hover OR @Click"));
                    return Success;
                }
            }
            else
            {
                WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("'@' Special Marker must only be used when specifying effects for a style. Example: @Hover OR @Click"));
                return Success;
            }
        }
        else
        {
            CreateStyleToken((UIStyleToken_Type)Char, Parser);
            File->At += 1;
        }
    }

    Success = 1;
    return Success;
}

internal void
WriteStyleAttribute(UIStyleAttribute_Flag Attribute, style_token ValueToken, style_parser *Parser)
{
    switch (Attribute)
    {

    case UIStyleAttribute_None:
    {
        WriteStyleErrorMessage(Parser->AtLine, OSMessage_Error, byte_string_literal("Invalid style supplied"));
    } break;

    case UIStyleAttribute_Size:        Parser->CurrentStyle.Size        = Vec2F32(ValueToken.Vector.X, ValueToken.Vector.Y);  break;
    case UIStyleAttribute_Color:       Parser->CurrentStyle.Color       = NormalizedColor(ValueToken.Vector);                 break;
    case UIStyleAttribute_Padding:     Parser->CurrentStyle.Padding     = ValueToken.Vector;                                  break;
    case UIStyleAttribute_Spacing:     Parser->CurrentStyle.Spacing     = Vec2F32(ValueToken.Vector.X, ValueToken.Vector.Y);  break;
    case UIStyleAttribute_BorderColor: Parser->CurrentStyle.BorderColor = NormalizedColor(ValueToken.Vector);                 break;
    case UIStyleAttribute_BorderWidth: Parser->CurrentStyle.BorderWidth = ValueToken.UInt32;                                  break;

    }
}

internal UIStyleAttribute_Flag
GetStyleAttributeFlagFromIdentifier(byte_string Identifier)
{
    UIStyleAttribute_Flag AttributeFlag = UIStyleAttribute_None;
    
    // Valid Types
    byte_string Size        = byte_string_literal("size");
    byte_string Color       = byte_string_literal("color");
    byte_string Padding     = byte_string_literal("padding");
    byte_string Spacing     = byte_string_literal("spacing");
    byte_string BorderWidth = byte_string_literal("borderwidth");
    byte_string BorderColor = byte_string_literal("bordercolor");
    
    if (ByteStringMatches(Identifier, Size))
    {
        AttributeFlag = UIStyleAttribute_Size;
    }
    else if (ByteStringMatches(Identifier, Color))
    {
        AttributeFlag = UIStyleAttribute_Color;
    }
    else if (ByteStringMatches(Identifier, Padding))
    {
        AttributeFlag = UIStyleAttribute_Padding;
    }
    else if (ByteStringMatches(Identifier, Spacing))
    {
        AttributeFlag = UIStyleAttribute_Spacing;
    }
    else if (ByteStringMatches(Identifier, BorderWidth))
    {
        AttributeFlag = UIStyleAttribute_BorderWidth;
    }
    else if (ByteStringMatches(Identifier, BorderColor))
    {
        AttributeFlag = UIStyleAttribute_BorderColor;
    }
    
    return AttributeFlag;
}

internal void
CacheStyle(ui_style Style, byte_string Name, ui_style_registery *Registery)
{
    if (Name.Size <= ThemeNameLength)
    {
        ui_style_name CachedName = UIGetCachedNameFromStyleName(Name, Registery);

        if (!CachedName.Value.String || !CachedName.Value.Size)
        {
            ui_cached_style *Sentinel = UIGetStyleSentinel(Name, Registery);

            ui_cached_style *CachedStyle = Registery->CachedStyles + Registery->Count;
            CachedStyle->Style = Style;
            CachedStyle->Index = Registery->Count;
            CachedStyle->Next  = Sentinel->Next;

            ui_style_name *NewName = Registery->CachedName + CachedStyle->Index;
            NewName->Value.String = PushArena(Registery->Arena, Name.Size + 1, AlignOf(u8));
            NewName->Value.Size   = Name.Size;
            memcpy(NewName->Value.String, Name.String, Name.Size);

            Sentinel->Next   = CachedStyle;
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
ParseStyleTokens(style_parser *Parser, ui_style_registery *Registery)
{
    b32 Success = {0};

    style_token *Tokens     = Parser->TokenBuffer;
    u32          TokenCount = Parser->TokenCount;

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

            // Valid types.
            byte_string WindowString = byte_string_literal("window");
            byte_string ButtonString = byte_string_literal("button");

            if (ByteStringMatches(PeekTokens[2].Identifier, WindowString))
            {
                Parser->CurrentType = UIStyle_Window;
            }
            else if (ByteStringMatches(PeekTokens[2].Identifier, ButtonString))
            {
                Parser->CurrentType = UIStyle_Button;
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

            WriteStyleAttribute(AttributeFlag, PeekTokens[1], Parser);

            Parser->AtToken += ArrayCount(PeekTokens) + 1;
        } break;

        case UIStyleToken_HoverEffect:
        case UIStyleToken_ClickEffect:
        {
            // NOTE: For now, these are no-ops, because I am unsure how I want to store them.
            ++Parser->AtToken;
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

            CacheStyle(Parser->CurrentStyle, Parser->CurrentUserStyleName, Registery);

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

    case UIStyleAttribute_None:        return "None";
    case UIStyleAttribute_Size:        return "Size";
    case UIStyleAttribute_Color:       return "Color";
    case UIStyleAttribute_Padding:     return "Padding";
    case UIStyleAttribute_Spacing:     return "Spacing";
    case UIStyleAttribute_BorderColor: return "BorderColor";
    case UIStyleAttribute_BorderWidth: return "BorderWidth";
    default:                           return "Unknown";

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
    ErrorString.Size  = snprintf ((char *) Buffer, sizeof(Buffer), "[Parsing Error: Line=%u] ->", LineInFile);
    ErrorString.Size += vsnprintf((char *)(Buffer + ErrorString.Size), sizeof(Buffer), (char *)Format.String, Args);

    OSLogMessage(ErrorString, Severity);

    __crt_va_end(Args);
}