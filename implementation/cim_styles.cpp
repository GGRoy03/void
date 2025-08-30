// 1) This is long-term, but some of the errors are probably not clear enough.

// [Internal]
static theme_parsing_error StoreAttributeInTheme   (UIThemeAttribute_Flag Attribute, theme_token *Value, theme_parser *Parser);
static theme_parsing_error ValidateAndStoreThemes  (theme_parser *Parser);

// Tokenizer Helpers
static theme_token        * CreateThemeToken    (UIStyleToken_Type Type, cim_u32 Line, buffer *TokenBuffer);
static theme_parsing_error  GetNextTokenBuffer  (buffer *FileContent, theme_parser *Parser);

// Parser Helpers
static ui_theme   * AllocateUITheme  (cim_u8 *ThemeName, cim_u32 ThemeNameLength, UITheme_Type Type);
static theme_info * GetNextUITheme   ();

// General Parsing Helpers
static bool   IsAlpha            (cim_u8 Char);
static bool   IsNumber           (cim_u8 Char);
static bool   IsWhiteSpace       (cim_u8 Char);
static cim_u8 ToLowerChar        (cim_u8 Char);
static bool   StringMatches      (cim_u8 *Ptr, cim_u8 *String, cim_u32 StringSize);
static void   IgnoreWhiteSpaces  (buffer *Content);

// Error Handling
static void        WriteErrorMessage         (theme_parsing_error *Erro, const char *Format, ...);
static void        HandleThemeError          (theme_parsing_error *Error, char *FileName, const char *PublicAPIFunctionName);
static const char *UIThemeAttributeToString  (UIThemeAttribute_Flag Flag);

// [Macros]

// TODO: Make these functions?

#define CimTheme_MaxVectorSize 4
#define CimTheme_ArrayToVec2(Array)  {Array[0], Array[1]}
#define CimTheme_ArrayToVec4(Array)  {Array[0], Array[1], Array[2], Array[3]}
#define CimTheme_ArrayToColor(Array) {Array[0] * 1/255, Array[1] * 1/255, Array[2] * 1/255, Array[3] * 1/255}

// [Globals]

static theme_table UITheme_Table;

// [Public API Implementation]

static ui_theme *
GetUITheme(const char *ThemeName, theme_id *ComponentId)
{
    if (ComponentId && ComponentId->Value >= UITheme_NameLength)
    {
        theme_info *ThemeInfo = &UITheme_Table.Themes[ComponentId->Value];
        return &ThemeInfo->Theme;
    }
    else
    {
        Cim_Assert(ThemeName);

        cim_u8 *NameToFind = (cim_u8 *)ThemeName;
        cim_u32 NameLength = (cim_u32)strlen(ThemeName);

        theme_info *Sentinel = &UITheme_Table.Themes[NameLength];
        theme_id    ReadPointer = Sentinel->NextWithSameLength;

        while (ReadPointer.Value)
        {
            theme_info *ThemeInfo = &UITheme_Table.Themes[ReadPointer.Value];

            if (strcmp((const char *)NameToFind, (const char *)ThemeInfo->Name) == 0)
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

static ui_window_theme
GetWindowTheme(const char *ThemeName, theme_id ThemeId)
{
    ui_window_theme Result = {};
    ui_theme       *Theme  = GetUITheme(ThemeName, &ThemeId);
    Cim_Assert(Theme && Theme->Type == UITheme_Window);

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

static ui_button_theme
GetButtonTheme(const char *ThemeName, theme_id ThemeId)
{
    ui_button_theme Result = {};
    ui_theme       *Theme  = GetUITheme(ThemeName, &ThemeId);
    Cim_Assert(Theme->Type == UITheme_Button);

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

// BUG: This leaks data on error.
static void
LoadThemeFiles(char **Files, cim_u32 FileCount)
{
    if (!Files)
    {
        theme_parsing_error Error;
        Error.Type          = ThemeParsingError_Argument;
        Error.ArgumentIndex = 0;
        WriteErrorMessage(&Error, "Must be a valid pointer of type char**.");

        HandleThemeError(&Error, NULL, "InitializeUIThemes");

        return;
    }

    theme_parser Parser = {};

    for (cim_u32 FileIdx = 0; FileIdx < FileCount; FileIdx++)
    {
        char *FileName = Files[FileIdx];

        buffer FileContent = PlatformReadFile(FileName);
        if (!FileContent.Data || !FileContent.Size)
        {
            continue;
        }

        theme_parsing_error TokenError = GetNextTokenBuffer(&FileContent, &Parser);
        if (TokenError.Type != ThemeParsingError_None)
        {
            HandleThemeError(&TokenError, FileName, "InitializeUIThemes");
            continue;
        }

        theme_parsing_error ParseError = ValidateAndStoreThemes(&Parser);
        if (ParseError.Type != ThemeParsingError_None)
        {
            HandleThemeError(&ParseError, FileName, "InitializeUIThemes");
            continue;
        }

        Parser.TokenBuffer.At = 0;
        FreeBuffer(&FileContent);  // NOTE: A lot of free/malloc, could be better.

        CimLog_Info("Successfully parsed file: %s", FileName);
    }

    FreeBuffer(&Parser.TokenBuffer);
}

// [Internal Implementation]

static theme_token *
CreateThemeToken(UIStyleToken_Type Type, cim_u32 Line, buffer *TokenBuffer)
{
    if (!IsValidBuffer(TokenBuffer))
    {
        size_t NewSize = (TokenBuffer->Size * 2) * sizeof(theme_token);
        buffer Temp    = AllocateBuffer(NewSize);
        Temp.At = TokenBuffer->At;

        if (!Temp.Data)
        {
            // TODO: Report error.
            return NULL;
        }

        memcpy(Temp.Data, TokenBuffer->Data, TokenBuffer->Size);

        FreeBuffer(TokenBuffer);

        TokenBuffer->Data = Temp.Data;
        TokenBuffer->Size = NewSize;
        TokenBuffer->At   = Temp.At;
    }

    theme_token *Result = (theme_token*)(TokenBuffer->Data + TokenBuffer->At);
    Result->LineInFile = Line;
    Result->Type       = Type;

    TokenBuffer->At += sizeof(theme_token);

    return Result;
}

static void
IgnoreWhiteSpaces(buffer *Content)
{
    while(IsValidBuffer(Content) && IsWhiteSpace(Content->Data[Content->At]))
    {
        Content->At++;
    }
}

static bool
StringMatches(cim_u8 *Ptr, cim_u8 *String, cim_u32 StringSize)
{
    cim_u32 Matches = 0;
    while(Matches < StringSize && *Ptr++ == *String++)
    {
        Matches++;
    }

    bool Result = Matches == StringSize;
    return Result;
}

static bool
IsAlpha(cim_u8 Char)
{
    bool Result = (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z');
    return Result;
}

static bool
IsNumber(cim_u8 Char)
{
    bool Result = (Char >= '0' && Char <= '9');
    return Result;
}

static bool
IsWhiteSpace(cim_u8 Char)
{
    bool Result = (Char == ' ') || (Char == '\t');
    return Result;
}

static cim_u8 
ToLowerChar(cim_u8 Char)
{
    Cim_Assert(IsAlpha(Char));

    cim_u8 Result = '\0';
    if (Char < 'a')
    {
        Result = Char + ('a' - 'A');
    }
    else
    {
        Result = Char;
    }

    return Result;
}

// NOTE: Why do I use a goto here? Unless I cleanup some resources?
static theme_parsing_error
GetNextTokenBuffer(buffer *FileContent, theme_parser *Parser)
{
    theme_parsing_error Error = {};

    Parser->TokenBuffer = AllocateBuffer(128 * sizeof(theme_token));
    Parser->AtLine      = 1;

    while(IsValidBuffer(FileContent))
    {
        IgnoreWhiteSpaces(FileContent);

        cim_u8  Char    = FileContent->Data[FileContent->At];
        cim_u64 At      = FileContent->At;
        cim_u64 StartAt = At;

        switch(Char)
        {

        // WARN: Compiler extension.
        case 'A' ... 'Z':
        case 'a' ... 'z':
        {
            At++;

            Char = FileContent->Data[At];
            while (IsValidBuffer(FileContent) && (IsAlpha(Char) || IsNumber(Char)))
            {
                Char = FileContent->Data[++At];
            }

            if(!IsValidBuffer(FileContent))
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Stray Identifier at end of file.");
            }

            cim_u32 IdLength = At - StartAt;
            cim_u8 *IdPtr    = FileContent->Data + StartAt;

            const cim_u32 ThemeLength = sizeof("theme") - 1;
            const cim_u32 ForLength   = sizeof("for")   - 1;

            if (IdLength == ThemeLength && StringMatches(IdPtr, (cim_u8 *)"theme", ThemeLength))
            {
                CreateThemeToken(UIStyleToken_Theme, Parser->AtLine, &Parser->TokenBuffer);
            }
            else if (IdLength == ForLength && StringMatches(IdPtr, (cim_u8 *)"for", ForLength))
            {
                CreateThemeToken(UIStyleToken_For, Parser->AtLine, &Parser->TokenBuffer);
            }
            else
            {
                theme_token *Token = CreateThemeToken(UIStyleToken_Identifier,
                                                      Parser->AtLine,
                                                      &Parser->TokenBuffer);
                Token->Identifier.At   = IdPtr;
                Token->Identifier.Size = IdLength;
            }
        } break;

        // WARN: Compiler extension.
        case '0' ... '9':
        {
            theme_token *Token = CreateThemeToken(UIStyleToken_Number, Parser->AtLine, &Parser->TokenBuffer);

            Char = FileContent->Data[At];
            while (IsValidBuffer(FileContent) && IsNumber(Char))
            {
                Token->UInt32 = (Token->UInt32 * 10) + (Char - '0');
                Char          = FileContent->Data[++At];
            }
        } break;

        case '\r':
        case '\n':
        {
            At++;
            if (Char == '\r' && FileContent->Data[At] == '\n')
            {
                At++;
            }

            Parser->AtLine += 1;
        } break;

        case ':':
        {
            At++;

            if (IsValidBuffer(FileContent) && FileContent->Data[At] == '=')
            {
                CreateThemeToken(UIStyleToken_Assignment, Parser->AtLine, &Parser->TokenBuffer);
                At++;
            }
            else
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Stray ':'. Did you mean := ?");

                goto End;
            }
        } break;

        case '[':
        {
            At++;

            theme_token *Token = CreateThemeToken(UIStyleToken_Vector, Parser->AtLine, &Parser->TokenBuffer);

            while (Token->Vector.Size < CimTheme_MaxVectorSize && IsValidBuffer(FileContent))
            {
                IgnoreWhiteSpaces(FileContent);

                Char        = FileContent->Data[At++];
                cim_u32 Idx = Token->Vector.Size;
                while (IsValidBuffer(FileContent) && IsNumber(Char))
                {
                    Token->Vector.DataF32[Idx] = (Token->Vector.DataF32[Idx] * 10) + (Char - '0');
                    Char                       = FileContent->Data[At++];     
                }

                IgnoreWhiteSpaces(FileContent);

                if (Char == ',')
                {
                    At++;
                    Token->Vector.Size++;
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

                    goto End;
                }
            }

            if (Char != ']')
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Vector exceeds maximum size (4).");

                goto End;
            }
        } break;

        case '"':
        {
            At++;

            theme_token *Token = CreateThemeToken(UIStyleToken_String, Parser->AtLine, &Parser->TokenBuffer);
            Token->Identifier.At = FileContent->Data + At;

            Char = FileContent->Data[At];
            while (IsValidBuffer(FileContent) && Char != '"')
            {
                Char = FileContent->Data[++At];
            }

            if (!IsValidBuffer(FileContent))
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "End of file reached without closing string.");

                goto End;
            }

            Token->Identifier.Size = (FileContent->Data + At) - Token->Identifier.At;
            At++;
        } break;

        case '@':
        {
            At++;

            if (StringMatches(FileContent->Data + At, (cim_u8*)"hover", sizeof("hover") - 1))
            {
                CreateThemeToken(UIStyleToken_HoverEffect, Parser->AtLine, &Parser->TokenBuffer);
                At += sizeof("effects") - 1;
            }
            else if (StringMatches(FileContent->Data + At, (cim_u8 *)"click", sizeof("click") - 1))
            {
                CreateThemeToken(UIStyleToken_ClickEffect, Parser->AtLine, &Parser->TokenBuffer);
                At += sizeof("click") - 1;
            }
            else
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "'@' Special Marker must only be used when specifying effects for a theme. Example: @Effects");

                goto End;
            }
        } break;

        default:
        {
            CreateThemeToken((UIStyleToken_Type)Char, Parser->AtLine, &Parser->TokenBuffer);
            At++;
        } break;

        }

        FileContent->At = At;
    }

// BUG: We leak on errors.
End:
    return Error;
}

static theme_parsing_error
StoreAttributeInTheme(UIThemeAttribute_Flag Attribute, theme_token *Value, theme_parser *Parser)
{
    theme_parsing_error Error = {};
    Error.Type = ThemeParsingError_None;

    switch (Parser->ActiveTheme->Type)
    {

    case UITheme_None:
    {
        theme_parsing_error Error = {};
        Error.Type                = ThemeParsingError_Internal;
        WriteErrorMessage(&Error, "Invalid parser state when trying to set an attribute. Should have been found earlier.");

        return Error;
    }

    case UITheme_Window:
    {
        ui_window_theme *Theme = &Parser->ActiveTheme->Window;

        switch (Attribute)
        {

        case UIThemeAttribute_Size:        Theme->Size        = CimTheme_ArrayToVec2(Value->Vector.DataF32);  break;
        case UIThemeAttribute_Color:       Theme->Color       = CimTheme_ArrayToColor(Value->Vector.DataF32); break;
        case UIThemeAttribute_Padding:     Theme->Padding     = CimTheme_ArrayToVec4(Value->Vector.DataF32);  break;
        case UIThemeAttribute_Spacing:     Theme->Spacing     = CimTheme_ArrayToVec2(Value->Vector.DataF32);  break;
        case UIThemeAttribute_BorderColor: Theme->BorderColor = CimTheme_ArrayToColor(Value->Vector.DataF32); break;
        case UIThemeAttribute_BorderWidth: Theme->BorderWidth = Value->UInt32;                                break;

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
        ui_button_theme *Theme = &Parser->ActiveTheme->Button;

        switch (Attribute)
        {

        case UIThemeAttribute_Size:        Theme->Size        = CimTheme_ArrayToVec2(Value->Vector.DataF32);  break;
        case UIThemeAttribute_Color:       Theme->Color       = CimTheme_ArrayToColor(Value->Vector.DataF32); break;
        case UIThemeAttribute_BorderColor: Theme->BorderColor = CimTheme_ArrayToColor(Value->Vector.DataF32); break;
        case UIThemeAttribute_BorderWidth: Theme->BorderWidth = Value->UInt32;                                break;

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

static theme_parsing_error
ValidateAndStoreThemes(theme_parser *Parser)
{
    theme_parsing_error Error = {};

    theme_token *Tokens     = (theme_token *)Parser->TokenBuffer.Data;;
    cim_u32      TokenCount = Parser->TokenBuffer.At / sizeof(theme_token);

    while (Parser->AtToken< TokenCount)
    {
        theme_token *Token = Tokens + Parser->AtToken;

        switch (Token->Type)
        {

        case UIStyleToken_Theme:
        {
            if (Parser->ActiveTheme && Parser->ActiveTheme->Type != UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = Parser->AtLine;
                WriteErrorMessage(&Error, "Forgot to close previous ui_theme with } ?");

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
                WriteErrorMessage(&Error, "A theme must be set like this: Theme \"NameOfTheme\" for ComponentType");

                return Error;
            }

            theme_token *ThemeNameToken     = Token + 1;
            theme_token *ComponentTypeToken = Token + 3;

            typedef struct known_type { const char *Name; size_t Length; UITheme_Type Type; } known_type;
            known_type KnownTypes[] =
            {
                {"window", sizeof("window") - 1, UITheme_Window},
                {"button", sizeof("button") - 1, UITheme_Button},
            };

            for (cim_u32 KnownIdx = 0; KnownIdx < Cim_ArrayCount(KnownTypes); ++KnownIdx)
            {
                known_type Known = KnownTypes[KnownIdx];

                if (Known.Length != ComponentTypeToken->Identifier.Size)
                {
                    continue;
                }

                cim_u32 CharIdx;
                for (CharIdx = 0; CharIdx < ComponentTypeToken->Identifier.Size; CharIdx++)
                {
                    cim_u8 LowerChar = ToLowerChar(ComponentTypeToken->Identifier.At[CharIdx]);
                    if (LowerChar != Known.Name[CharIdx])
                    {
                        break;
                    }
                }

                if (CharIdx == Known.Length)
                {
                    Parser->ActiveTheme = AllocateUITheme(ThemeNameToken->Identifier.At, ThemeNameToken->Identifier.Size, Known.Type);
                    break;
                }
            }

            if (!Parser->ActiveTheme || Parser->ActiveTheme->Type == UITheme_None)
            {
                Error.Type       = ThemeParsingError_Syntax;
                Error.LineInFile = ComponentTypeToken->LineInFile;

                // BUG: Can easily overflow.
                Cim_Assert(ThemeNameToken->Identifier.Size < 64);
                char ThemeName[64] = { 0 };
                memcpy(ThemeName, ThemeNameToken->Identifier.At, ThemeNameToken->Identifier.Size);
                WriteErrorMessage(&Error, "Invalid component type for : %s", ThemeName);

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

                // BUG: Can easily overflow.
                char IdentifierString[64];
                memcpy(IdentifierString, Token->Identifier.At, Token->Identifier.Size);
                WriteErrorMessage(&Error, "Found identifier: %s | Outside of a ui_theme block.", IdentifierString);

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

            theme_token *AttributeToken = Token;
            theme_token *ValueToken     = Token + 2;

            bool HasNegativeValue = false;
            if (Token[2].Type == '-')
            {
                HasNegativeValue = true;

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
            typedef struct known_type { const char *Name; size_t Length; UIThemeAttribute_Flag TypeFlag; cim_bit_field ValidValueTokenMask; } known_type;
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
            for (cim_u32 KnownIdx = 0; KnownIdx < Cim_ArrayCount(KnownTypes); ++KnownIdx)
            {
                known_type Known = KnownTypes[KnownIdx];

                if (Known.Length != AttributeToken->Identifier.Size)
                {
                    continue;
                }

                cim_u32 CharIdx;
                for (CharIdx = 0; CharIdx < AttributeToken->Identifier.Size; CharIdx++)
                {
                    cim_u8 LowerChar = ToLowerChar(AttributeToken->Identifier.At[CharIdx]);
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

                // BUG: Can easily overflow.
                char AttributeName[64] = { 0 };
                memcpy(AttributeName, AttributeToken->Identifier.At, AttributeToken->Identifier.Size);
                WriteErrorMessage(&Error, "Invalid attribute name: %s", AttributeName);

                return Error;
            }

            theme_parsing_error Error = StoreAttributeInTheme(Attribute, ValueToken, Parser);
            if (Error.Type != ThemeParsingError_None)
            {
                return Error;
            }

            Parser->AtToken += HasNegativeValue ? 5 : 4;
        } break;

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

// [Parser Helpers]

static theme_info *
GetNextUITheme()
{
    theme_info *Result = NULL;
    cim_u32    Index   = UITheme_Table.NextWriteIndex + UITheme_NameLength;  // WARN: Kind of a hack.

    if (Index < UITheme_ThemeCount)
    {
        Result     = &UITheme_Table.Themes[Index];
        Result->Id = { Index };
        Cim_Assert(Result->NameLength == 0);

        UITheme_Table.NextWriteIndex += 1;
    }

    return Result;
}

static ui_theme *
AllocateUITheme(cim_u8 *ThemeName, cim_u32 ThemeNameLength, UITheme_Type Type)
{
    ui_theme *Result = GetUITheme((char *)ThemeName, NULL); // NOTE: Pass NULL, because we don't store the ID.

    if (!Result)
    {
        theme_info *Sentinel = &UITheme_Table.Themes[ThemeNameLength];
        Cim_Assert(ThemeNameLength < 64);

        theme_info *New = GetNextUITheme();
        New->Theme.Type         = Type;
        New->NameLength         = ThemeNameLength;
        New->NextWithSameLength = Sentinel->NextWithSameLength;
        memcpy(New->Name, ThemeName, ThemeNameLength);

        Sentinel->NextWithSameLength = New->Id;

        Result = &New->Theme;
        UITheme_Table.NextWriteIndex += 1;

        Result = &New->Theme;
    }

    return Result;
}

// [Error Handling]

static const char *
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
WriteErrorMessage(theme_parsing_error *Error, const char *Format, ...)
{
    va_list Args;
    __crt_va_start(Args, Format);
    __va_start(&Args, Format);

    vsnprintf(Error->Message, UITheme_ErrorMessageLength, Format, Args);

    __crt_va_end(Args);
}

static void
HandleThemeError(theme_parsing_error *Error, char *FileName, const char *PublicAPIFunctionName)
{
    switch (Error->Type)
    {

    case ThemeParsingError_Syntax:
    {
        CimLog_Error("Syntax Error: In file %s, at line %u -> %s",
                     FileName, Error->LineInFile, Error->Message);
    } break;

    case ThemeParsingError_Argument:
    {
        CimLog_Error("Argument Error: When calling %s argument %u is invalid -> %s",
                      PublicAPIFunctionName, Error->ArgumentIndex, Error->Message);
    } break;

    case ThemeParsingError_Internal:
    {
        CimLog_Error("Internal Error. Please report it at: https://github.com/GGRoy03/CIM/. Error: %s",
                     Error->Message);
    } break;

    default:
    {
        CimLog_Error("UNKNOWN ERROR TYPE");
        break;
    }

    }
}