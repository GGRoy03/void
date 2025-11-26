//-----------------------------------------------------------------------------------
// Data Tables

typedef enum StyleToken_Type
{
    StyleToken_EndOfFile   = 0,
    StyleToken_Percent     = 37,
    StyleToken_Comma       = 44,
    StyleToken_Period      = 46,
    StyleToken_SemiColon   = 59,
    StyleToken_AtSymbol    = 64,
    StyleToken_OpenBrace   = 123,
    StyleToken_CloseBrace  = 125,
    StyleToken_Identifier  = 256,
    StyleToken_Assignment  = 257,
    StyleToken_Unit        = 258,
    StyleToken_String      = 259,
    StyleToken_Vector      = 260,
    StyleToken_Style       = 261,
    StyleToken_Var         = 262,
} StyleToken_Type;

// TODO: Move these to respect context.
enum StyleParser_Constant
{
    StyleParser_MaximumTokenPerFile = 100000,
    StyleParser_MaximumFileSize     = VOID_GIGABYTE(1),
    StyleParser_MaximumVarPerFile   = 1024,
    StyleParser_VarHashEntryPerFile = 256,
};

constexpr uint32_t InvalidStyleTableResult = VOID_BIT(32);

typedef struct style_parser_table_entry
{
    byte_string Name;
    uint32_t         Value;
} style_parser_table_entry;

const static style_parser_table_entry StyleKeywordTable[] =
{
    {byte_string_compile("style"), StyleToken_Style},
    {byte_string_compile("var")  , StyleToken_Var  },
};

const static style_parser_table_entry StyleStateTable[] =
{
    {byte_string_compile("default"), StyleState_Default},
    {byte_string_compile("hovered"), StyleState_Hovered},
    {byte_string_compile("focused"), StyleState_Focused},
};

const static style_parser_table_entry StylePropertyTable[] =
{
    {byte_string_compile("size")        , StyleProperty_Size        },
    {byte_string_compile("color")       , StyleProperty_Color       },
    {byte_string_compile("padding")     , StyleProperty_Padding     },
    {byte_string_compile("spacing")     , StyleProperty_Spacing     },
    {byte_string_compile("softness")    , StyleProperty_Softness    },
    {byte_string_compile("borderwidth") , StyleProperty_BorderWidth },
    {byte_string_compile("bordercolor") , StyleProperty_BorderColor },
    {byte_string_compile("cornerradius"), StyleProperty_CornerRadius},

    // Layout Properties
    {byte_string_compile("display"), StyleProperty_Display},

    // Text Properties
    {byte_string_compile("font")        , StyleProperty_Font      },
    {byte_string_compile("font-size")   , StyleProperty_FontSize  },
    {byte_string_compile("text-align-x"), StyleProperty_XTextAlign},
    {byte_string_compile("text-align-y"), StyleProperty_YTextAlign},
    {byte_string_compile("text-color")  , StyleProperty_TextColor },
    {byte_string_compile("caret-color") , StyleProperty_CaretColor},
    {byte_string_compile("caret-width") , StyleProperty_CaretWidth},

    // Flex Properties
    {byte_string_compile("flex-direction") , StyleProperty_FlexDirection },
    {byte_string_compile("justify-content"), StyleProperty_JustifyContent},
    {byte_string_compile("align-items")    , StyleProperty_AlignItems    },
    {byte_string_compile("self-align")     , StyleProperty_SelfAlign     },
    {byte_string_compile("flex-grow")      , StyleProperty_FlexGrow      },
    {byte_string_compile("flex-shrink")    , StyleProperty_FlexShrink    },
};

const static style_parser_table_entry StyleUnitKeywordTable[] =
{
    {byte_string_compile("auto"), UIUnit_Auto},
};

const static style_parser_table_entry StyleDisplayTable[] =
{
    {byte_string_compile("none")  , UIDisplay_None  },
    {byte_string_compile("normal"), UIDisplay_Normal},
    {byte_string_compile("flex")  , UIDisplay_Flex  },
};

const static style_parser_table_entry FlexDirectionTable[] =
{
    {byte_string_compile("row")   , UIFlexDirection_Row   },
    {byte_string_compile("column"), UIFlexDirection_Column},
};

const static style_parser_table_entry JustifyContentTable[] =
{
    {byte_string_compile("start")        , UIJustifyContent_Start       },
    {byte_string_compile("center")       , UIJustifyContent_Center      },
    {byte_string_compile("end")          , UIJustifyContent_End         },
    {byte_string_compile("space-between"), UIJustifyContent_SpaceBetween},
    {byte_string_compile("space-around") , UIJustifyContent_SpaceAround },
    {byte_string_compile("space-evenly") , UIJustifyContent_SpaceEvenly },
};

const static style_parser_table_entry AlignItemsTable[] =
{
    {byte_string_compile("start")  , UIAlignItems_Start  },
    {byte_string_compile("center") , UIAlignItems_Center },
    {byte_string_compile("end")    , UIAlignItems_End    },
    {byte_string_compile("stretch"), UIAlignItems_Stretch},
};

const static style_parser_table_entry SelfAlignTable[] =
{
    {byte_string_compile("start")  , UIAlignItems_Start  },
    {byte_string_compile("center") , UIAlignItems_Center },
    {byte_string_compile("end")    , UIAlignItems_End    },
    {byte_string_compile("stretch"), UIAlignItems_Stretch},
    {byte_string_compile("none")   , UIAlignItems_None  },
};

const static style_parser_table_entry TextAlignTable[] =
{
    {byte_string_compile("start")  , UIAlign_Start  },
    {byte_string_compile("center") , UIAlign_Center },
    {byte_string_compile("end")    , UIAlign_End    },
};

static uint32_t
FindEnumFromStyleTable(byte_string Name, const style_parser_table_entry *Table, uint32_t TableSize)
{
    uint32_t Result = InvalidStyleTableResult;

    for (uint32_t Idx = 0; Idx < TableSize; ++Idx)
    {
        style_parser_table_entry Entry = Table[Idx];
        if (ByteStringMatches(Name, Entry.Name, 0))
        {
            Result = Entry.Value;
            return Result;
        }
    }

    return Result;
}

// ----------------------------------------------------------------------------------
// Style Parser Error Handling

typedef struct style_file_debug_info
{
    byte_string  FileName;
    os_read_file FileContent;

    uint32_t CurrentLineInFile;

    uint32_t ErrorCount;
    uint32_t WarningCount;
} style_file_debug_info;

static void
ReportStyleParserError(style_file_debug_info *Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console)
{
    if (Severity == ConsoleMessage_Error)
    {
        Debug->ErrorCount++;
    }
    else if (Severity == ConsoleMessage_Warn)
    {
        Debug->WarningCount++;
    }

    ConsoleWriteMessage(Severity, Message, Console);
}

static void
ReportStyleFileError(style_file_debug_info *Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console)
{
    uint8_t          Buffer[VOID_KILOBYTE(4)] = { 0 };
    byte_string Error = ByteString(Buffer, 0);

    Error.Size += snprintf((char *)Error.String, sizeof(Buffer), "[File %s | Line: %u]", Debug->FileName.String, Debug->CurrentLineInFile);
    Error.Size += snprintf((char *)Error.String + Error.Size, sizeof(Buffer), "[%s]", Message.String);

    ReportStyleParserError(Debug, Severity, Error, Console);
}

// ---------------------------------------------------------------------------------
// Style Tokenizer Implementation

typedef struct style_vector
{
    vec4_unit V;
    uint32_t       Size;
} style_vector;

typedef struct style_token
{
    uint64_t LineInFile;
    uint64_t ByteInFile;

    StyleToken_Type Type;
    union
    {
        ui_unit      Unit;
        byte_string  Identifier;
        style_vector Vector;
    };
} style_token;

typedef struct style_token_buffer
{
    style_token *Tokens;
    uint64_t          At;
    uint64_t          Count;
    uint64_t          Size;
} style_token_buffer;

typedef struct tokenized_style_file
{
    bool                CanBeParsed;
    uint32_t                StylesCount;
    style_token_buffer Buffer;
} tokenized_style_file;

static ui_color
ToNormalizedColor(vec4_unit Vec)
{
    float      Inverse = 1.f / 255;
    ui_color Result  = UIColor(Vec.X.Float32 * Inverse, Vec.Y.Float32 * Inverse, Vec.Z.Float32 * Inverse, Vec.W.Float32 * Inverse);
    return Result;
}

static style_token *
GetStyleToken(style_token_buffer *Buffer, long long Index)
{
    style_token *Result = 0;

    if (Index < Buffer->Size)
    {
        Result = Buffer->Tokens + Index;
    }

    return Result;
}

static style_token *
EmitStyleToken(style_token_buffer *Buffer, StyleToken_Type Type, uint64_t AtLine, uint64_t AtByte)
{
    style_token *Result = GetStyleToken(Buffer, Buffer->Count++);

    if(Result)
    {
        Result->ByteInFile = AtByte;
        Result->LineInFile = AtLine;
        Result->Type       = Type;
    }

    return Result;
}

static void
EatStyleToken(style_token_buffer *Buffer, uint32_t Count)
{
    if (Buffer->At + Count < Buffer->Count)
    {
        Buffer->At += Count;
    }

    if(Buffer->At > Buffer->Count)
    {
        Buffer->At = Buffer->Count;
    }
}

static style_token *
PeekStyleToken(style_token_buffer *Buffer, int Offset)
{
    style_token *Result = GetStyleToken(Buffer, (Buffer->At - 1));

    long long Index = Buffer->At + Offset;
    if (Index >= 0 && Index < Buffer->Count)
    {
        Result = GetStyleToken(Buffer, Index);
    }

    return Result;
}

static bool
ExpectStyleToken(style_token_buffer *Buffer, StyleToken_Type Type)
{
    style_token *Token = PeekStyleToken(Buffer, 0);

    if(Token->Type == Type)
    {
        EatStyleToken(Buffer, 1);
        return 1;
    }

    return 0;
}

static style_token *
ConsumeStyleToken(style_token_buffer *Buffer, StyleToken_Type Type, byte_string ErrorMessage, style_file_debug_info *Debug)
{
    style_token *Token = PeekStyleToken(Buffer, 0);

    if(Token->Type == Type)
    {
        EatStyleToken(Buffer, 1);
        return Token;
    }

    ReportStyleFileError(Debug, ConsoleMessage_Error, ErrorMessage, &UIState.Console);
    return 0;
}

static bool
MatchStyleToken(style_token_buffer *Buffer, StyleToken_Type Type)
{
    style_token *Token = PeekStyleToken(Buffer, 0);
    return Token->Type == Type;
}

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
ReadUnit(os_read_file *File, style_file_debug_info *Debug)
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
        byte_string Identifier = ReadIdentifier(File);

        Result.Type = (UIUnit_Type)FindEnumFromStyleTable(Identifier, StyleUnitKeywordTable, VOID_ARRAYCOUNT(StyleUnitKeywordTable));
        if(Result.Type == UIUnit_None)
        {
            ReportStyleFileError(Debug, error_message("Found invalid identifier in file."));
        }
    } else
    {
        ReportStyleFileError(Debug, error_message("Could not parse identifier."));
    }

    return Result;
}

static style_vector
ReadVector(os_read_file *File, style_file_debug_info *Debug)
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

static tokenized_style_file
TokenizeStyleFile(os_read_file File, memory_arena *Arena, style_file_debug_info *Debug)
{
    tokenized_style_file Result = {};
    Result.Buffer.Tokens = PushArray(Arena, style_token, StyleParser_MaximumTokenPerFile);
    Result.Buffer.Size   = StyleParser_MaximumTokenPerFile;

    while (IsValidFile(&File))
    {
        SkipWhiteSpaces(&File);

        uint8_t  Char   = PeekFile(&File, 0);
        uint64_t AtLine = Debug->CurrentLineInFile;
        uint64_t AtByte = File.At;

        if (IsAlpha(Char))
        {
            byte_string Identifier = ReadIdentifier(&File);
            if(IsValidByteString(Identifier))
            {
                style_token *Token = 0;

                uint32_t TokenType = FindEnumFromStyleTable(Identifier, StyleKeywordTable, VOID_ARRAYCOUNT(StyleKeywordTable));
                if(TokenType != InvalidStyleTableResult)
                {
                    Token = EmitStyleToken(&Result.Buffer, (StyleToken_Type)TokenType, AtLine, AtByte);
                }

                if(Token == 0)
                {
                    Token = EmitStyleToken(&Result.Buffer, StyleToken_Identifier, AtLine, AtByte);
                    if(Token)
                    {
                        Token->Identifier = Identifier;
                    }
                }
                else if(Token->Type == StyleToken_Style)
                {
                    ++Result.StylesCount;
                }
            }

            continue;
        }

        if (IsDigit(Char))
        {
            style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_Unit, AtLine, AtByte);
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

            ++Debug->CurrentLineInFile;

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(&File, 1) == '=')
            {
                EmitStyleToken(&Result.Buffer, StyleToken_Assignment, AtLine, AtByte);
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

            style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_Vector, AtLine, AtByte);
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
                    style_token *Token = EmitStyleToken(&Result.Buffer, StyleToken_String, AtLine, AtByte);
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
            EmitStyleToken(&Result.Buffer, (StyleToken_Type)Char, AtLine, AtByte);

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

    style_token *EndOfFile = EmitStyleToken(&Result.Buffer, StyleToken_EndOfFile, 0, 0);

    Result.CanBeParsed = (EndOfFile && 1);

    return Result;
}

//-----------------------------------------------------------------------------------
// Style Variables Implementation

typedef struct style_var_hash
{
    uint64_t Value;
} style_var_hash;

typedef struct style_var_entry
{
    style_var_hash Hash;
    uint32_t            NextWithSameHash;

    bool          ValueIsParsed;
    style_token *ValueToken;
} style_var_entry;

typedef struct style_var_table_params
{
    uint32_t HashCount;
    uint32_t EntryCount;
} style_var_table_params;

typedef struct style_var_table
{
    uint32_t HashMask;
    uint32_t HashCount;
    uint32_t EntryCount;

    uint32_t             *HashTable;
    style_var_entry *Entries;
} style_var_table;

typedef struct style_variable
{
    bool          IsValid;
    byte_string  Name;
    uint32_t          LineInFile;
    style_token *ValueToken;
} style_variable;

static style_var_entry *
GetStyleVarEntry(uint32_t Index, style_var_table *Table)
{
    VOID_ASSERT(Index < Table->EntryCount);

    style_var_entry *Result = Table->Entries + Index;
    return Result;
}

static style_var_entry *
GetStyleVarSentinel(style_var_table *Table)
{
    style_var_entry *Result = Table->Entries;
    return Result;
}

static style_var_table *
PlaceStyleVarTableInMemory(style_var_table_params Params, void *Memory)
{
    VOID_ASSERT(Params.EntryCount > 0);
    VOID_ASSERT(Params.HashCount > 0);
    VOID_ASSERT(VOID_ISPOWEROFTWO(Params.HashCount));

    style_var_table *Result = 0;

    if (Memory)
    {
        Result = (style_var_table *)Memory;
        Result->HashTable = (uint32_t *)(Result + 1);
        Result->Entries = (style_var_entry *)(Result->HashTable + Params.HashCount);

        Result->HashMask = Params.HashCount - 1;
        Result->HashCount = Params.HashCount;
        Result->EntryCount = Params.EntryCount;

        MemorySet(Result->HashTable, 0, Result->HashCount * sizeof(Result->HashTable[0]));

        for (uint32_t Idx = 0; Idx < Params.EntryCount; Idx++)
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

            Entry->ValueIsParsed = 0;
        }
    }

    return Result;
}

static uint32_t
PopFreeStyleVarEntry(style_var_table *Table)
{
    style_var_entry *Sentinel = GetStyleVarSentinel(Table);

    if (!Sentinel->NextWithSameHash)
    {
        return 0;
    }

    uint32_t              Result = Sentinel->NextWithSameHash;
    style_var_entry *Entry = GetStyleVarEntry(Result, Table);

    Sentinel->NextWithSameHash = Entry->NextWithSameHash;
    Entry->NextWithSameHash = 0;

    return Result;
}

static style_var_entry *
FindStyleVarEntry(style_var_hash Hash, style_var_table *Table)
{
    uint32_t HashSlot = Hash.Value & Table->HashMask;
    uint32_t EntryIndex = Table->HashTable[HashSlot];

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
            Result->Hash = Hash;

            Table->HashTable[HashSlot] = EntryIndex;
        }
    }

    return Result;
}

static style_var_hash
HashStyleVar(byte_string Name)
{
    style_var_hash Result = { HashByteString(Name) };
    return Result;
}

static size_t
GetStyleVarTableFootprint(style_var_table_params Params)
{
    size_t HashSize = Params.HashCount * sizeof(uint32_t);
    size_t EntrySize = Params.EntryCount * sizeof(style_var_entry);
    size_t Result = sizeof(style_var_table) + HashSize + EntrySize;

    return Result;
}

static style_variable
ParseStyleVariable(style_token_buffer *Buffer, style_file_debug_info *Debug)
{
    style_variable Variable = { 0 };

    if (!ExpectStyleToken(Buffer, StyleToken_Var))
    {
        return Variable;
    }

    style_token *Name = ConsumeStyleToken(Buffer, StyleToken_Identifier, byte_string_literal("Expected a name for variable. Name must be [a..z][A..Z][_][-]"), Debug);
    if (Name)
    {
        Variable.Name = Name->Identifier;
    }
    else
    {
        return Variable;
    }

    if (!ConsumeStyleToken(Buffer, StyleToken_Assignment, byte_string_literal("Expected an assignment"), Debug))
    {
        return Variable;
    }

    style_token *Value = PeekStyleToken(Buffer, 0);
    if (Value->Type == StyleToken_Unit || Value->Type == StyleToken_String || Value->Type == StyleToken_Vector)
    {
        Variable.ValueToken = Value;
        EatStyleToken(Buffer, 1);
    }
    else
    {
        ReportStyleFileError(Debug, error_message("Expected a value (unit, string, or vector)"));
        return Variable;
    }

    if (!ConsumeStyleToken(Buffer, StyleToken_SemiColon, byte_string_literal("Expected a ;"), Debug))
    {
        return Variable;
    }

    Variable.IsValid = 1;

    return Variable;
}

//-----------------------------------------------------------------------------------

static StyleState_Type
ParseStyleState(style_token_buffer *Buffer, style_file_debug_info *Debug)
{
    StyleState_Type Result = StyleState_None;

    if(!ExpectStyleToken(Buffer, StyleToken_AtSymbol))
    {
        return Result;
    }

    style_token *Name = ConsumeStyleToken(Buffer, StyleToken_Identifier, byte_string_literal("Expected Default OR Hover OR Focus after using @"), Debug);
    if(Name)
    {
        uint32_t Enum = FindEnumFromStyleTable(Name->Identifier, StyleStateTable, VOID_ARRAYCOUNT(StyleStateTable));
        if(Enum != InvalidStyleTableResult)
        {
            Result = (StyleState_Type)Enum;
        }
    }

    return Result;
}

// ---------------------------------------------------------------------------------
// Style Attribute Parser Implementation
//
// An attribute should look like:
// Name := Value;
//
// Name must be inside this table ->
// Value must abide to the rules specific to that attribute.
// Attributes are stored inside style blocks. See Style Blocks for more information.

static style_property
ConvertToStyleProperty(style_token *Value, StyleProperty_Type PropType, style_file_debug_info *Debug)
{
    style_property Result = {};
    Result.IsSet = 1;
    Result.Type  = PropType;

    byte_string ErrorMessage = ByteString(0, 0);

    switch (PropType)
    {

    case StyleProperty_Size:
    {
        if (Value->Type != StyleToken_Vector || Value->Vector.Size != 2)
        {
            ErrorMessage = byte_string_literal("Size expects a vector with 2 values [Width, Height]");
            break;
        }

        Result.Vec2.X = Value->Vector.V.X;
        Result.Vec2.Y = Value->Vector.V.Y;
    } break;

    case StyleProperty_Padding:
    case StyleProperty_CornerRadius:
    {
        if (Value->Type != StyleToken_Vector || Value->Vector.Size != 4)
        {
            ErrorMessage = byte_string_literal("Padding OR Corner Radius expects a vector with 4 values [Left, Top, Right, Bottom]");
            break;
        }

        vec4_unit Vector = Value->Vector.V;

        if (Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32 ||
            Vector.Z.Type != UIUnit_Float32 || Vector.W.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Padding OR Corner Radius values must be floats");
            break;
        }

        if(PropType == StyleProperty_Padding)
        {
            Result.Padding.Left  = Vector.X.Float32;
            Result.Padding.Top   = Vector.Y.Float32;
            Result.Padding.Right = Vector.Z.Float32;
            Result.Padding.Bot   = Vector.W.Float32;
        } else
        if(PropType == StyleProperty_CornerRadius)
        {
            Result.CornerRadius.TopLeft  = Vector.X.Float32;
            Result.CornerRadius.TopRight = Vector.Y.Float32;
            Result.CornerRadius.BotRight = Vector.Z.Float32;
            Result.CornerRadius.BotLeft  = Vector.W.Float32;
        }
        else
        {
            VOID_ASSERT(!"Not Possible");
        }
    } break;

    case StyleProperty_Spacing:
    {
        if (Value->Type != StyleToken_Vector || Value->Vector.Size != 2)
        {
            ErrorMessage = byte_string_literal("Spacing expects a vector with 2 values [Horizontal, Vertical]");
            break;
        }

        vec4_unit Vector = Value->Vector.V;

        if (Vector.X.Type != UIUnit_Float32 || Vector.Y.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Spacing values must be floats");
            break;
        }

        Result.Spacing.Horizontal = Vector.X.Float32;
        Result.Spacing.Vertical   = Vector.Y.Float32;
    } break;

    case StyleProperty_Color:
    case StyleProperty_TextColor:
    case StyleProperty_BorderColor:
    case StyleProperty_CaretColor:
    {
        if (Value->Type != StyleToken_Vector || Value->Vector.Size != 4)
        {
            ErrorMessage = byte_string_literal("Color expects a vector with 4 values [R, G, B, A]");
            break;
        }

        vec4_unit V = Value->Vector.V;
        if (V.X.Type != UIUnit_Float32 || !IsInRangefloat(0.f, 255.f, V.X.Float32) ||
            V.Y.Type != UIUnit_Float32 || !IsInRangefloat(0.f, 255.f, V.Y.Float32) ||
            V.Z.Type != UIUnit_Float32 || !IsInRangefloat(0.f, 255.f, V.Z.Float32) ||
            V.W.Type != UIUnit_Float32 || !IsInRangefloat(0.f, 255.f, V.W.Float32))
        {
            ErrorMessage = byte_string_literal("Color values must be floats in range [0, 255]");
            break;
        }

        Result.Color = ToNormalizedColor(V);
    } break;

    case StyleProperty_BorderWidth:
    case StyleProperty_FontSize:
    case StyleProperty_Softness:
    case StyleProperty_FlexGrow:
    case StyleProperty_FlexShrink:
    case StyleProperty_CaretWidth:
    {
        if (Value->Type != StyleToken_Unit || Value->Unit.Type != UIUnit_Float32)
        {
            ErrorMessage = byte_string_literal("Expected a numeric value");
            break;
        }

        if (Value->Unit.Float32 <= 0)
        {
            ErrorMessage = byte_string_literal("Value must be greater than 0");
            break;
        }

        Result.Float32 = Value->Unit.Float32;
    } break;

    case StyleProperty_Font:
    {
        if (Value->Type != StyleToken_String || !IsValidByteString(Value->Identifier))
        {
            ErrorMessage = byte_string_literal("Font expects a string value");
            break;
        }

        Result.String = Value->Identifier;
    } break;

    case StyleProperty_Display:
    case StyleProperty_SelfAlign:
    case StyleProperty_XTextAlign:
    case StyleProperty_YTextAlign:
    case StyleProperty_AlignItems:
    case StyleProperty_FlexDirection:
    case StyleProperty_JustifyContent:
    {
        if (Value->Type != StyleToken_String || !IsValidByteString(Value->Identifier))
        {
            ErrorMessage = byte_string_literal("Expected a string value");
            break;
        }

        const style_parser_table_entry *Table     = 0;
        uint32_t                                 TableSize = 0;

        if(PropType == StyleProperty_Display)
        {
            Table     = StyleDisplayTable;
            TableSize = VOID_ARRAYCOUNT(StyleDisplayTable);
        } else
        if(PropType == StyleProperty_SelfAlign)
        {
            Table     = SelfAlignTable;
            TableSize = VOID_ARRAYCOUNT(SelfAlignTable);
        } else
        if(PropType == StyleProperty_XTextAlign || PropType == StyleProperty_YTextAlign)
        {
            Table     = TextAlignTable;
            TableSize = VOID_ARRAYCOUNT(TextAlignTable);
        } else
        if(PropType == StyleProperty_AlignItems)
        {
            Table     = AlignItemsTable;
            TableSize = VOID_ARRAYCOUNT(AlignItemsTable);
        } else
        if(PropType == StyleProperty_FlexDirection)
        {
            Table     = FlexDirectionTable;
            TableSize = VOID_ARRAYCOUNT(FlexDirectionTable);
        } else
        if(PropType == StyleProperty_JustifyContent)
        {
            Table     = JustifyContentTable;
            TableSize = VOID_ARRAYCOUNT(JustifyContentTable);
        }

        VOID_ASSERT(Table);
        VOID_ASSERT(TableSize);

        Result.Enum = FindEnumFromStyleTable(Value->Identifier, Table, TableSize);
        if(Result.Enum == InvalidStyleTableResult)
        {
            ErrorMessage = byte_string_literal("Invalid value for property. Check documentation for valid options");
            break;
        }
    } break;

    default:
    {
        VOID_ASSERT(!"Forgot to handle property type");
    } break;

    }

    if (IsValidByteString(ErrorMessage))
    {
        ReportStyleFileError(Debug, error_message(ErrorMessage.String));
        Result.IsSet = 0;
    }

    return Result;
}

static style_property
ParseStyleProperty(style_token_buffer *Buffer, style_var_table *VarTable, style_file_debug_info *Debug)
{
    style_property Property = {};

    style_token *Name = ConsumeStyleToken(Buffer, StyleToken_Identifier, byte_string_literal("Expected an attribute name."), Debug);
    if(Name)
    {
        uint32_t PropertyType = FindEnumFromStyleTable(Name->Identifier, StylePropertyTable, VOID_ARRAYCOUNT(StylePropertyTable));
        if(PropertyType == InvalidStyleTableResult)
        {
            ReportStyleFileError(Debug, error_message("Invalid attribute name"));
            return Property;
        }

        Property.Type = (StyleProperty_Type)PropertyType;
    }
    else
    {
        return Property;
    }

    if(!ConsumeStyleToken(Buffer, StyleToken_Assignment, byte_string_literal("Expected an assignment ':='"), Debug))
    {
        return Property ;
    }

    style_token *Value = PeekStyleToken(Buffer, 0);
    if(MatchStyleToken(Buffer, StyleToken_Identifier))
    {
        style_var_hash   Hash  = HashStyleVar(Value->Identifier);
        style_var_entry *Entry = FindStyleVarEntry(Hash, VarTable);

        if(Entry && Entry->ValueIsParsed)
        {
            Value = Entry->ValueToken;
        }
        else
        {
            ReportStyleFileError(Debug, error_message("Undefined variable."));
        }
    }
    EatStyleToken(Buffer, 1);

    if(!ConsumeStyleToken(Buffer, StyleToken_SemiColon, byte_string_literal("Expected a semicolon"), Debug))
    {
        return Property;
    }

    VOID_ASSERT(Value);
    Property = ConvertToStyleProperty(Value, Property.Type, Debug);

    return Property;
}

// ---------------------------------------------------------------------------------
// Style Block Parser Implementation
//
// [Header]              -> Parsed before this
// {                     -> Opening Brace
//    @Effect            -> Effects
//     ...               -> Property List
// }                     -> Closing Brace
//
// Implemented as a state machine that simply controls the flow. Does not parse
// the attributes itself (Calls It).

typedef enum ParseBlock_State
{
    ParseBlock_ExpectOpenBrace,
    ParseBlock_ExpectEffect,
    ParseBlock_InEffect,
    ParseBlock_Done,
    ParseBlock_Error,
} ParseBlock_State;

typedef struct style_block
{
    uint32_t            PropsCount;
    style_property Props[StyleState_Count][StyleProperty_Count];
} style_block;

// BUG:
// These look bugged.

static void
SynchronizeToNextEffect(style_token_buffer *Buffer)
{
    while(!MatchStyleToken(Buffer, StyleToken_AtSymbol) && !MatchStyleToken(Buffer, StyleToken_CloseBrace) && !MatchStyleToken(Buffer, StyleToken_EndOfFile))
    {
        EatStyleToken(Buffer, 1);
    }
}

static void
SynchronizeToNextProperty(style_token_buffer *Buffer)
{
    while(!MatchStyleToken(Buffer, StyleToken_SemiColon) && !MatchStyleToken(Buffer, StyleToken_CloseBrace) && !MatchStyleToken(Buffer, StyleToken_EndOfFile))
    {
        EatStyleToken(Buffer, 1);
    }

    // NOTE: Not clear.
    if (MatchStyleToken(Buffer, StyleToken_SemiColon))
    {
        EatStyleToken(Buffer, 1);
    }
}

static style_block
ParseStyleBlock(style_token_buffer *Buffer, style_var_table *VarTable, style_file_debug_info *Debug)
{
    style_block Result = {};

    if(!ConsumeStyleToken(Buffer, StyleToken_OpenBrace, byte_string_literal("Expected a { after style header."), Debug))
    {
        return Result;
    }

    ParseBlock_State State        = ParseBlock_ExpectEffect;
    StyleState_Type  CurrentState = StyleState_Default;

    while(State != ParseBlock_Done && State != ParseBlock_Error)
    {
        if(MatchStyleToken(Buffer, StyleToken_EndOfFile))
        {
            ReportStyleFileError(Debug, error_message("Unexpected End Of File"));
            State = ParseBlock_Error;
            break;
        }

        if(ExpectStyleToken(Buffer, StyleToken_CloseBrace))
        {
            State = ParseBlock_Done;
            break;
        }

        switch(State)
        {

        case ParseBlock_ExpectEffect:
        {
            StyleState_Type StyleState = ParseStyleState(Buffer, Debug);
            if(StyleState != StyleState_None)
            {
                CurrentState = StyleState;
            }

            State = ParseBlock_InEffect;
        } break;

        case ParseBlock_InEffect:
        {
            if(MatchStyleToken(Buffer, StyleToken_AtSymbol))
            {
                State = ParseBlock_ExpectEffect;
                continue;
            }

            style_property Property = ParseStyleProperty(Buffer, VarTable, Debug);
            if(!Property.IsSet)
            {
                SynchronizeToNextProperty(Buffer);
                continue;
            }

            if(!Result.Props[CurrentState][Property.Type].IsSet)
            {
                Result.Props[CurrentState][Property.Type] = Property;
                Result.PropsCount++; // NOTE: Why?
            }
            else
            {
                ReportStyleFileError(Debug, warn_message("Attribute already set for this effect"));
            }
        } break;

        default:
        {
            VOID_ASSERT(!"Invalid Parser State");
        } break;

        }
    }

    return Result;
}

// -------------------------------------------------------------------------------
// Style Header Implementation

typedef struct style_header
{
    bool         HadError;
    byte_string StyleName;
} style_header;

static style_header
ParseStyleHeader(style_token_buffer *Buffer, style_file_debug_info *Debug)
{
    style_header Header = {};

    if(!ConsumeStyleToken(Buffer, StyleToken_Style, byte_string_literal("Expected 'style' keyword"), Debug))
    {
        Header.HadError = 1;
        return Header;
    }

    style_token *Name = ConsumeStyleToken(Buffer, StyleToken_String, byte_string_literal("Expected style name as string"), Debug);
    if(!Name)
    {
        Header.HadError = 1;
        return Header;
    }

    Header.StyleName = Name->Identifier;
    return Header;
}

// -------------------------------------------------------------------------------
//

typedef struct style
{
    style_header Header;
    style_block  Block;
} style;

static bool
IsPropertySet(ui_cached_style *Style, StyleState_Type State, StyleProperty_Type Property)
{
    bool Result = Style->Properties[State][Property].IsSet;
    return Result;
}

static void
CacheStyle(style *ParsedStyle, ui_style_registry *Registry, style_file_debug_info *Debug)
{
    VOID_ASSERT(Registry->StylesCount < Registry->StylesCapacity);

    ui_cached_style *CachedStyle = Registry->Styles + Registry->StylesCount;
    style_header    *Header      = &ParsedStyle->Header;
    style_block     *Block       = &ParsedStyle->Block;

    CachedStyle->CachedIndex = ++Registry->StylesCount; // 0 is a reserved spot

    // NOTE:
    // Is this code even useful then? Not really, apart from font loading?

    ForEachEnum(StyleState_Type, StyleState_Count, State)
    {
        ForEachEnum(StyleProperty_Type, StyleProperty_Count, Property)
        {
            CachedStyle->Properties[State][Property] = Block->Props[State][Property];
        }

        // Load Fonts - Why are they not lazy loaded?

        if(IsPropertySet(CachedStyle, State, StyleProperty_Font))
        {
            byte_string Name = Block->Props[State][StyleProperty_Font].String;

            if(IsPropertySet(CachedStyle, State, StyleProperty_FontSize))
            {
                style_property *Props = GetCachedProperties(CachedStyle->CachedIndex, StyleState_Default, Registry);
                float Size = UIGetFontSize(Props);

                ui_font *Font = UIQueryFont(Name, Size);
                if(!Font)
                {
                    Font = UILoadFont(Name, Size);
                }

                CachedStyle->Properties[State][StyleProperty_Font].Pointer = Font;
            }
        }
    }

    VOID_UNUSED(Header); // NOTE: VOID_UNUSED at the moment. Unsure where I wanna go with this.
}

static ui_style_registry *
ParseStyleFile(tokenized_style_file *File, memory_arena *RoutineArena, memory_arena *OutputArena, style_file_debug_info *Debug)
{
    ui_style_registry *Result = PushStruct(OutputArena, ui_style_registry);

    if(Result)
    {
        Result->StylesCount    = 0;
        Result->StylesCapacity = File->StylesCount;
        Result->Styles         = PushArray(OutputArena, ui_cached_style, File->StylesCount);

        style_var_table *VarTable = 0;
        {
            style_var_table_params Params = {};
            Params.EntryCount = StyleParser_MaximumVarPerFile;
            Params.HashCount  = StyleParser_VarHashEntryPerFile;

            size_t Footprint      = GetStyleVarTableFootprint(Params);
            uint8_t    *VarTableMemory = PushArray(RoutineArena, uint8_t, Footprint);

            VarTable = PlaceStyleVarTableInMemory(Params, VarTableMemory);
        }

        if(VarTable)
        {
            style_token *Next = PeekStyleToken(&File->Buffer, 0);
            while (Next->Type != StyleToken_EndOfFile)
            {
                style_variable Variable = ParseStyleVariable(&File->Buffer, Debug);
                while (Variable.IsValid)
                {
                    style_var_hash   Hash  = HashStyleVar(Variable.Name);
                    style_var_entry *Entry = FindStyleVarEntry(Hash, VarTable);
                    if(!Entry->ValueIsParsed)
                    {
                        Entry->ValueToken    = Variable.ValueToken;
                        Entry->ValueIsParsed = 1;
                    }
                    else
                    {
                        ReportStyleFileError(Debug, error_message("Two variables cannot have the same name"));
                    }

                    Variable = ParseStyleVariable(&File->Buffer, Debug);
                };

                style Style = {};
                Style.Header = ParseStyleHeader(&File->Buffer, Debug);
                Style.Block  = ParseStyleBlock(&File->Buffer, VarTable, Debug);

                CacheStyle(&Style, Result, Debug);

                Next = PeekStyleToken(&File->Buffer, 0);
            }
        }
        else
        {
            ReportStyleParserError(Debug, error_message("Could not allocate style_var_table."));
        }
    }
    else
    {
        ReportStyleParserError(Debug, error_message("Could not allocate registry."));
    }

    return Result;
}

// ----------------------------------------------------------------------------------
// Public API Implementation

static ui_style_registry *
CreateStyleRegistry(byte_string FileName, memory_arena *OutputArena)
{
    ui_style_registry *Result = 0;

    memory_arena *RoutineArena = {};
    {
        memory_arena_params Params = {};
        Params.AllocatedFromLine = __LINE__;
        Params.AllocatedFromFile = __FILE__;
        Params.ReserveSize       = StyleParser_MaximumFileSize + (StyleParser_MaximumTokenPerFile * sizeof(style_token));
        Params.CommitSize        = ArenaDefaultCommitSize;

        RoutineArena = AllocateArena(Params);
    }

    if(!RoutineArena)
    {
        return Result;
    }

    os_handle FileHandle = OSFindFile(FileName);
    if(!OSIsValidHandle(FileHandle))
    {
        return Result;
    }

    uint64_t FileSize = OSFileSize(FileHandle);
    if(FileSize > StyleParser_MaximumFileSize)
    {
        return Result;
    }

    os_read_file File = OSReadFile(FileHandle, RoutineArena);
    if(!File.FullyRead)
    {
        return Result;
    }

    style_file_debug_info Debug = {};
    Debug.FileContent = File;
    Debug.FileName    = FileName;

    tokenized_style_file TokenizedFile = TokenizeStyleFile(File, RoutineArena, &Debug);
    if(TokenizedFile.CanBeParsed)
    {
        Result = ParseStyleFile(&TokenizedFile, RoutineArena, OutputArena, &Debug);
    }

    OSReleaseFile(FileHandle);

    return Result;
}
