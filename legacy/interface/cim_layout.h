#pragma once

// [Constants]

#define UILayout_NodeCount 128
#define UILayout_NestingDepth 32
#define UILayout_InvalidNode 0xFFFFFFFF

typedef enum CimComponent_Flag
{
    CimComponent_Invalid = 0,
    CimComponent_Window  = 1 << 0,
    CimComponent_Button  = 1 << 1,
    CimComponent_Text    = 1 << 2,
} CimComponent_Flag;

typedef enum UIContainer_Type
{
    UIContainer_None   = 0,
    UIContainer_Row    = 1,
    UIContainer_Column = 2,
} UIContainer_Type;

// [Types]

typedef struct cim_window
{
    cim_u32 Nothing;
} cim_window;

typedef struct ui_button
{
    void *Nothing;
} ui_button;

typedef struct cim_text
{
    void *Nothing;
} cim_text;

typedef struct ui_component
{
    theme_id ThemeId;

    CimComponent_Flag Type;
    union
    {
        cim_window Window;
        ui_button  Button;
        cim_text   Text;
    } Extra;
} ui_component;

typedef struct ui_component_entry
{
    ui_component Value;
    char         Key[64];
} ui_component_entry;

typedef struct ui_component_hashmap
{
    cim_u8             *Metadata;
    ui_component_entry *Buckets;
    cim_u32             GroupCount;
    bool                IsInitialized;
} ui_component_hashmap;

// NOTE: We could have leaner types? If this grows too much.
typedef struct ui_component_state
{
    bool    Clicked;
    bool    Hovered;
    cim_u32 ActiveIndex;
} ui_component_state;

typedef struct ui_layout_node
{
    // NOTE: Some of these we don't need...
    cim_u32 Id;
    cim_u32 Parent;
    cim_u32 FirstChild;
    cim_u32 Next;
    cim_u32 ChildCount;

    UIContainer_Type ContainerType;
    cim_vector2      Spacing;
    cim_vector4      Padding;

    cim_f32 X;
    cim_f32 Y;
    cim_f32 Width;
    cim_f32 Height;

    ui_component_state *State;
} ui_layout_node;

typedef struct ui_tree
{
    union
    {
        ui_draw_node   Draw[UILayout_NodeCount];
        ui_layout_node Layout[UILayout_NodeCount];
    } Nodes;

    cim_u32 NodeCount;

    cim_u32 ParentStack[32];
    cim_u32 ParentTop;
} ui_tree;

typedef struct ui_tree_sizing_result
{
    cim_u32 Stack[256];
    cim_u32 Top;
} ui_tree_sizing_result;