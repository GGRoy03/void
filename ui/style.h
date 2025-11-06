#pragma once

typedef enum StyleProperty_Type
{
    StyleProperty_Size         = 0,
    StyleProperty_Color        = 1,
    StyleProperty_Padding      = 2,
    StyleProperty_Spacing      = 3,
    StyleProperty_Softness     = 4,
    StyleProperty_BorderColor  = 5,
    StyleProperty_BorderWidth  = 6,
    StyleProperty_CornerRadius = 7,
    StyleProperty_Display      = 8,

    // Text Properties
    StyleProperty_Font         = 9,
    StyleProperty_FontSize     = 10,
    StyleProperty_TextAlign    = 11,
    StyleProperty_TextColor    = 12,

    // Flex Properties
    StyleProperty_FlexDirection  = 13,
    StyleProperty_JustifyContent = 14,
    StyleProperty_AlignItems     = 15,
    StyleProperty_SelfAlign      = 16,

    StyleProperty_Count        = 17,
    StyleProperty_None         = 666,
} StyleProperty_Type;

typedef enum StyleState_Type
{
    StyleState_Basic = 0,
    StyleState_Hover = 1,

    StyleState_None  = 2,
    StyleState_Count = 2,
} StyleState_Type;

// [CORE TYPES]

#define InvalidCachedStyleIndex 0

typedef struct style_property
{
    b32 IsSet;
    b32 IsSetRuntime;

    StyleProperty_Type Type;
    union
    {
        u32              Enum;
        f32              Float32;
        vec2_unit        Vec2;
        ui_spacing       Spacing;
        ui_padding       Padding;
        ui_color         Color;
        ui_corner_radius CornerRadius;
        void            *Pointer;
        byte_string      String;
    };
} style_property;

typedef struct ui_cached_style
{
    style_property Properties[StyleState_Count][StyleProperty_Count];
    u32            CachedIndex;
} ui_cached_style;

typedef struct ui_style_registry
{
    u32              StylesCount;
    u32              StylesCapacity;
    ui_cached_style *Styles;
} ui_style_registry;

typedef struct ui_node_style
{
    b32            IsLastVersion;
    u32            CachedStyleIndex;
    style_property Properties[StyleState_Count][StyleProperty_Count];
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
// UIGetDisplay:
//   Small helpers to make code less verbose when querying properties.
//   Ensure that it is a valid array of length StyleProperty_Count. No bounds checking are done.

internal f32                   UIGetBorderWidth    (style_property Properties[StyleProperty_Count]);
internal f32                   UIGetSoftness       (style_property Properties[StyleProperty_Count]);
internal f32                   UIGetFontSize       (style_property Properties[StyleProperty_Count]);
internal vec2_unit             UIGetSize           (style_property Properties[StyleProperty_Count]);
internal ui_color              UIGetColor          (style_property Properties[StyleProperty_Count]);
internal ui_color              UIGetBorderColor    (style_property Properties[StyleProperty_Count]);
internal ui_color              UIGetTextColor      (style_property Properties[StyleProperty_Count]);
internal ui_padding            UIGetPadding        (style_property Properties[StyleProperty_Count]);
internal ui_spacing            UIGetSpacing        (style_property Properties[StyleProperty_Count]);
internal ui_corner_radius      UIGetCornerRadius   (style_property Properties[StyleProperty_Count]);
internal ui_font             * UIGetFont           (style_property Properties[StyleProperty_Count]);
internal UIDisplay_Type        UIGetDisplay        (style_property Properties[StyleProperty_Count]);
internal UIFlexDirection_Type  UIGetFlexDirection  (style_property Properties[StyleProperty_Count]);
internal UIJustifyContent_Type UIGetJustifyContent (style_property Properties[StyleProperty_Count]);
internal UIAlignItems_Type     UIGetAlignItems     (style_property Properties[StyleProperty_Count]);
internal UIAlignItems_Type     UIGetSelfAlign      (style_property Properties[StyleProperty_Count]);


// GetHoverStyle:
//

internal ui_node_style  * GetNodeStyle         (u32 NodeIndex, ui_subtree *Subtree);
internal style_property * GetCachedProperties  (u32 StyleIndex, StyleState_Type State, ui_style_registry *Registry);

// UISetTextColor:
//  Override a style at runtine.

internal void UISetSize       (u32 NodeIndex, vec2_unit      Size   , ui_subtree *Subtree);
internal void UISetTextColor  (u32 NodeIndex, ui_color       Color  , ui_subtree *Subtree);
internal void UISetDisplay    (u32 NodeIndex, UIDisplay_Type Display, ui_subtree *Subtree);

// [Helpers]

internal b32 IsVisibleColor  (ui_color Color);
