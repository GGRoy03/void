#pragma once

// [CONSTANTS]

typedef enum CachedStyle_Flag
{
    CachedStyle_FontIsLoaded = 1 << 0,
} CachedStyle_Flag;

typedef enum StyleProperty_Type
{
    StyleProperty_Size         = 0,
    StyleProperty_Color        = 1,
    StyleProperty_Padding      = 2,
    StyleProperty_Spacing      = 3,
    StyleProperty_Font         = 4,
    StyleProperty_FontSize     = 5,
    StyleProperty_Softness     = 6,
    StyleProperty_BorderColor  = 7,
    StyleProperty_BorderWidth  = 8,
    StyleProperty_CornerRadius = 9,
    StyleProperty_TextColor    = 10,

    StyleProperty_Count        = 11,
    StyleProperty_None         = 666,
} StyleProperty_Type;

typedef enum StyleEffect_Type
{
    StyleEffect_Base  = 0,
    StyleEffect_Hover = 1,
    StyleEffect_Click = 2,
    StyleEffect_None  = 3,

    StyleEffect_Count = 3,
} StyleEffect_Type;

// [CORE TYPES]

typedef struct style_property style_property;
struct style_property
{
    b32             IsSet;
    style_property *Next;

    StyleProperty_Type Type;
    union
    {
        f32              Float32;
        vec2_unit        Vec2;
        ui_spacing       Spacing;
        ui_padding       Padding;
        ui_color         Color;
        ui_corner_radius CornerRadius;
        void            *Pointer;
        byte_string      String;
    };
};

typedef struct style_property_list
{
    style_property *First;
    style_property *Last;
    u32             Count;
} style_property_list;

typedef struct ui_cached_style ui_cached_style;
struct ui_cached_style
{
    // Properties (Not very memory efficient)
    style_property Properties[StyleEffect_Count][StyleProperty_Count];

    // Misc
    bit_field Flags;
};

typedef struct ui_style_subregistry ui_style_subregistry;
typedef struct ui_style_subregistry
{
    u32              StyleCount;
    u32              StyleCapacity;
    ui_cached_style *Styles;

    ui_style_subregistry *Next;
} ui_style_subregistry;

typedef struct ui_style_registry
{
    u32                   Count;
    ui_style_subregistry *First;
    ui_style_subregistry *Last;
} ui_style_registry;

// [Properties]

internal f32               UIGetBorderWidth   (ui_cached_style *Cached);
internal f32               UIGetSoftness      (ui_cached_style *Cached);
internal f32               UIGetFontSize      (ui_cached_style *Cached);
internal ui_color          UIGetColor         (ui_cached_style *Cached);
internal ui_color          UIGetBorderColor   (ui_cached_style *Cached);
internal ui_color          UIGetTextColor     (ui_cached_style *Cached);
internal byte_string       UIGetFontName      (ui_cached_style *Cached);
internal ui_corner_radius  UIGetCornerRadius  (ui_cached_style *Cached);
internal ui_font         * UIGetFont          (ui_cached_style *Cached);

internal void              UISetFont          (ui_cached_style *Cached, ui_font *Font);
