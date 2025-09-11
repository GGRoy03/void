// [Macros]

// TODO: Make these functions?

#define CimTheme_MaxVectorSize 4
#define CimTheme_ArrayToVec2(Array)  {Array[0], Array[1]}
#define CimTheme_ArrayToVec4(Array)  {Array[0], Array[1], Array[2], Array[3]}
#define CimTheme_ArrayToColor(Array) {Array[0] * 1/255, Array[1] * 1/255, Array[2] * 1/255, Array[3] * 1/255}

// [Globals]

// TODO: Remove this global.
static style_table UITheme_Table;

// [Public API Implementation]

static ui_style *
GetUITheme(read_only char *ThemeName, style_id *ComponentId)
{
    if (ComponentId && ComponentId->Value >= ThemeNameLength)
    {
        style_info *ThemeInfo = &UITheme_Table.Themes[ComponentId->Value];
        return &ThemeInfo->Theme;
    }
    else
    {
        Assert(ThemeName);

        u8 *NameToFind = (u8 *)ThemeName;
        u32 NameLength = (u32)strlen(ThemeName);

        style_info *Sentinel = &UITheme_Table.Themes[NameLength];
        style_id    ReadPointer = Sentinel->NextWithSameLength;

        while (ReadPointer.Value)
        {
            style_info *ThemeInfo = &UITheme_Table.Themes[ReadPointer.Value];

            if (strcmp((read_only char *)NameToFind, (read_only char *)ThemeInfo->Name) == 0)
            {
                if (ComponentId)
                {
                    ComponentId->Value = ReadPointer.Value;
                }

                return &ThemeInfo->Theme;
            }

            ReadPointer = ThemeInfo->NextWithSameLength;
        }

        return NULL;
    }

}

static ui_window_style
GetWindowTheme(read_only char *ThemeName, style_id ThemeId)
{
    ui_window_style Result = {0};
    ui_style       *Theme  = GetUITheme(ThemeName, &ThemeId);
    Assert(Theme && Theme->Type == UITheme_Window);

    if (Theme)
    {
        Result.ThemeId     = ThemeId;
        Result.BorderColor = Theme->Window.BorderColor;
        Result.BorderWidth = Theme->Window.BorderWidth;
        Result.Color       = Theme->Window.Color;
        Result.Padding     = Theme->Window.Padding;
        Result.Size        = Theme->Window.Size;
        Result.Spacing     = Theme->Window.Spacing;
    }

    return Result;
}

static ui_button_style
GetButtonTheme(read_only char *ThemeName, style_id ThemeId)
{
    ui_button_style Result = {0};
    ui_style       *Theme  = GetUITheme(ThemeName, &ThemeId);
    Assert(Theme->Type == UITheme_Button);

    if (Theme)
    {
        Result.ThemeId     = ThemeId;
        Result.BorderColor = Theme->Button.BorderColor;
        Result.BorderWidth = Theme->Button.BorderWidth;
        Result.Color       = Theme->Button.Color;
        Result.Size        = Theme->Button.Size;
    }

    return Result;
}

internal void
LoadThemeFiles(byte_string *Files, u32 FileCount)
{
    if (!Files)
    {
        style_parsing_error Error = {0};
        Error.Type          = ThemeParsingError_Argument;
        Error.ArgumentIndex = 0;
        WriteErrorMessage(&Error, "'Files' must be a valid pointer.");
        HandleThemeError(&Error, NULL, "InitializeUIThemes");

        return;
    }

    memory_arena_params Params = {0};
    Params.AllocatedFromFile = __FILE__;
    Params.AllocatedFromLine = __LINE__;
    Params.CommitSize        = ArenaDefaultCommitSize;
    Params.ReserveSize       = Megabyte(10);

    style_parser Parser = {0};
    Parser.Arena = AllocateArena(Params);

    for (u32 FileIdx = 0; FileIdx < FileCount; FileIdx++)
    {
        byte_string FileName = Files[FileIdx];

        os_handle OSFileHandle = OSFindFile(FileName);
        os_file   OSFile       = OSReadFile(OSFileHandle, Parser.Arena);
  
        if (!OSFile.FullyRead)
        {
            style_parsing_error Error = {0};
            Error.Type          = ThemeParsingError_Argument;
            Error.ArgumentIndex = 0;
            WriteErrorMessage(&Error, "File with path: %s does not exist on disk.", FileName);
            HandleThemeError(&Error, 0, 0);

            continue;
        }

        style_parsing_error TokenError = GetNextTokenBuffer(&OSFile, &Parser);
        if (TokenError.Type != ThemeParsingError_None)
        {
            HandleThemeError(&TokenError, FileName.String, "InitializeUIThemes");
            continue;
        }

        style_parsing_error ParseError = ValidateAndStoreThemes(&Parser);
        if (ParseError.Type != ThemeParsingError_None)
        {
            HandleThemeError(&ParseError, FileName.String, "InitializeUIThemes");
            continue;
        }

        // BUG: Need to reset some state.
    }

    // BUG: Probably need to release some data.
}

// [Internal Implementation]

static style_token *
CreateStyleToken(UIStyleToken_Type Type,style_parser *Parser)
{
    style_token *Result = PushArena(Parser->Arena, sizeof(style_token), AlignOf(style_token));
    Result->LineInFile = Parser->AtLine;
    Result->Type       = Type;

    ++Parser->TokenCount;

    return Result;
}

static style_parsing_error
GetNextTokenBuffer(os_file *File, style_parser *Parser)
{
    style_parsing_error Error = {0};

    Parser->TokenBuffer = PushArray(Parser->Arena, style_token, 128);
    Parser->AtLine      = 1;

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
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Unexpected end of file.");

                return Error;
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
            Char = OSGetFileChar(File);
            if (Char == '\r' && !OSGetNextFileChar(File) == '\n')
            {
                --File->At;
            }
        }
        else if (Char == ':')
        {
            Char = OSGetNextFileChar(File);
            if (OSIsValidFile(File) && Char == '=')
            {
                CreateStyleToken(UIStyleToken_Assignment, Parser);
            }
            else
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Stray ':'. Did you mean := ?");

                return Error;
            }
        }
        else if (Char == '[')
        {
            style_token *Token = CreateStyleToken(UIStyleToken_Vector, Parser);

            u32 DigitCount   = 0;
            u32 MaximumDigit = 0;
            while (DigitCount < MaximumDigit && OSIsValidFile(File))
            {
                OSIgnoreWhiteSpaces(File);

                u32 Idx = DigitCount;
                while (OSIsValidFile(File) && IsNumber(OSGetNextFileChar(File)))
                {
                    Token->Vector.Values[Idx] = (Token->Vector.Values[Idx] * 10) + (Char - '0');
                }

                if (!OSIsValidFile(File))
                {
                    Error.Type       = ThemeParsingError_Syntax;
                    Error.LineInFile = Parser->AtLine;
                    WriteErrorMessage(&Error, "Unexpected end of file.");

                    return Error;
                }

                OSIgnoreWhiteSpaces(File);

                if (Char == ',')
                {
                    File->At++;
                    DigitCount++;
                }
                else if (Char == ']')
                {
                    break;
                }
                else
                {
                    Error.Type       = ThemeParsingError_Syntax;
                    Error.LineInFile = Parser->AtLine;
                    WriteErrorMessage(&Error, "Found invalid character in vector: %c.", Char);

                    return Error;
                }
            }

            if (Char != ']')
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Vector exceeds maximum size (4).");

                return Error;
            }
        }
        else if (Char == '"')
        {
            style_token *Token = CreateStyleToken(UIStyleToken_String, Parser);
            Token->Identifier = ByteString(File->Content.String + File->At, 0);

            while (OSIsValidFile(File) && OSGetNextFileChar(File) != '"') 
            {
                ++Token->Identifier.Size;
            };

            if (!OSIsValidFile(File))
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Unexpected end of file.");

                return Error;
            }
        }
        else if (Char == '@')
        {
            ++File->At;

            byte_string HoverString  = byte_string_literal("hover");
            byte_string ClickString  = byte_string_literal("click");
            byte_string EffectString = ByteString(File->Content.String + File->At, 0); // BUG: Size 0 will always fail.

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
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "'@' Special Marker must only be used when specifying effects for a style. Example: @Hover OR @Click");

                return Error;
            }
        }
        else
        {
            // NOTE: Now do we treat this as an error?
        }
    }

    return Error;
}

// TODO: This code needs a rework.

static style_parsing_error
StoreAttributeInTheme(UIThemeAttribute_Flag Attribute, style_token *Value, style_parser *Parser)
{
    style_parsing_error Error = {0};
    Error.Type = ThemeParsingError_None;

    ui_style *ThemeToFill = Parser->ActiveEffectTheme ? Parser->ActiveEffectTheme : Parser->ActiveTheme;

    // TODO: Double check this.
    switch (ThemeToFill->Type)
    {

    case UITheme_None:
    {
        Error.Type = ThemeParsingError_Internal;
        WriteErrorMessage(&Error, "Invalid style supplied");

        return Error;
    }

    case UITheme_Window:
    {
        ui_window_style *Theme = &ThemeToFill->Window;

        switch (Attribute)
        {

        case UIThemeAttribute_Size:        Theme->Size        = Vec2F32(Value->Vector.X, Value->Vector.Y); break;
        case UIThemeAttribute_Color:       Theme->Color       = NormalizeColors(Value->Vector);            break;
        case UIThemeAttribute_Padding:     Theme->Padding     = Value->Vector;                             break;
        case UIThemeAttribute_Spacing:     Theme->Spacing     = Vec2F32(Value->Vector.X, Value->Vector.Y); break;
        case UIThemeAttribute_BorderColor: Theme->BorderColor = NormalizeColors(Value->Vector);            break;
        case UIThemeAttribute_BorderWidth: Theme->BorderWidth = Value->UInt32;                             break;

        default:
        {
            Error.Type       = ThemeParsingError_Syntax;
            Error.LineInFile = Parser->AtLine;           // WARN: Incorrect?
            WriteErrorMessage(&Error, "Invalid attribute supplied to a Window Theme. Invalid: %s", UIThemeAttributeToString(Attribute));
        } break;

        }
    } break;

    case UITheme_Button:
    {
        ui_button_style *Theme = &ThemeToFill->Button;

        switch (Attribute)
        {

        case UIThemeAttribute_Size:        Theme->Size        = Vec2F32(Value->Vector.X, Value->Vector.Y); break;
        case UIThemeAttribute_Color:       Theme->Color       = NormalizeColors(Value->Vector);              break;
        case UIThemeAttribute_BorderColor: Theme->BorderColor = NormalizeColors(Value->Vector);              break;
        case UIThemeAttribute_BorderWidth: Theme->BorderWidth = Value->UInt32;                               break;

        default:
        {
            Error.Type       = ThemeParsingError_Syntax;
            Error.LineInFile = Parser->AtLine;           // WARN: Incorrect?
            WriteErrorMessage(&Error, "Invalid attribute supplied to Button Theme. Invalid: %s", UIThemeAttributeToString(Attribute));
        } break;

        }
    } break;

    }

    return Error;
}

static style_parsing_error
ValidateAndStoreThemes(style_parser *Parser)
{
    style_parsing_error Error = {0};

    style_token *Tokens    = (style_token *)Parser->TokenBuffer;;
    u32         TokenCount = Parser->TokenCount;

    while (Parser->AtToken< TokenCount)
    {
        style_token *Token = Tokens + Parser->AtToken;

        switch (Token->Type)
        {

        case UIStyleToken_Style:
        {
            if (Parser->ActiveTheme && Parser->ActiveTheme->Type != UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Forgot to close previous ui_style with } ?");

                return Error;
            }

            if (Parser->AtToken + 3 >= TokenCount)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Unexpected end of file.");

                return Error;
            }

            if (Token[1].Type != UIStyleToken_String || Token[2].Type != UIStyleToken_For || Token[3].Type != UIStyleToken_Identifier)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token->LineInFile;
                WriteErrorMessage(&Error, "A style must be set like this: Theme \"NameOfTheme\" for ComponentType");

                return Error;
            }

            style_token *ThemeNameToken     = Token + 1;
            style_token *ComponentTypeToken = Token + 3;

            typedef struct known_type { read_only char *Name; size_t Length; UITheme_Type Type; } known_type;
            known_type KnownTypes[] =
            {
                {"window", sizeof("window") - 1, UITheme_Window},
                {"button", sizeof("button") - 1, UITheme_Button},
            };

            for (u32 KnownIdx = 0; KnownIdx < ArrayCount(KnownTypes); ++KnownIdx)
            {
                known_type Known = KnownTypes[KnownIdx];

                if (Known.Length != ComponentTypeToken->Identifier.Size)
                {
                    continue;
                }

                u32 CharIdx;
                for (CharIdx = 0; CharIdx < ComponentTypeToken->Identifier.Size; CharIdx++)
                {
                    u8 LowerChar = ToLowerChar(ComponentTypeToken->Identifier.String[CharIdx]);
                    if (LowerChar != Known.Name[CharIdx])
                    {
                        break;
                    }
                }

                if (CharIdx == Known.Length)
                {
                    // TODO: Store the theme.
                    break;
                }
            }

            if (!Parser->ActiveTheme || Parser->ActiveTheme->Type == UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = ComponentTypeToken->LineInFile;

                // BUG: Can easily overflow. And not really clear.
                Assert(ThemeNameToken->Identifier.Size < 64);
                WriteErrorMessage(&Error, "Invalid component type for : %s", ThemeNameToken->Identifier.String);

                return Error;
            }

            Parser->ActiveThemeNameToken = ThemeNameToken;
            Parser->AtToken             += 4;
        } break;

        case UIStyleToken_Identifier:
        {
            if (Parser->ActiveTheme->Type == UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token->LineInFile;

                // BUG: Can easily overflow. And not really clear.
                WriteErrorMessage(&Error, "Found identifier: %s | Outside of a ui_style block.", Token->Identifier.String);

                return Error;
            }

            if (Parser->AtToken+ 2 >= TokenCount)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token->LineInFile;
                WriteErrorMessage(&Error, "End of file reached earlier than expected.");

                return Error;
            }

            if (Token[1].Type != UIStyleToken_Assignment)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token[1].LineInFile;
                WriteErrorMessage(&Error, "Invalid formatting. Should be -> Attribute := Value.");

                return Error;
            }

            style_token *AttributeToken = Token;
            style_token *ValueToken     = Token + 2;

            b32 HasNegativeValue = 0;
            if (Token[2].Type == '-')
            {
                HasNegativeValue = 1;

                if (Parser->AtToken+ 3 >= TokenCount)
                {
                    Error.Type       = ThemeParsingError_Syntax;
                    Error.LineInFile = Token[2].LineInFile;
                    WriteErrorMessage(&Error, "End of file reached with invalid formatting.");

                    return Error;
                }

                if (Token[4].Type != ';')
                {
                    Error.Type       = ThemeParsingError_Syntax;
                    Error.LineInFile = Token[4].LineInFile;
                    WriteErrorMessage(&Error, "Missing ; after setting the attribute value.");

                    return Error;
                }
            }
            else if (Token[3].Type != ';')
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token[3].LineInFile;
                WriteErrorMessage(&Error, "Missing ; after setting the attribute value.");

                return Error;
            }

            // NOTE: Could use binary search if number of attributes becomes large.
            typedef struct known_type { read_only char *Name; size_t Length; UIThemeAttribute_Flag TypeFlag; bit_field ValidValueTokenMask; } known_type;
            known_type KnownTypes[] =
            {
                {"size"       , sizeof("size")        - 1, UIThemeAttribute_Size       , UIStyleToken_Vector},
                {"color"      , sizeof("color")       - 1, UIThemeAttribute_Color      , UIStyleToken_Vector},
                {"padding"    , sizeof("padding")     - 1, UIThemeAttribute_Padding    , UIStyleToken_Vector},
                {"spacing"    , sizeof("spacing")     - 1, UIThemeAttribute_Spacing    , UIStyleToken_Vector},
                {"borderwidth", sizeof("borderwidth") - 1, UIThemeAttribute_BorderWidth, UIStyleToken_Number},
                {"bordercolor", sizeof("bordercolor") - 1, UIThemeAttribute_BorderColor, UIStyleToken_Vector},
            };

            UIThemeAttribute_Flag Attribute = UIThemeAttribute_None;
            for (u32 KnownIdx = 0; KnownIdx < ArrayCount(KnownTypes); ++KnownIdx)
            {
                known_type Known = KnownTypes[KnownIdx];

                if (Known.Length != AttributeToken->Identifier.Size)
                {
                    continue;
                }

                u32 CharIdx;
                for (CharIdx = 0; CharIdx < AttributeToken->Identifier.Size; CharIdx++)
                {
                    u8 LowerChar = ToLowerChar(AttributeToken->Identifier.String[CharIdx]);
                    if (LowerChar != Known.Name[CharIdx])
                    {
                        break;
                    }
                }

                if (CharIdx == Known.Length)
                {
                    if (!(ValueToken->Type & Known.ValidValueTokenMask))
                    {
                        Error.Type       = ThemeParsingError_Syntax;
                        Error.LineInFile = ValueToken->LineInFile;
                        WriteErrorMessage(&Error, "Attribute of type: %s does not accept this format for values.", Known.Name);

                        return Error;
                    }

                    Attribute = Known.TypeFlag;
                    break;
                }
            }

            if (Attribute == UIThemeAttribute_None)
            {
                Error.Type = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;

                // BUG: Can easily overflow. And not really clear.
                WriteErrorMessage(&Error, "Invalid attribute name: %s", AttributeToken->Identifier.String);

                return Error;
            }

            Error = StoreAttributeInTheme(Attribute, ValueToken, Parser);
            if (Error.Type != ThemeParsingError_None)
            {
                return Error;
            }

            Parser->AtToken += HasNegativeValue ? 5 : 4;
        } break;

        case UIStyleToken_HoverEffect:
        case UIStyleToken_ClickEffect:
        {
            // TODO: Do something with these.
            ++Parser->AtToken;
        } break;

        DisableWarning(4063)
        case '{':
        {
            Parser->AtToken++;

            if (Parser->ActiveTheme->Type == UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token->LineInFile;
                WriteErrorMessage(&Error, "You must end a block with '}' before beginning a new one with '{'.");

                return Error;
            }
        } break;

        DisableWarning(4063)
        case '}':
        {
            Parser->AtToken++;

            if (Parser->ActiveTheme->Type == UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Token->LineInFile;
                WriteErrorMessage(&Error, "You must end a block with '}' before beginning a new one with '{'.");

                return Error;
            }

            memset(&Parser->ActiveTheme, 0, sizeof(Parser->ActiveTheme));
            Parser->ActiveThemeNameToken = NULL;
            Parser->ActiveTheme          = NULL;
            Parser->ActiveEffectTheme    = NULL;

        } break;

        default:
        {
            // WARN: The error message could be wrong? Depends if we correctly handle
            // earlier cases I guess?

            Error.Type       = ThemeParsingError_Syntax;
            Error.LineInFile = Token->LineInFile;
            WriteErrorMessage(&Error, "Found invalid token in file: %c", (char)Token->Type);

            return Error;
        } break;

        }
    }

    return Error;
}

// [Error Handling]

static read_only char *
UIThemeAttributeToString(UIThemeAttribute_Flag Flag)
{
    switch (Flag)
    {

    case UIThemeAttribute_None:        return "None";
    case UIThemeAttribute_Size:        return "Size";
    case UIThemeAttribute_Color:       return "Color";
    case UIThemeAttribute_Padding:     return "Padding";
    case UIThemeAttribute_Spacing:     return "Spacing";
    case UIThemeAttribute_LayoutOrder: return "LayoutOrder";
    case UIThemeAttribute_BorderColor: return "BorderColor";
    case UIThemeAttribute_BorderWidth: return "BorderWidth";
    default:                           return "Unknown";

    }
}

static void
WriteErrorMessage(style_parsing_error *Error, read_only char *Format, ...)
{
    va_list Args;
    __crt_va_start(Args, Format);
    __va_start(&Args, Format);

    vsnprintf(Error->Message, ThemeErrorMessageLength, Format, Args);

    __crt_va_end(Args);
}

static void
HandleThemeError(style_parsing_error *Error, u8 *FileName, read_only char *PublicAPIFunctionName)
{
    UNUSED(Error);
    UNUSED(FileName);
    UNUSED(PublicAPIFunctionName);
}