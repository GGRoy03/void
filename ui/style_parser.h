#pragma once

// [Enums]

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

enum StyleParser_Constant
{
    StyleParser_MaximumTokenPerFile = 100000,
    StyleParser_MaximumFileSize     = Gigabyte(1),
    StyleParser_MaximumVarPerFile   = 1024,
    StyleParser_VarHashEntryPerFile = 256,
};

// Basic tokens.

typedef struct style_vector
{
    vec4_unit V;
    u32       Size;
} style_vector;

typedef struct style_token
{
    u64 LineInFile;
    u64 ByteInFile;

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
    u64          At;
    u64          Count;
    u64          Size;
} style_token_buffer;

// Parsing Context

typedef struct style_file_debug_info
{
    byte_string  FileName;
    os_read_file FileContent;

    u32 CurrentLineInFile;

    u32 ErrorCount;
    u32 WarningCount;
} style_file_debug_info;

typedef struct tokenized_style_file
{
    b32                CanBeParsed;
    u32                StylesCount;
    style_token_buffer Buffer;
} tokenized_style_file;

// Style Primitives
// The diffrent primitives that compose a style file.
// Only used in the context of the parser.

typedef struct style_header
{
    b32         HadError;
    byte_string StyleName;
} style_header;

typedef struct style_attribute
{
    b32                IsSet;
    u32                HadError;
    u32                LineInFile;
    StyleProperty_Type PropertyType;
    StyleToken_Type    ParsedAs;
    union
    {
        ui_unit      Unit;
        byte_string  String;
        style_vector Vector;
    };
} style_attribute;

typedef struct style_variable
{
    b32          IsValid;
    byte_string  Name;
    u32          LineInFile;
    style_token *ValueToken;
} style_variable;

typedef struct style_effect
{
    StyleState_Type Type;
} style_effect;

typedef struct style_block
{
    u32             AttributesCount;
    style_attribute Attributes[StyleState_Count][StyleProperty_Count];
} style_block;

typedef struct style
{
    style_header Header;
    style_block  Block;
} style;

// Variables
// Implemented as a simple linked list hashmap. Per file.

typedef struct style_var_hash
{
    u64 Value;
} style_var_hash;

typedef struct style_var_entry
{
    style_var_hash Hash;
    u32            NextWithSameHash;

    b32          ValueIsParsed;
    style_token *ValueToken;
} style_var_entry;

typedef struct style_var_table_params
{
    u32 HashCount;
    u32 EntryCount;
} style_var_table_params;

typedef struct style_var_table
{
    u32 HashMask;
    u32 HashCount;
    u32 EntryCount;

    u32             *HashTable;
    style_var_entry *Entries;
} style_var_table;

// [Tables]

typedef struct style_parser_table_entry
{
    byte_string Name;
    u32         Value;
} style_parser_table_entry;

// [Tables]

read_only global style_parser_table_entry StyleKeywordTable[] = 
{
    {byte_string_compile("style"), StyleToken_Style},
    {byte_string_compile("var")  , StyleToken_Var  },
};

read_only global style_parser_table_entry StyleStateTable[] = 
{
    {byte_string_compile("base") , StyleState_Basic},
    {byte_string_compile("hover"), StyleState_Hover},
};

read_only global style_parser_table_entry StylePropertyTable[] =
{
    {byte_string_compile("size")        , StyleProperty_Size        },
    {byte_string_compile("color")       , StyleProperty_Color       },
    {byte_string_compile("padding")     , StyleProperty_Padding     },
    {byte_string_compile("spacing")     , StyleProperty_Spacing     },
    {byte_string_compile("font")        , StyleProperty_Font        },
    {byte_string_compile("fontsize")    , StyleProperty_FontSize    },
    {byte_string_compile("softness")    , StyleProperty_Softness    },
    {byte_string_compile("textcolor")   , StyleProperty_TextColor   },
    {byte_string_compile("borderwidth") , StyleProperty_BorderWidth },
    {byte_string_compile("bordercolor") , StyleProperty_BorderColor },
    {byte_string_compile("cornerradius"), StyleProperty_CornerRadius},

    // Layout Properties
    {byte_string_compile("display"), StyleProperty_Display},

    // Flex Properties
    {byte_string_compile("flex-direction") , StyleProperty_FlexDirection },
    {byte_string_compile("justify-content"), StyleProperty_JustifyContent},
    {byte_string_compile("align-items")    , StyleProperty_AlignItems    },
    {byte_string_compile("self-align")     , StyleProperty_SelfAlign     },
};

read_only global style_parser_table_entry StyleUnitKeywordTable[] =
{
    {byte_string_compile("auto"), UIUnit_Auto},
};

read_only global style_parser_table_entry StyleDisplayTable[] =
{
    {byte_string_compile("none")  , UIDisplay_None  },
    {byte_string_compile("normal"), UIDisplay_Normal},
    {byte_string_compile("flex")  , UIDisplay_Flex  },
};

read_only global style_parser_table_entry FlexDirectionKeywordTable [] =
{
    {byte_string_compile("row")   , UIFlexDirection_Row   },
    {byte_string_compile("column"), UIFlexDirection_Column},
};

read_only global style_parser_table_entry JustifyContentKeywordTable [] =
{
    {byte_string_compile("start")        , UIJustifyContent_Start       },
    {byte_string_compile("center")       , UIJustifyContent_Center      },
    {byte_string_compile("end")          , UIJustifyContent_End         },
    {byte_string_compile("space-between"), UIJustifyContent_SpaceBetween},
    {byte_string_compile("space-around") , UIJustifyContent_SpaceAround },
    {byte_string_compile("space-evenly") , UIJustifyContent_SpaceEvenly },
};

read_only global style_parser_table_entry AlignItemKeywordTable [] =
{
    {byte_string_compile("start")  , UIAlignItems_Start  },
    {byte_string_compile("center") , UIAlignItems_Center },
    {byte_string_compile("end")    , UIAlignItems_End    },
    {byte_string_compile("stretch"), UIAlignItems_Stretch},
};

read_only global style_parser_table_entry SelfAlignItemKeywordTable [] =
{
    {byte_string_compile("start")    , UIAlignItems_Start  },
    {byte_string_compile("center")   , UIAlignItems_Center },
    {byte_string_compile("align-end"), UIAlignItems_End    },
    {byte_string_compile("stretch")  , UIAlignItems_Stretch},
    {byte_string_compile("none")     , UIAlignItems_None  },
};

// [Runtime API]

internal ui_style_registry * CreateStyleRegistry (byte_string FileName, memory_arena *OutputArena);

// [Helpers]

internal b32      IsValidStyleTokenBuffer  (style_token_buffer *Buffer);
internal ui_color ToNormalizedColor        (vec4_unit Vec);

// [Tokenizer]

internal style_token        * GetStyleToken      (style_token_buffer *Buffer, i64 Index);
internal style_token        * EmitStyleToken     (style_token_buffer *Buffer, StyleToken_Type Type, u64 AtLine, u64 AtByte);
internal style_token        * PeekStyleToken     (style_token_buffer *Buffer, i32 Offset, style_file_debug_info *Debug);
internal void                 EatStyleTokens     (style_token_buffer *Buffer, u32 Count);
internal ui_unit              ReadUnit           (os_read_file *File, style_file_debug_info *Debug);
internal byte_string          ReadIdentifier     (os_read_file *File);
internal style_vector         ReadVector         (os_read_file *File, style_file_debug_info *Debug);
internal tokenized_style_file TokenizeStyleFile  (os_read_file File, memory_arena *Arena, style_file_debug_info *Debug);

// [Parsing Routines]

internal ui_style_registry * ParseStyleFile         (tokenized_style_file *File, memory_arena *RoutineArena, memory_arena *OutputArena, style_file_debug_info *Debug);
internal style_header           ParseStyleHeader     (style_token_buffer *Buffer, style_file_debug_info *Debug);
internal style_effect           ParseStyleEffect     (style_token_buffer *Buffer, style_file_debug_info *Debug);
internal style_block            ParseStyleBlock      (style_token_buffer *Buffer, style_var_table *VarTable, style_file_debug_info *Debug);
internal style_attribute        ParseStyleAttribute  (style_token_buffer *Buffer, style_var_table *VarTable, style_file_debug_info *debug);
internal style_variable         ParseStyleVariable   (style_token_buffer *Buffer, style_file_debug_info *Debug);

// [Variables]

internal style_var_table * PlaceStyleVarTableInMemory  (style_var_table_params Params, void *Memory);
internal style_var_entry * GetStyleVarSentinel         (style_var_table *Table);
internal style_var_entry * GetStyleVarEntry            (u32 Index, style_var_table *Table);
internal u32               PopFreeStyleVarEntry        (style_var_table *Table);
internal style_var_entry * FindStyleVarEntry           (style_var_hash Hash, style_var_table *Table);
internal style_var_hash    HashStyleVar                (byte_string Name);
internal size_t            GetStyleVarTableFootprint   (style_var_table_params Params);

// [Caching]

internal void           ValidateAttributeFormatting  (style_attribute *Attribute, style_file_debug_info *Debug);
internal style_property ConvertAttributeToProperty   (style_attribute Attribute);
internal void           CacheStyle                   (style *ParsedStyle, ui_style_registry *Registry, style_file_debug_info *Debug);

// [Error Handling]

internal void  SynchronizeParser       (style_token_buffer *Buffer, StyleToken_Type StopAt, style_file_debug_info *Debug);
internal void  ReportStyleParserError  (style_file_debug_info *Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console);
internal void  ReportStyleFileError    (style_file_debug_info *Debug, ConsoleMessage_Severity Severity, byte_string Message, console_queue *Console);
