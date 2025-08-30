#pragma once

// [Macros]

#define UITheme_NameLength 64
#define UITheme_ThemeCount 1000
#define UITheme_ErrorMessageLength 256

// [Enums]

typedef enum UIThemeAttribute_Flag
{
    UIThemeAttribute_None        = 0,
    UIThemeAttribute_Size        = 1 << 0,
    UIThemeAttribute_Color       = 1 << 1,
    UIThemeAttribute_Padding     = 1 << 2,
    UIThemeAttribute_Spacing     = 1 << 3,
    UIThemeAttribute_LayoutOrder = 1 << 4,
    UIThemeAttribute_BorderColor = 1 << 5,
    UIThemeAttribute_BorderWidth = 1 << 6,
} UIThemeAttribute_Flag;

typedef enum UIStyleToken_Type
{
    // Extended ASCII...

    // Flags used when parsing values.
    UIStyleToken_String      = 1 << 8,
    UIStyleToken_Identifier  = 1 << 9,
    UIStyleToken_Number      = 1 << 10,
    UIStyleToken_Assignment  = 1 << 11,
    UIStyleToken_Vector      = 1 << 12,
    UIStyleToken_Theme       = 1 << 13,
    UIStyleToken_For         = 1 << 14,

    // Effects (Does it need to be flagged?)
    UIStyleToken_HoverEffect = 1 << 15,
    UIStyleToken_ClickEffect = 1 << 16,
} UIStyleToken_Type;

// NOTE: This is weird. Kind of.
typedef enum UITheme_Type
{
    UITheme_None   = 0,
    UITheme_Window = 1,
    UITheme_Button = 2,
} UITheme_Type;

typedef enum UIThemeParsingError_Type
{
    ThemeParsingError_None     = 0,
    ThemeParsingError_Syntax   = 1,
    ThemeParsingError_Argument = 2,
    ThemeParsingError_Internal = 3,
} UIThemeParsingError_Type;

// [Types]

typedef struct theme_id
{
    cim_u32 Value;
} theme_id;

typedef struct theme_parsing_error
{
    UIThemeParsingError_Type Type;
    union
    {
        cim_u32 LineInFile;
        cim_u32 ArgumentIndex;
    };
    
    char Message[UITheme_ErrorMessageLength];
} theme_parsing_error;

typedef struct theme_token
{
    cim_u32 LineInFile;

    UIStyleToken_Type Type;
    union
    {
        cim_f32 Float32;
        cim_u32 UInt32;
        struct
        {
            cim_u8 *At;
            cim_u32 Size;
        } Identifier;
        struct
        {
            cim_f32 DataF32[4];
            cim_u32 Size;
        } Vector;
    };
} theme_token;

typedef struct ui_window_theme
{
    theme_id ThemeId;

    // Attributes
    cim_vector4 Color;
    cim_vector4 BorderColor;
    cim_u32     BorderWidth;
    cim_vector2 Size;
    cim_vector2 Spacing;
    cim_vector4 Padding;
} ui_window_theme;

typedef struct ui_button_theme
{
    theme_id ThemeId;

    // Attributes
    cim_vector4 Color;
    cim_vector4 BorderColor;
    cim_u32     BorderWidth;
    cim_vector2 Size;
} ui_button_theme;

typedef struct ui_theme
{
    UITheme_Type Type;
    union
    {
        ui_window_theme Window;
        ui_button_theme Button;
    };
} ui_theme;

typedef struct theme_parser
{
    buffer TokenBuffer;

    cim_u32 AtLine;
    cim_u32 AtToken;

    theme_token *ActiveThemeNameToken;
    ui_theme    *ActiveTheme;
} theme_parser;

typedef struct theme_info
{
    cim_u8  Name[UITheme_NameLength];
    cim_u32 NameLength;

    theme_id NextWithSameLength;
    theme_id Id;
    theme_id HoverTheme;
    theme_id ClickTheme;

    ui_theme Theme;
} theme_info;

typedef struct theme_table
{
    theme_info Themes[UITheme_ThemeCount];
    cim_u32    NextWriteIndex;
} theme_table;

// [API]

// Queries
static ui_window_theme GetWindowTheme(const char *ThemeName, theme_id ThemeId);
static ui_button_theme GetButtonTheme(const char *ThemeName, theme_id ThemeId);