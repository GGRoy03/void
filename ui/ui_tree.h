#pragma once

// [CONSTANTS]

typedef enum UITree_Type
{
    UITree_None   = 0,
    UITree_Style  = 1,
    UITree_Layout = 2,
} UITree_Type;

typedef enum UINode_Type
{
    UINode_None   = 0,
    UINode_Window = 1,
    UINode_Button = 2,
    UINode_Label  = 3,
    UINode_Header = 4,
} UINode_Type;

// [CORE TYPES]

struct ui_node
{
    u32      Id;
    ui_node *Parent;
    ui_node *First;
    ui_node *Last;
    ui_node *Next;
    ui_node *Prev;

    UINode_Type Type;
    union
    {
        ui_style      Style;
        ui_layout_box Layout;
    };
};

typedef struct ui_tree_params
{
    u32         Depth;     // Deepest node in the tree.
    u32         NodeCount; // How many nodes can it hold.
    UITree_Type Type;
} ui_tree_params;

typedef struct ui_tree
{
    memory_arena *Arena;
    UITree_Type   Type;

    u32      NodeCapacity;
    u32      NodeCount;
    ui_node *Nodes;

    u32       ParentTop;
    u32       MaximumDepth;
    ui_node **ParentStack;
} ui_tree;

// [GLOBALS]

read_only global u32 InvalidNodeId = 0xFFFFFFFF;

read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth = 16;

// [API]

internal ui_tree * UITree_Allocate             (ui_tree_params Params);
                                               
internal void      UITree_PopParent            (ui_tree *Tree);
internal void      UITree_PushParent           (ui_node *Node, ui_tree *Tree);
                                               
internal ui_node * UITree_GetParent            (ui_tree *Tree);
internal ui_node * UITree_GetFreeNode          (ui_tree *Tree, UINode_Type Type);
internal ui_node * UITree_GetLayoutNode        (ui_node *Node, ui_pipeline *Pipeline);
internal ui_node * UITree_GetStyleNode         (ui_node *Node, ui_pipeline *Pipeline);

internal void      UITree_BindText             (ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_node *Node);
internal void      UITree_BindFlag             (ui_node *Node, b32 Set, UILayoutNode_Flag Flag, ui_pipeline *Pipeline);
internal void      UITree_BindClickCallback    (ui_node *Node, ui_click_callback *Callback, ui_pipeline *Pipeline);
                                            
internal b32       UITree_IsValidNode          (ui_node *Node, ui_tree *Tree);
internal b32       UITree_NodesAreParallel     (ui_node *Node1, ui_node *Node2);
internal b32       UITree_IsNodeALeaf          (UINode_Type Type);
internal void      UITree_LinkNodes            (ui_node *Node, ui_node *Parent);

internal void      UITree_SynchronizePipeline  (ui_node *StyleRoot, ui_pipeline *Pipeline);