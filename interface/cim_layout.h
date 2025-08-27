#pragma once

#define CimLayout_MaxNestDepth 32
#define CimLayout_InvalidNode 0xFFFFFFFF
#define CimLayout_MaxShapesForDrag 4
#define CimLayout_MaxDragPerBatch 16

typedef enum CimComponent_Flag
{
    CimComponent_Invalid = 0,
    CimComponent_Window  = 1 << 0,
    CimComponent_Button  = 1 << 1,
    CimComponent_Text    = 1 << 2,
} CimComponent_Flag;

typedef enum Layout_Order
{
    Layout_Horizontal = 0,
    Layout_Vertical = 1,
} Layout_Order;

typedef struct cim_window
{
    cim_i32 LastFrameScreenX;
    cim_i32 LastFrameScreenY;

    bool IsInitialized;
} cim_window;

// NOTE: This is weird. What is the button state?
typedef struct cim_button
{
    cim_u32 Nothing;
} cim_button;

typedef struct cim_text
{
    text_layout_info LayoutInfo;
} cim_text;

typedef struct ui_component
{
    bool IsInitialized;

    cim_u32  LayoutNodeIndex;
    theme_id ThemeId;
    
    CimComponent_Flag Type;
    union
    {
        cim_window Window;
        cim_button Button;
        cim_text   Text;
    } For;
} ui_component;

typedef struct ui_component_entry
{
    ui_component Value;
    char          Key[64];
} ui_component_entry;

typedef struct ui_component_hashmap
{
    cim_u8             *Metadata;
    ui_component_entry *Buckets;
    cim_u32             GroupCount;
    bool                IsInitialized;
} ui_component_hashmap;

// NOTE: Trying something new...
typedef struct ui_component_state
{
    // State
    bool Clicked;
    bool Hovered;
} ui_component_state;

typedef struct ui_layout_node
{
    // Hierarchy
    cim_u32 Id;
    cim_u32 Parent;
    cim_u32 FirstChild;
    cim_u32 LastChild;
    cim_u32 NextSibling;

    // Layout Intent
    cim_f32 ContentWidth;
    cim_f32 ContentHeight;
    cim_f32 PrefWidth;
    cim_f32 PrefHeight;

    // Arrange Result
    cim_f32 X;
    cim_f32 Y;
    cim_f32 Width;
    cim_f32 Height;

    // Layout..
    cim_vector2  Spacing;
    cim_vector4  Padding;
    Layout_Order Order;

    ui_component_state *State;
} ui_layout_node;

typedef struct ui_layout_tree
{
    // Memory pool
    ui_layout_node *Nodes;
    cim_u32         NodeCount;
    cim_u32         NodeSize;

    // Tree-Logic
    cim_u32 ParentStack[CimLayout_MaxNestDepth];
    cim_u32 AtParent;

    // Tree-State
    ui_draw_list DrawList;

    // Garbage drag code, needs to be removed.
    cim_u32 FirstDragNode;  // Set transforms to 0 when we pop that node.
    cim_i32 DragTransformX;
    cim_i32 DragTransformY;
} ui_layout_tree;

typedef struct cim_layout
{
    ui_layout_tree       Tree;       // WARN: Forced to 1 tree for now.
    ui_component_hashmap Components;
} cim_layout;