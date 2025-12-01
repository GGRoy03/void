namespace StyleParser
{

//-----------------------------------------------------------------------------------
// @Internal: Data Maps & Constants

static constexpr uint64_t FNV64_BASIS            = 0xcbf29ce484222325ULL;
static constexpr uint64_t FNV64_PRIME            = 0x100000001b3ULL;
static constexpr uint64_t MAXIMUM_FILE_SIZE      = VOID_GIGABYTE(1);
static constexpr uint32_t INVALID_MAP_ENTRY      = 0xFFFFFFFFu;
static constexpr uint32_t MAXIMUM_TOKEN_PER_FILE = 100'000u;
static constexpr uint32_t MAXIMUM_VAR_PER_FILE   = 1024;

enum class StyleToken : uint32_t
{
    EndOfFile   = 0,
    Percent     = 37,
    Comma       = 44,
    Period      = 46,
    SemiColon   = 59,
    AtSymbol    = 64,
    OpenBrace   = 123,
    CloseBrace  = 125,
    Identifier  = 256,
    Assignment  = 257,
    Unit        = 258,
    String      = 259,
    Vector      = 260,
    Style       = 261,
    Var         = 262,
    None        = 1000,
};

static inline StyleToken FindStyleKeywordToken(byte_string Name)
{
    if (ByteStringMatches(str8_lit("style"), Name, 0)) return StyleToken::Style;
    if (ByteStringMatches(str8_lit("var")  , Name, 0)) return StyleToken::Var;

    return StyleToken::None;
}

static inline StyleProperty FindStyleProperty(byte_string Name)
{
    if (ByteStringMatches(str8_lit("size")            , Name, 0)) return StyleProperty::Size;
    if (ByteStringMatches(str8_lit("min-size")        , Name, 0)) return StyleProperty::MinSize;
    if (ByteStringMatches(str8_lit("max-size")        , Name, 0)) return StyleProperty::MaxSize;
    if (ByteStringMatches(str8_lit("grow")            , Name, 0)) return StyleProperty::Grow;
    if (ByteStringMatches(str8_lit("shrink")          , Name, 0)) return StyleProperty::Shrink;
    if (ByteStringMatches(str8_lit("layout-direction"), Name, 0)) return StyleProperty::LayoutDirection;
    if (ByteStringMatches(str8_lit("color")           , Name, 0)) return StyleProperty::Color;
    if (ByteStringMatches(str8_lit("padding")         , Name, 0)) return StyleProperty::Padding;
    if (ByteStringMatches(str8_lit("spacing")         , Name, 0)) return StyleProperty::Spacing;
    if (ByteStringMatches(str8_lit("softness")        , Name, 0)) return StyleProperty::Softness;
    if (ByteStringMatches(str8_lit("border-width")    , Name, 0)) return StyleProperty::BorderWidth;
    if (ByteStringMatches(str8_lit("border-color")    , Name, 0)) return StyleProperty::BorderColor;
    if (ByteStringMatches(str8_lit("corner-radius")   , Name, 0)) return StyleProperty::CornerRadius;
    if (ByteStringMatches(str8_lit("font")            , Name, 0)) return StyleProperty::Font;
    if (ByteStringMatches(str8_lit("font-size")       , Name, 0)) return StyleProperty::FontSize;
    if (ByteStringMatches(str8_lit("text-align")      , Name, 0)) return StyleProperty::TextAlign;
    if (ByteStringMatches(str8_lit("text-color")      , Name, 0)) return StyleProperty::TextColor;
    if (ByteStringMatches(str8_lit("caret-color")     , Name, 0)) return StyleProperty::CaretColor;
    if (ByteStringMatches(str8_lit("caret-width")     , Name, 0)) return StyleProperty::CaretWidth;

    return StyleProperty::None;
}

static inline Alignment FindAlignmentKeyword(byte_string Name)
{
    if (ByteStringMatches(str8_lit("start"),  Name, 0)) return Alignment::Start;
    if (ByteStringMatches(str8_lit("center"), Name, 0)) return Alignment::Center;
    if (ByteStringMatches(str8_lit("end"),    Name, 0)) return Alignment::End;

    return Alignment::None;
}

static inline LayoutDirection FindLayoutDirection(byte_string Name)
{
    if (ByteStringMatches(str8_lit("horizontal"), Name, 0)) return LayoutDirection::Horizontal;
    if (ByteStringMatches(str8_lit("vertical"),   Name, 0)) return LayoutDirection::Vertical;

    return LayoutDirection::None;
}

static inline StyleState FindStyleModifier(byte_string Name)
{
    if (ByteStringMatches(str8_lit("default"), Name, 0)) return StyleState::Default;
    if (ByteStringMatches(str8_lit("hovered"), Name, 0)) return StyleState::Hovered;
    if (ByteStringMatches(str8_lit("focused"), Name, 0)) return StyleState::Focused;

    return StyleState::None;
}

// ----------------------------------------------------------------------------------
// Style Parser Error Handling

typedef struct file_debug_info
{
    byte_string  FileName;
    os_read_file FileContent;

    uint32_t CurrentLineInFile;

    uint32_t ErrorCount;
    uint32_t WarningCount;
} file_debug_info;

static void
ReportStyleParserError(file_debug_info &Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console)
{
    if (Severity == ConsoleMessage_Error)
    {
        Debug.ErrorCount++;
    }
    else if (Severity == ConsoleMessage_Warn)
    {
        Debug.WarningCount++;
    }

    ConsoleWriteMessage(Severity, Message, Console);
}

static void
ReportStyleFileError(file_debug_info &Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console)
{
    uint8_t     Buffer[VOID_KILOBYTE(4)] = { 0 };
    byte_string Error = ByteString(Buffer, 0);

    Error.Size += snprintf((char *)Error.String, sizeof(Buffer), "[File %s | Line: %u]", Debug.FileName.String, Debug.CurrentLineInFile);
    Error.Size += snprintf((char *)Error.String + Error.Size, sizeof(Buffer), "[%s]", Message.String);

    ReportStyleParserError(Debug, Severity, Error, Console);
}

// ---------------------------------------------------------------------------------
// Style Tokenizer Implementation

typedef struct style_vector
{
    vec4_unit V;
    uint32_t  Size;
} style_vector;

struct style_token
{
    uint64_t LineInFile;
    uint64_t ByteInFile;

    StyleToken Type;
    union
    {
        ui_unit      Unit;
        byte_string  Identifier;
        style_vector Vector;
    };

    bool Matches(StyleToken Type)
    {
        bool Result = this->Type == Type;
        return Result;
    }
};

struct token_buffer
{
    style_token *Tokens;
    uint64_t     At;
    uint64_t     Count;
    uint64_t     Size;

    bool IsValid()
    {
        bool Result = (this->Tokens) && (this->At <= this->Count) && (this->Count < this->Size);
        return Result;
    }

    style_token * Get(uint64_t Index)
    {
        style_token *Result = 0;

        if(Index < this->Size)
        {
            Result = this->Tokens + Index;
        }

        return Result;
    }

    style_token * Peek(uint32_t Offset)
    {
        style_token *Result = this->Get(this->Count - 1);
        VOID_ASSERT(Result->Type == StyleToken::EndOfFile);

        uint64_t PeekIndex = this->At + Offset;
        if(PeekIndex < this->Count)
        {
            Result = this->Get(PeekIndex);
        }

        return Result;
    }

    style_token * Emit(StyleToken Type, uint64_t Line, uint64_t Byte)
    {
        style_token *Result = this->Get(this->Count++);

        if(Result)
        {
            Result->ByteInFile = Byte;
            Result->LineInFile = Line;
            Result->Type       = Type;
        }

        return Result;
    }

    void Eat(uint32_t Count)
    {
        if(this->At + Count < this->Count)
        {
            this->At += Count;
        }
        else
        {
            this->At = this->Count;
        }
    }

    bool Match(StyleToken Type)
    {
        style_token *Token = this->Peek(0);
        VOID_ASSERT(Token);

        bool Result = Token->Type == Type;
        return Result;
    }

    bool Expect(StyleToken Type)
    {
        style_token *Token = this->Peek(0);
        VOID_ASSERT(Token);

        bool Result = Token->Type == Type;

        if(Result)
        {
            this->Eat(1);
        }

        return Result;
    }

    style_token * Consume(StyleToken Type, byte_string Error, file_debug_info &Debug)
    {
        style_token *Token = this->Peek(0);
        VOID_ASSERT(Token);

        bool Expected = this->Expect(Type);

        if(!Expected)
        {
            Token = 0;
            ReportStyleFileError(Debug, ConsoleMessage_Error, Error, &UIState.Console);
        }

        return Token;
    }
};

static byte_string
ReadIdentifier(os_read_file *File)
{
    byte_string Result = ByteString(PeekFilePointer(File), 0);

    while (IsValidFile(File))
    {
        uint8_t Character = PeekFile(File, 0);
        if (IsAlpha(Character) || Character == '_' || Character == '-')
        {
            ++Result.Size;
            AdvanceFile(File, 1);
        }
        else
        {
            break;
        }
    }

    return Result;
}

static ui_unit
ReadUnit(os_read_file *File, file_debug_info &Debug)
{
    ui_unit Result = {};

    if (IsDigit(PeekFile(File, 0)))
    {
        double Number = 0;
        while (IsValidFile(File))
        {
            uint8_t Char = PeekFile(File, 0);
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

        if(IsValidFile(File) && PeekFile(File, 0) == '.')
        {
            AdvanceFile(File, 1);

            double C = 1.0 / 10.0;
            while (IsValidFile(File))
            {
                uint8_t Char = PeekFile(File, 0);
                if (IsDigit(Char))
                {
                    Number = Number + (C * (double)(Char - '0'));
                    C     *= 1.0f / 10.0f;

                    AdvanceFile(File, 1);
                }
                else
                {
                    break;
                }
            }
        }

        if(IsValidFile(File) && PeekFile(File, 0) == '%')
        {
            Result.Type = UIUnit_Percent;

            if (Number >= 0.f && Number <= 100.f)
            {
                Result.Percent = (float)Number;
                AdvanceFile(File, 1);
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Percent value must be: 0% <= Value <= 100%"));
            }
        }
        else
        {
            Result.Type    = UIUnit_Float32;
            Result.Float32 = (float)Number;
        }
    } else
    if (IsAlpha(PeekFile(File, 0)))
    {
        // TODO:
        // 1) I believe here we might need to check some values depending on how we encode widths/heights.
        //    For example; Fit, Grow, Stretch (We already handle Percent and Fixed
    } else
    {
        ReportStyleFileError(Debug, error_message("Could not parse identifier."));
    }

    return Result;
}

static style_vector
ReadVector(os_read_file *File, file_debug_info &Debug)
{
    style_vector Result = {};

    while(Result.Size < 4)
    {
        SkipWhiteSpaces(File);

        ui_unit Unit = ReadUnit(File, Debug);
        if (Unit.Type != UIUnit_None)
        {
            Result.V.Values[Result.Size++] = Unit;

            SkipWhiteSpaces(File);

            uint8_t Character = PeekFile(File, 0);
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
                break;
            }
        }
    }

    return Result;
}

static token_buffer
TokenizeStyleFile(os_read_file File, memory_arena *ParsingArena, file_debug_info &Debug)
{
    token_buffer Buffer =
    {
        .Tokens = PushArray(ParsingArena, style_token, MAXIMUM_TOKEN_PER_FILE),
        .Size   = MAXIMUM_TOKEN_PER_FILE,
    };

    while (IsValidFile(&File) && Buffer.IsValid())
    {
        SkipWhiteSpaces(&File);

        uint8_t  Char   = PeekFile(&File, 0);
        uint64_t AtLine = Debug.CurrentLineInFile;
        uint64_t AtByte = File.At;

        if (IsAlpha(Char))
        {
            byte_string Identifier = ReadIdentifier(&File);
            if(IsValidByteString(Identifier))
            {
                style_token *Token = 0;

                StyleToken TokenType = FindStyleKeywordToken(Identifier);
                if(TokenType != StyleToken::None)
                {
                    Token = Buffer.Emit(TokenType, AtLine, AtByte);
                }

                if(Token == 0)
                {
                    Token = Buffer.Emit(StyleToken::Identifier, AtLine, AtByte);
                    if(Token)
                    {
                        Token->Identifier = Identifier;
                    }
                }
            }

            continue;
        }

        if (IsDigit(Char))
        {
            style_token *Token = Buffer.Emit(StyleToken::Unit, AtLine, AtByte);
            if (Token)
            {
                ui_unit Unit = ReadUnit(&File, Debug);
                if(Unit.Type != UIUnit_None)
                {
                    Token->Unit = Unit;
                }
            }

            continue;
        }

        if (IsNewLine(Char))
        {
            uint8_t Next = PeekFile(&File, 1);
            if (Char == '\r' && Next == '\n')
            {
                AdvanceFile(&File, 2);
            }
            else
            {
                AdvanceFile(&File, 1);
            }

            ++Debug.CurrentLineInFile;

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(&File, 1) == '=')
            {
                Buffer.Emit(StyleToken::Assignment, AtLine, AtByte);
                AdvanceFile(&File, 2);
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Stray ':' did you mean ':='"));
            }

            continue;
        }

        if (Char == '[')
        {
            AdvanceFile(&File, 1); // Consumes '['

            style_token *Token = Buffer.Emit(StyleToken::Vector, AtLine, AtByte);
            if (Token)
            {
                 Token->Vector = ReadVector(&File, Debug);
            }

            continue;
        }

        if (Char == '"')
        {
            AdvanceFile(&File, 1); // Skip the first '"'

            byte_string String = ReadIdentifier(&File);
            if(IsValidByteString(String))
            {
                if(PeekFile(&File, 0) == '"')
                {
                    style_token *Token = Buffer.Emit(StyleToken::String, AtLine, AtByte);
                    if (Token)
                    {
                        Token->Identifier = String;
                    }

                    AdvanceFile(&File, 1); // Skip the second '"'
                }
                else
                {
                    ReportStyleFileError(Debug, error_message("Found invalid character in string"));
                }
            }

            continue;
        }

        if (Char == '{' || Char == '}' || Char == ';' || Char == '.' || Char == ',' || Char == '%' || Char == '@')
        {
            AdvanceFile(&File, 1);
            Buffer.Emit((StyleToken)Char, AtLine, AtByte);

            continue;
        }

        if(Char == '#')
        {
            AdvanceFile(&File, 1);

            while(!IsNewLine(PeekFile(&File, 0)))
            {
                AdvanceFile(&File, 1);
            }

            continue;
        }

        // BUG: Break? Seems wrong... Because 1 wrong token = invalid file??

        ReportStyleFileError(Debug, error_message("Found invalid character"));
        break;
    }

    // BUG:
    // If we have exactly the maximum amount of tokens, this will not get pushed
    // and we will parse to infinity.

    Buffer.Emit(StyleToken::EndOfFile, 0, 0);

    return Buffer;
}

//-----------------------------------------------------------------------------------
// Style Variables Implementation

typedef struct variable_hash
{
    uint64_t Value;
} variable_hash;

typedef struct variable_entry
{
    variable_hash Hash;
    uint32_t            NextWithSameHash;

    bool          ValueIsParsed;
    style_token *ValueToken;
} variable_entry;

struct variable_table_params
{
    uint32_t HashCount;
    uint32_t EntryCount;
};

typedef struct variable_table
{
    uint32_t HashMask;
    uint32_t HashCount;
    uint32_t EntryCount;

    uint32_t             *HashMap;
    variable_entry *Entries;
} variable_table;

typedef struct variable
{
    bool          IsValid;
    byte_string  Name;
    uint32_t          LineInFile;
    style_token *ValueToken;
} variable;

static variable_entry *
GetStyleVarEntry(uint32_t Index, variable_table *Map)
{
    VOID_ASSERT(Index < Map->EntryCount);

    variable_entry *Result = Map->Entries + Index;
    return Result;
}

static variable_entry *
GetStyleVarSentinel(variable_table *Map)
{
    variable_entry *Result = Map->Entries;
    return Result;
}

static variable_table *
PlaceVariableTableInMemory(variable_table_params Params, void *Memory)
{
    VOID_ASSERT(Params.EntryCount > 0);
    VOID_ASSERT(Params.HashCount > 0);
    VOID_ASSERT(VOID_ISPOWEROFTWO(Params.HashCount));

    variable_table *Result = 0;

    if (Memory)
    {
        Result = (variable_table *)Memory;
        Result->HashMap = (uint32_t *)(Result + 1);
        Result->Entries = (variable_entry *)(Result->HashMap + Params.HashCount);

        Result->HashMask = Params.HashCount - 1;
        Result->HashCount = Params.HashCount;
        Result->EntryCount = Params.EntryCount;

        MemorySet(Result->HashMap, 0, Result->HashCount * sizeof(Result->HashMap[0]));

        for (uint32_t Idx = 0; Idx < Params.EntryCount; Idx++)
        {
            variable_entry *Entry = GetStyleVarEntry(Idx, Result);
            if ((Idx + 1) < Params.EntryCount)
            {
                Entry->NextWithSameHash = Idx + 1;
            }
            else
            {
                Entry->NextWithSameHash = 0;
            }

            Entry->ValueIsParsed = 0;
        }
    }

    return Result;
}

static uint32_t
PopFreeStyleVarEntry(variable_table *Map)
{
    variable_entry *Sentinel = GetStyleVarSentinel(Map);

    if (!Sentinel->NextWithSameHash)
    {
        return 0;
    }

    uint32_t              Result = Sentinel->NextWithSameHash;
    variable_entry *Entry = GetStyleVarEntry(Result, Map);

    Sentinel->NextWithSameHash = Entry->NextWithSameHash;
    Entry->NextWithSameHash = 0;

    return Result;
}

static variable_entry *
FindStyleVarEntry(variable_hash Hash, variable_table *Map)
{
    uint32_t HashSlot = Hash.Value & Map->HashMask;
    uint32_t EntryIndex = Map->HashMap[HashSlot];

    variable_entry *Result = 0;
    while (EntryIndex)
    {
        variable_entry *Entry = GetStyleVarEntry(EntryIndex, Map);
        if (Hash.Value == Entry->Hash.Value)
        {
            Result = Entry;
            break;
        }

        EntryIndex = Entry->NextWithSameHash;
    }

    if (!Result)
    {
        EntryIndex = PopFreeStyleVarEntry(Map);
        if (EntryIndex)
        {
            Result = GetStyleVarEntry(EntryIndex, Map);
            Result->NextWithSameHash = Map->HashMap[HashSlot];
            Result->Hash = Hash;

            Map->HashMap[HashSlot] = EntryIndex;
        }
    }

    return Result;
}

static variable_hash
HashStyleVar(byte_string Name)
{
    variable_hash Result = { HashByteString(Name) };
    return Result;
}

static size_t
GetVariableTableFootprint(variable_table_params Params)
{
    size_t HashSize = Params.HashCount * sizeof(uint32_t);
    size_t EntrySize = Params.EntryCount * sizeof(variable_entry);
    size_t Result = sizeof(variable_table) + HashSize + EntrySize;

    return Result;
}

// ---------------------------------------------------------------------------------
// @Internal: Style Parser

static bool
CheckParsedVector(style_token *Token, uint32_t ExpectedSize, UIUnit_Type ExpectedType, byte_string Error, file_debug_info &Debug)
{
    bool Result = (Token->Type == StyleToken::Vector) && (Token->Vector.Size == ExpectedSize);

    if(Result && ExpectedType != UIUnit_None)
    {
        for(uint32_t Idx = 0; Idx < ExpectedSize; ++Idx)
        {
            if(Token->Vector.V.Values[Idx].Type != ExpectedType)
            {
                Result = 0;
                break;
            }
        }
    }

    if(!Result)
    {
        ReportStyleFileError(Debug, ConsoleMessage_Error, Error, &UIState.Console);
    }

    return Result;
}

static bool
CheckParsedColor(style_token *Token, file_debug_info &Debug)
{
    bool Result = CheckParsedVector(Token, 4, UIUnit_Float32, str8_lit("Expected a vector with four values"),  Debug);

    if(Result)
    {
        for(uint32_t Idx = 0; Idx < 4; ++Idx)
        {
            if(!IsInRange(0.f, 255.f, Token->Vector.V.Values[Idx].Float32))
            {
                Result = 0;
                break;
            }
        }
    }

    if(!Result)
    {
        ReportStyleFileError(Debug, ConsoleMessage_Error, str8_lit("Expected values between 0.0 and 255.0"), &UIState.Console);
    }

    return Result;
}

static bool
CheckParsedUnit(style_token *Token, UIUnit_Type ExpectedType, byte_string Error, file_debug_info &Debug)
{
    bool Result = (Token->Type == StyleToken::Unit && Token->Unit.Type == ExpectedType);

    if(!Result)
    {
        ReportStyleFileError(Debug, ConsoleMessage_Error, Error, &UIState.Console);
    }

    return Result;
}

static ui_color
ParsedVectorToColor(vec4_unit Vector)
{
    ui_color Result =
    {
        .R = Vector.X.Float32,
        .G = Vector.Y.Float32,
        .B = Vector.Z.Float32,
        .A = Vector.W.Float32,
    };

    Result = NormalizeColor(Result);

    return Result;
}

static void
ValidateProperty(style_token *Value, StyleProperty PropType, StyleState State, ui_cached_style *Style, file_debug_info &Debug)
{
    switch (PropType)
    {

    case StyleProperty::Size:
    {
        if(!CheckParsedVector(Value, 2, UIUnit_None, str8_lit("Expected a vector with two values."), Debug))
        {
            break;
        }

        if(Value->Vector.V.X.Type == UIUnit_Float32)
        {
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Horizontal.Type  = Sizing::Fixed;
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Horizontal.Fixed = Value->Vector.V.X.Float32;
        } else
        if(Value->Vector.V.X.Type == UIUnit_Percent && IsInRange(0.f, 100.f, Value->Vector.V.X.Percent))
        {
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Horizontal.Type  = Sizing::Percent;
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Horizontal.Fixed = Value->Vector.V.X.Percent;
        }
        else
        {
            break;
        }

        if(Value->Vector.V.Y.Type == UIUnit_Float32)
        {
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Vertical.Type  = Sizing::Fixed;
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Vertical.Fixed = Value->Vector.V.Y.Float32;
        } else
        if(Value->Vector.V.Y.Type == UIUnit_Percent)
        {
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Vertical.Type  = Sizing::Percent;
            Style->Properties[static_cast<uint32_t>(State)].Sizing.Vertical.Fixed = Value->Vector.V.X.Percent;
        }
        else
        {
            break;
        }
    } break;

    case StyleProperty::MinSize:
    {
        if(!CheckParsedVector(Value, 2, UIUnit_Float32, str8_lit("Expected a vector with two values."), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].MinSize =
        {
            .Width  = Value->Vector.V.X.Float32,
            .Height = Value->Vector.V.Y.Float32,
        };

    } break;

    case StyleProperty::MaxSize:
    {
        if(!CheckParsedVector(Value, 2, UIUnit_Float32, str8_lit("Expected a vector with two values."), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].MaxSize =
        {
            .Width  = Value->Vector.V.X.Float32,
            .Height = Value->Vector.V.Y.Float32,
        };
    } break;

    case StyleProperty::LayoutDirection:
    {
        if(!Value->Matches(StyleToken::Identifier))
        {
            break;
        }

        LayoutDirection Direction = FindLayoutDirection(Value->Identifier);
        if(Direction == LayoutDirection::None)
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].LayoutDirection = Direction;
    } break;

    case StyleProperty::Padding:
    {
        if(!CheckParsedVector(Value, 4, UIUnit_Float32, str8_lit("Expected a vector with fours values."), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].Padding =
        {
            .Left  = Value->Vector.V.X.Float32,
            .Top   = Value->Vector.V.Y.Float32,
            .Right = Value->Vector.V.Z.Float32,
            .Bot   = Value->Vector.V.W.Float32,
        };

    } break;

    case StyleProperty::CornerRadius:
    {
        if(!CheckParsedVector(Value, 4, UIUnit_Float32, str8_lit("Expected a vector with four values."), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].CornerRadius =
        {
            .TL = Value->Vector.V.X.Float32,
            .TR = Value->Vector.V.Y.Float32,
            .BR = Value->Vector.V.Z.Float32,
            .BL = Value->Vector.V.W.Float32,
        };

    } break;

    case StyleProperty::Color:
    {
        if(!CheckParsedColor(Value, Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].Color = ParsedVectorToColor(Value->Vector.V);
    } break;

    case StyleProperty::BorderColor:
    {
        if(!CheckParsedColor(Value, Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].BorderColor = ParsedVectorToColor(Value->Vector.V);
    } break;

    case StyleProperty::TextColor:
    {
        if(!CheckParsedColor(Value, Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].TextColor = ParsedVectorToColor(Value->Vector.V);
    } break;

    case StyleProperty::CaretColor:
    {
        if(!CheckParsedColor(Value, Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].BorderColor = ParsedVectorToColor(Value->Vector.V);
    } break;

    case StyleProperty::Spacing:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].Spacing = Value->Unit.Float32;
    } break;

    case StyleProperty::BorderWidth:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].BorderWidth = Value->Unit.Float32;
    } break;

    case StyleProperty::Softness:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].Softness = Value->Unit.Float32;
    } break;

    case StyleProperty::Grow:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].Grow = Value->Unit.Float32;
    } break;

    case StyleProperty::Shrink:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].Shrink = Value->Unit.Float32;
    } break;

    case StyleProperty::FontSize:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].FontSize = Value->Unit.Float32;
    } break;

    case StyleProperty::CaretWidth:
    {
        if (!CheckParsedUnit(Value, UIUnit_Float32, str8_lit(""), Debug))
        {
            break;
        }

        Style->Properties[static_cast<uint32_t>(State)].CaretWidth = Value->Unit.Float32;
    } break;

    case StyleProperty::Font:
    {
        if (Value->Type != StyleToken::String || !IsValidByteString(Value->Identifier))
        {
            break;
        }

        VOID_ASSERT(!"Implement");

    } break;

    case StyleProperty::TextAlign:
    {
        VOID_ASSERT(!"Implement");
    } break;

    default:
    {
        VOID_ASSERT(!"Impossible");
    } break;

    }
}

enum class StyleParserState : uint32_t
{
    Start,

    ExpectVariable,
    StoreVariable,

    ExpectHeader,
    BlockStart,

    ExpectEffect,
    ExpectProperty,

    Done,
    Error,
};

struct style_parser
{
    StyleParserState State;
    ui_cached_style  Output;
    variable         Variable;
    StyleState       StyleState;
};

static void
ParseStyleFile(token_buffer &Buffer, variable_table *VariableTable, memory_arena *OutputArena, ui_cached_style_list *List, file_debug_info &Debug)
{
    style_parser Parser =
    {
        .State  = StyleParserState::Start,
        .Output = {},
    };

    while (Parser.State != StyleParserState::Done && Parser.State != StyleParserState::Error)
    {
        if (Buffer.Expect(StyleToken::EndOfFile))
        {
            if (Parser.State == StyleParserState::Start)
            {
                Parser.State = StyleParserState::Done;
                break;
            }

            ReportStyleFileError(Debug, error_message("Unexpected End Of File"));
            Parser.State = StyleParserState::Error;
            break;
        }

        switch (Parser.State)
        {

        case StyleParserState::Start:
        {
            if(Buffer.Expect(StyleToken::Var))
            {
                Parser.State = StyleParserState::ExpectVariable;
            } else
            if(Buffer.Expect(StyleToken::Style))
            {
                Parser.State = StyleParserState::ExpectHeader;
            }
            else
            {
                VOID_ASSERT(!"Sync");
            }
        } break;

        case StyleParserState::ExpectVariable:
        {
            variable Variable = {};

            style_token *Name = Buffer.Consume(StyleToken::Identifier, byte_string_literal("Expected a name after 'var'."), Debug);
            if (!IsValidByteString(Name->Identifier))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            Variable.Name = Name->Identifier;

            if (!Buffer.Consume(StyleToken::Assignment, byte_string_literal("Expected an assignment"), Debug))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            style_token *Value = Buffer.Peek(0);
            if(Buffer.Expect(StyleToken::Unit) || Buffer.Expect(StyleToken::String) || Buffer.Expect(StyleToken::Vector))
            {
                Variable.ValueToken = Value;
            }
            else
            {
                ReportStyleFileError(Debug, error_message("Expected a value (unit, string, or vector)"));
            }

            if (!Buffer.Consume(StyleToken::SemiColon, byte_string_literal("Expected a ;"), Debug))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            Parser.State    = StyleParserState::StoreVariable;
            Parser.Variable = Variable;
        } break;

        case StyleParserState::StoreVariable:
        {
            VOID_ASSERT(IsValidByteString(Parser.Variable.Name));
            VOID_ASSERT(Parser.Variable.ValueToken);

            variable_hash   Hash  = HashStyleVar(Parser.Variable.Name);
            variable_entry *Entry = FindStyleVarEntry(Hash, VariableTable);

            if (!Entry)
            {
                ReportStyleFileError(Debug, error_message("Too many variables inside the same file."));
            }
            else if (Entry->ValueToken)
            {
                ReportStyleFileError(Debug, error_message("Found two variables with the same name."));
            }
            else
            {
                Entry->ValueToken          = Parser.Variable.ValueToken;
                Parser.Variable.ValueToken = {};
            }

            Parser.State = StyleParserState::Start;
        } break;

        case StyleParserState::ExpectHeader:
        {
            if (!Buffer.Consume(StyleToken::String, byte_string_literal("Expected style name as string"), Debug))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            if (!Buffer.Consume(StyleToken::OpenBrace, byte_string_literal("Expected a {."), Debug))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            Parser.State = StyleParserState::BlockStart;
        } break;

        case StyleParserState::BlockStart:
        {
            if(Buffer.Expect(StyleToken::AtSymbol))
            {
                Parser.State = StyleParserState::ExpectEffect;
                break;
            }

            if(Buffer.Expect(StyleToken::CloseBrace))
            {
                ui_cached_style_node *Node = PushStruct(OutputArena, ui_cached_style_node);

                if (!Node)
                {
                    Parser.State = StyleParserState::Error;
                    break;
                }

                Node->Value   = Parser.Output;
                Parser.Output = {};            // TODO: Create a small helper to reset the state or do it inline at the start state.

                AppendToLinkedList(List, Node, List->Count);

                Parser.State = StyleParserState::Start;
                break;
            }

            Parser.State = StyleParserState::ExpectProperty;
        } break;

        case StyleParserState::ExpectEffect:
        {
            style_token *Name = Buffer.Consume(StyleToken::Identifier, str8_lit("Expected a valid effect name"), Debug);
            if(Name)
            {
                StyleState Modifier = FindStyleModifier(Name->Identifier);
                if(Modifier != StyleState::None)
                {
                    Parser.StyleState = Modifier;
                }
            }

            Parser.State = StyleParserState::BlockStart;
        } break;

        case StyleParserState::ExpectProperty:
        {
            StyleProperty Property = StyleProperty::None;

            style_token *Name = Buffer.Consume(StyleToken::Identifier, str8_lit("Expected a property name"), Debug);
            if(Name)
            {
                Property = FindStyleProperty(Name->Identifier);
                if(Property == StyleProperty::None)
                {
                    ReportStyleFileError(Debug, error_message("Invalid attribute name"));
                    break;
                }
            }
            else
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            if(!Buffer.Consume(StyleToken::Assignment, byte_string_literal("Expected an assignment ':='"), Debug))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            // NOTE:
            // Quite Garbage.

            style_token *Value = Buffer.Peek(0);
            if(Buffer.Expect(StyleToken::Identifier))
            {
                variable_hash   Hash  = HashStyleVar(Value->Identifier);
                variable_entry *Entry = FindStyleVarEntry(Hash, VariableTable);

                if(!Entry || !Entry->ValueToken)
                {
                    VOID_ASSERT(!"Sync");
                    ReportStyleFileError(Debug, error_message("Undefined variable."));
                    break;
                }

                Value = Entry->ValueToken;
            }
            else
            {
                Buffer.Eat(1);
            }

            if (!(Value->Matches(StyleToken::String) || Value->Matches(StyleToken::Vector) || Value->Matches(StyleToken::Unit)))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            if(!Buffer.Consume(StyleToken::SemiColon, byte_string_literal("Expected a semicolon"), Debug))
            {
                VOID_ASSERT(!"Sync");
                break;
            }

            ValidateProperty(Value, Property, Parser.StyleState, &Parser.Output, Debug);

            Parser.State = StyleParserState::BlockStart;
        } break;

        case StyleParserState::Error:
        case StyleParserState::Done:
        {
        } break;

        }
    }
}

// ----------------------------------------------------------------------------------
// @Public: Parser API

static ui_cached_style_list *
LoadStyles(os_read_file *Files, uint32_t FileCount, memory_arena *OutputArena)
{
    VOID_ASSERT(Files);
    VOID_ASSERT(OutputArena);

    ui_cached_style_list *Result = PushStruct(OutputArena, ui_cached_style_list);

    memory_arena *ParsingArena = {};
    {
        memory_arena_params Params =
        {
            .ReserveSize       = VOID_GIGABYTE(1),
            .CommitSize        = VOID_MEGABYTE(64),
            .AllocatedFromFile = __FILE__,
            .AllocatedFromLine = __LINE__,
        };

        ParsingArena = AllocateArena(Params);
    }

    if(Result && ParsingArena)
    {
        variable_table *VariableTable = 0;
        {
            variable_table_params Params =
            {
                .HashCount  = MAXIMUM_VAR_PER_FILE / 4,
                .EntryCount = MAXIMUM_VAR_PER_FILE,
            };

            uint64_t Footprint = GetVariableTableFootprint(Params);
            uint8_t *Memory    = PushArray(ParsingArena, uint8_t, Footprint);

            VariableTable = PlaceVariableTableInMemory(Params, Memory);
        }

        if(VariableTable)
        {
            for(uint32_t Idx = 0; Idx < FileCount; ++Idx)
            {
                memory_region Loop = EnterMemoryRegion(ParsingArena);

                os_read_file File = Files[Idx];
                if(File.FullyRead)
                {
                    file_debug_info Debug =
                    {
                        .FileContent = File,
                    };

                    token_buffer Buffer = TokenizeStyleFile(File, ParsingArena, Debug);
                    if(Buffer.IsValid())
                    {
                        ParseStyleFile(Buffer, VariableTable, OutputArena, Result, Debug);
                    }
                }

                LeaveMemoryRegion(Loop);
            }
        }

        ReleaseArena(ParsingArena);
    }


    return Result;
}

} // namespace StyleParser
