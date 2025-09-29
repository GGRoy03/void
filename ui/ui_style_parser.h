#pragma once

// TO ADD NEW STYLES, FOLLOW THIS GUIDE.
// 1) Add the attribute flag in UIStyleAttribute_Flag.
// 2) Add the valid style types which may have this new attribute in this table: StyleTypeValidAttributesTable.
// 3) Add the check for the new attribute in GetStyleAttributeFlag.
// 4) Add the format check in IsAttributeFormattedCorrectly
// 5) Add the SaveStyleAttribute implementation for the new type.

// NOTE: Is there no way to make this way simpler? With a single 'key' per type, the type itself?

// [Enums]

typedef enum UIStyleToken_Type
{
    UIStyleToken_EndOfFile   = 0,
    UIStyleToken_Percent     = 37,
    UIStyleToken_Comma       = 44,
    UIStyleToken_Period      = 46,
    UIStyleToken_SemiColon   = 59,
    UIStyleToken_AtSymbol    = 64,
    UIStyleToken_OpenBrace   = 123,
    UIStyleToken_CloseBrace  = 125,
    UIStyleToken_Identifier  = 256,
    UIStyleToken_Assignment  = 257,
    UIStyleToken_Unit        = 258,
    UIStyleToken_String      = 259,
    UIStyleToken_Vector      = 260,
    UIStyleToken_Style       = 261,
    UIStyleToken_For         = 262,
    UIStyleToken_Var         = 263,
} UIStyleToken_Type;

typedef enum UIStyleAttribute_Flag
{
    UIStyleAttribute_None         = 0,
    UIStyleAttribute_Size         = 1 << 0,
    UIStyleAttribute_Color        = 1 << 1,
    UIStyleAttribute_Padding      = 1 << 2,
    UIStyleAttribute_Spacing      = 1 << 3,
    UIStyleAttribute_FontName     = 1 << 4,
    UIStyleAttribute_FontSize     = 1 << 5,
    UIStyleAttribute_Softness     = 1 << 6,
    UIStyleAttribute_BorderColor  = 1 << 7,
    UIStyleAttribute_BorderWidth  = 1 << 8,
    UIStyleAttribute_CornerRadius = 1 << 9,
} UIStyleAttribute_Flag;

typedef enum UICacheStyle_Flag
{
    UICacheStyle_NoFlag          = 0,
    UICacheStyle_BindClickEffect = 1 << 0,
    UICacheStyle_BindHoverEffect = 1 << 1,
} UICacheStyle_Flag;

typedef enum UIStyle_Type
{
    UIStyle_Base  = 0,
    UIStyle_Hover = 1,
    UIStyle_Click = 2,
    UIStyle_None  = 3,

    UIStyle_Type_Count = 3,
} UIStyle_Type;

// [Types]

typedef struct style_token
{
    u32 LineInFile;

    UIStyleToken_Type Type;
    union
    {
        ui_unit     Unit;
        byte_string Identifier;
        vec4_unit   Vector;
    };
} style_token;

typedef struct tokenized_style_file
{
    style_token *Buffer;
    u32          BufferSize;
    u32          LineCount;
    u32          AtToken;
    b32          HasError;
} tokenized_style_file;

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

typedef struct style_parser
{
    UIStyle_Type     StyleType;
    ui_style         Styles[UIStyle_Type_Count];
    b32              StyleIsSet[UIStyle_Type_Count];
    byte_string      StyleName;
    style_var_table *VarTable;
} style_parser;

// [GLOBALS]

read_only global u32                    MAXIMUM_STYLE_NAME_LENGTH          = 64;
read_only global u32                    MAXIMUM_STYLE_COUNT_PER_REGISTERY  = 128;
read_only global u32                    MAXIMUM_STYLE_TOKEN_COUNT_PER_FILE = 10'000;
read_only global style_var_table_params STYLE_VAR_TABLE_PARAMS             = {.EntryCount = 512, .HashCount = 128};

read_only global bit_field StyleTypeValidAttributesTable[] =
{
    {0}, // None

    // Window
    {
        UIStyleAttribute_Size       |UIStyleAttribute_Padding     |UIStyleAttribute_Spacing |
        UIStyleAttribute_BorderColor|UIStyleAttribute_BorderWidth |UIStyleAttribute_Color   |
        UIStyleAttribute_Softness   |UIStyleAttribute_CornerRadius|UIStyleAttribute_FontName|
        UIStyleAttribute_FontSize
    },

    // Button
    {
        UIStyleAttribute_Size    |UIStyleAttribute_BorderColor|UIStyleAttribute_BorderWidth |
        UIStyleAttribute_Color   |UIStyleAttribute_Softness   |UIStyleAttribute_CornerRadius|
        UIStyleAttribute_FontName|UIStyleAttribute_FontSize
    },

    // Label
    {
        UIStyleAttribute_FontSize | UIStyleAttribute_FontName
    },

    // Header
    {
        UIStyleAttribute_Size    | UIStyleAttribute_BorderColor | UIStyleAttribute_BorderWidth  |
        UIStyleAttribute_Color   | UIStyleAttribute_Softness    | UIStyleAttribute_CornerRadius |
        UIStyleAttribute_FontName| UIStyleAttribute_FontSize    | UIStyleAttribute_Padding      |
        UIStyleAttribute_Spacing
    },
};

// [API]

internal ui_style_registery LoadStyleFromFiles(os_handle *FileHandles, u32 Count, memory_arena *OutputArena);

// [Tokenizer]

internal b32                  ReadString         (os_read_file *File, byte_string *OutString);
internal b32                  ReadVector         (os_read_file *File, u32 MinimumSize, u32 MaximumSize, u32 CurrentLineInFile, style_token *Result);
internal b32                  ReadUnit           (os_read_file *File, u32 CurrentLineInFile, ui_unit *Result);
internal tokenized_style_file TokenizeStyleFile  (os_read_file File, memory_arena *Arena);

// [Parsing Routines]

internal b32 ParseTokenizedStyleFile  (tokenized_style_file *TokenizedFile, memory_arena *Arena, ui_style_registery *Registery);
internal b32 ParseStyleVariable       (style_parser *Parser, tokenized_style_file *TokenizedFile);
internal b32 ParseStyleHeader         (style_parser *Parser, tokenized_style_file *TokenizedFile, ui_style_registery *Registery);
internal b32 ParseStyleAttribute      (style_parser *Parser, tokenized_style_file *TokenizedFile, UINode_Type ParsingFor);

// [Variables]

internal style_var_table * PlaceStyleVarTableInMemory  (style_var_table_params Params, void *Memory);
internal style_var_entry * GetStyleVarSentinel         (style_var_table *Table);
internal style_var_entry * GetStyleVarEntry            (u32 Index, style_var_table *Table);
internal u32               PopFreeStyleVarEntry        (style_var_table *Table);
internal style_var_entry * FindStyleVarEntry           (style_var_hash Hash, style_var_table *Table);
internal style_var_hash    HashStyleVarIdentifier      (byte_string Identifier);
internal size_t            GetStyleVarTableFootprint   (style_var_table_params Params);

// []

internal b32      ValidateColor      (vec4_unit Vec);
internal ui_color ToNormalizedColor  (vec4_unit Vec);

internal b32                   SaveStyleAttribute             (UIStyleAttribute_Flag Attribute, style_token *Value, style_parser *Parser);
internal UIStyleAttribute_Flag GetStyleAttributeFlag          (byte_string Identifier);
internal b32                   IsAttributeFormattedCorrectly  (UIStyleToken_Type TokenType, UIStyleAttribute_Flag AttributeFlag);
internal ui_cached_style      *CacheStyle                     (ui_style Style, byte_string Name, UIStyle_Type Type, ui_cached_style *BaseStyle, ui_style_registery *Registery);
internal ui_cached_style      *CreateCachedStyle              (ui_style Style, ui_style_registery *Registery);
internal ui_style_name        *CreateCachedStyleName          (byte_string Name, ui_cached_style *CachedStyle, ui_style_registery *Registery);

// [Error Handling]

internal read_only char * StyleAttributeToString  (UIStyleAttribute_Flag Flag);
internal void             LogStyleFileMessage     (u32 LineInFile, OSMessage_Severity Severity, byte_string Format, ...);
