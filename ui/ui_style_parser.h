#pragma once

// TO ADD NEW STYLES, FOLLOW THIS GUIDE.
// 1) Add the attribute flag in UIStyleAttribute_Flag.
// 2) Add the valid style types which may have this new attribute in this table: StyleTypeValidAttributesTable.
// 3) Add the check for the new attribute in GetStyleAttributeFlag.
// 4) Add the format check in IsAttributeFormattedCorrectly
// 5) Add the SaveStyleAttribute implementation for the new type.

// NOTE: Is there no way to make this way simpler? With a single 'key' per type, the type itself?

// [Globals]

#define ThemeNameLength 64
#define ThemeMaxCount   256

// [Enums]

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

typedef enum UIStyleToken_Type
{
    UIStyleToken_EndOfFile   = 0,
    UIStyleToken_Percent     = 37,
    UIStyleToken_Comma       = 44,
    UIStyleToken_Period      = 46,
    UIStyleToken_SemiColon   = 59,
    UIStyleToken_OpenBrace   = 123,
    UIStyleToken_CloseBrace  = 125,
    UIStyleToken_Identifier  = 256,
    UIStyleToken_Assignment  = 257,
    UIStyleToken_Unit        = 258,
    UIStyleToken_String      = 259,
    UIStyleToken_Vector      = 260,
    UIStyleToken_Style       = 261,
    UIStyleToken_For         = 262,
} UIStyleToken_Type;

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

typedef struct style_tokenizer
{
    u32           Count;
    u32           Capacity;
    style_token  *Buffer;
    memory_arena *Arena;
    u32           AtLine;
} style_tokenizer;

// TODO: Maybe separate this into a tokenizer and a parser.
typedef struct style_parser
{
    u32                 TokenIndex;
    byte_string         StyleName;
    UINode_Type         StyleType;
    ui_style            Style;
} style_parser;

// [GLOBALS]

read_only global bit_field StyleTokenValueMask = UIStyleToken_String | UIStyleToken_Unit | UIStyleToken_Vector;

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

internal void LoadThemeFiles  (byte_string *Files, u32 FileCount, ui_style_registery *Registery, render_handle Renderer, ui_state *UIState);

// [Tokenizer]

internal style_token *CreateStyleToken   (UIStyleToken_Type Type, style_tokenizer *Tokenizer);
internal b32          TokenizeStyleFile  (os_file *File, style_tokenizer *Tokenizer);
internal b32          ReadString         (os_file *File, byte_string *OutString);
internal b32          ReadVector         (os_file *File, u32 MinimumSize, u32 MaximumSize, style_token *Result);
internal b32          ReadUnit           (os_file *File, ui_unit *Result);

// [Parsing]

internal void          ConsumeStyleTokens  (style_parser *Parser, u32 Count);
internal style_token * PeekStyleToken      (style_token *Tokens, u32 TokenBufferSize, u32 Index, u32 Offset);

internal b32 ParseStyleFile        (style_parser *Parser, style_token *Tokens, u32 TokenBufferSize, render_handle Renderer, ui_state *UIState, ui_style_registery *Registery);
internal b32 ParseStyleAttribute   (style_parser *Parser, style_token *Tokens, u32 TokenBufferSize);
internal b32 ParseStyleHeader      (style_parser *Parser, style_token *Tokens, u32 TokenBufferSize, ui_style_registery *Registery);

internal b32      ValidateColor      (vec4_unit Vec);
internal ui_color ToNormalizedColor  (vec4_unit Vec);

internal b32                   SaveStyleAttribute             (UIStyleAttribute_Flag Attribute, style_token *Value, style_parser *Parser);
internal UIStyleAttribute_Flag GetStyleAttributeFlag          (byte_string Identifier);
internal b32                   IsAttributeFormattedCorrectly  (UIStyleToken_Type TokenType, UIStyleAttribute_Flag AttributeFlag);
internal void                  CacheStyle                     (ui_style Style, byte_string Name, render_handle Renderer, ui_state *UIState, ui_style_registery *Registery);

// [Error Handling]

internal read_only char *UIStyleAttributeToString  (UIStyleAttribute_Flag Flag);
internal void            WriteStyleErrorMessage    (u32 LineInFile, OSMessage_Severity Severity, byte_string Format, ...);
