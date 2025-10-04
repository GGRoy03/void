#pragma once

// [CONSTANTS]

typedef enum UILayoutNode_Type
{
    UILayoutNode_None       = 0,
    UILayoutNode_Window     = 1,
    UILayoutNode_Button     = 2,
    UILayoutNode_Label      = 3,
    UILayoutNode_Header     = 4,
    UILayoutNode_ScrollView = 5,
} UILayoutNode_Type;

// [FORWARD DECLARATIONS]

typedef struct ui_character ui_character;

// [CORE TYPES]

typedef struct ui_layout_node ui_layout_node;
struct ui_layout_node
{
    u32             Index;
    u32             ChildCount;
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    UILayoutNode_Type Type;
    ui_layout_box     Value;

    ui_cached_style *CachedStyle;
};

typedef struct ui_layout_tree_params
{
    u32 Depth;
    u32 NodeCount;
} ui_layout_tree_params;

typedef struct ui_layout_tree
{
    memory_arena *Arena;

    u32             NodeCapacity;
    u32             NodeCount;
    ui_layout_node *Nodes;

    u32              ParentTop;
    u32              MaximumDepth;
    ui_layout_node **ParentStack;
} ui_layout_tree;

// [GLOBALS]

read_only global u32 InvalidLayoutNodeIndex    = 0xFFFFFFFF;
read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth    = 16;

// [API]

internal ui_layout_tree * AllocateLayoutTree        (ui_layout_tree_params Params);

internal void             PopLayoutNodeParent       (ui_layout_tree *Tree);
internal void             PushLayoutNodeParent      (ui_layout_node *Node, ui_layout_tree*Tree);

internal ui_layout_node * GetLayoutNodeParent       (ui_layout_tree *Tree);
internal ui_layout_node * GetFreeLayoutNode         (ui_layout_tree *Tree, UILayoutNode_Type Type);

internal void             UITree_BindText           (ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_layout_node *Node);
internal void             UITree_BindClickCallback  (ui_layout_node *Node, ui_click_callback *Callback);

internal b32              IsValidLayoutNode         (ui_layout_node *Node);
internal void             AttachLayoutNode          (ui_layout_node *Node, ui_layout_node *Parent);
