#pragma once

// [CONSTANTS]

typedef enum CachedStyle_Flag
{
    CachedStyle_FontIsLoaded  = 1 << 0,
    CachedStyle_HasBaseStyle  = 1 << 1,
    CachedStyle_HasClickStyle = 1 << 2,
    CachedStyle_HasHoverStyle = 1 << 3,
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

typedef struct style_property
{
    b32 IsSet;

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
    };
} style_property;

typedef struct ui_cached_style
{
    // Properties (Not very memory efficient)
    style_property Properties[StyleEffect_Count][StyleProperty_Count];

    // Misc
    u32       CachedIndex;
    bit_field Flags;
} ui_cached_style;

typedef struct ui_style_registry
{
    u32              StylesCount;
    u32              StylesCapacity;
    ui_cached_style *Styles;
} ui_style_subregistry;

typedef struct ui_style_override_node ui_style_override_node;
struct ui_style_override_node
{
    ui_style_override_node *Next;
    style_property          Value;
};

typedef struct ui_style_override_list
{
    ui_style_override_node *First;
    ui_style_override_node *Last;
    u32                     Count;
} ui_style_override_list;

typedef struct ui_node_style
{
    u32                    CachedStyleIndex;
    ui_style_override_list Overrides;
} ui_node_style;

// [Properties]

internal f32               UIGetBorderWidth   (style_property Properties[StyleProperty_Count]);
internal f32               UIGetSoftness      (style_property Properties[StyleProperty_Count]);
internal f32               UIGetFontSize      (style_property Properties[StyleProperty_Count]);
internal vec2_unit         UIGetSize          (style_property Properties[StyleProperty_Count]);
internal ui_color          UIGetColor         (style_property Properties[StyleProperty_Count]);
internal ui_color          UIGetBorderColor   (style_property Properties[StyleProperty_Count]);
internal ui_color          UIGetTextColor     (style_property Properties[StyleProperty_Count]);
internal ui_padding        UIGetPadding       (style_property Properties[StyleProperty_Count]);
internal ui_spacing        UIGetSpacing       (style_property Properties[StyleProperty_Count]);
internal ui_corner_radius  UIGetCornerRadius  (style_property Properties[StyleProperty_Count]);
internal ui_font         * UIGetFont          (style_property Properties[StyleProperty_Count]);

internal void UISetStyleProperty  (StyleProperty_Type Type, style_property Value, ui_pipeline *Pipeline);
internal void UISetTextColor      (ui_color Color, ui_pipeline *Pipeline);

// [Styles]

internal ui_style_override_node * AllocateStyleOverrideNode  (StyleProperty_Type Type, memory_arena *Arena);
internal void                     SuperposeStyle             (style_property *BaseProperties, style_property *LayerProperties);
internal ui_node_style          * GetNodeStyle               (u32 Index, ui_pipeline *Pipeline);
internal ui_cached_style        * GetCachedStyle             (ui_style_registry *Registry, u32 Index);
internal style_property         * UIGetStyleEffect           (ui_cached_style *Cached, StyleEffect_Type Effect);


// [Helpers]

internal b32 IsVisibleColor  (ui_color Color);
internal b32 PropertyIsSet   (ui_cached_style *Style, StyleEffect_Type Effect, StyleProperty_Type Property);
