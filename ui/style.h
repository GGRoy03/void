#pragma once

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
    style_property Properties[StyleEffect_Count][StyleProperty_Count];
    u32            CachedIndex;
} ui_cached_style;

typedef struct ui_style_registry
{
    u32              StylesCount;
    u32              StylesCapacity;
    ui_cached_style *Styles;
} ui_style_registry;

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
    style_property         ComputedProperties[StyleProperty_Count];
} ui_node_style;

// UIGetBorderWidth:
// UIGetSoftness:
// UIGetFontSize:
// UIGetSize:
// UIGetColor:
// UIGetBorderColor:
// UIGetTextColor:
// UIGetPadding:
// UIGetSpacing:
// UIGetCornerRadius:
// UIGetFont:
//   Small helpers to make code less verbose when querying properties.

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

// GetHoverStyle:
//

internal style_property * GetBaseStyle      (u32 StyleIndex, ui_style_registry *Registry);
internal style_property * GetHoverStyle     (u32 CachedIndex, ui_style_registry *Registry);
internal style_property * GetComputedStyle  (u32 NodeIndex, ui_subtree *Subtree);

// UISetTextColor:
//  Override a style at runtine.

internal void SetUITextColor  (ui_node Node, ui_color Color, ui_subtree *Subtree, memory_arena *Arena);

// [Styles]


// [Helpers]

internal b32 IsVisibleColor  (ui_color Color);
internal b32 PropertyIsSet   (ui_cached_style *Style, StyleEffect_Type Effect, StyleProperty_Type Property);
