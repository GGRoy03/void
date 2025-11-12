// ui_layout_node:
// ui_layout_tree:
//   Opaque types the user does not need to know about.
//
// GetLayoutTreeFootprint:
//   Returns the size of the memory needed to call PlaceLayoutTreeInMemory.
//   Parameters will apply their default values if needed.
//
// PlaceLayoutTreeInMemory:
//   Places the tree in memory given the parameters and the memory YOU allocated.
//   You are responsible for managing this memory.
//   If memory is 0, it will fall through and 0 will be returned, thus you must only check that this function doesn't return 0.
//
// UIEnterSubtree & UILeaveSubtree & UISubtree:
//   ...

typedef struct ui_layout_node  ui_layout_node;
typedef struct ui_layout_tree  ui_layout_tree;

#define InvalidUINodeIndex 0xFFFFFFFF

typedef enum UILayoutNode_Flag
{
    // Painting
    UILayoutNode_DoNotPaint      = 1 << 1,
    UILayoutNode_HasClip         = 1 << 2,

    // State
    UILayoutNode_IsDraggable     = 1 << 4,
    UILayoutNode_IsResizable     = 1 << 5,

    // Layout Info (Should not exist most likely)
    UILayoutNode_IsParent        = 1 << 6,

    // Resources
    UILayoutNode_HasText         = 1 << 7,
    UILayoutNode_HasTextInput    = 1 << 8,
    UILayoutNode_HasScrollRegion = 1 << 9,

    // Debug
    UILayoutNode_DebugOuterBox   = 1 << 10,
    UILayoutNode_DebugInnerBox   = 1 << 11,
    UILayoutNode_DebugContentBox = 1 << 12,
} UILayoutNode_Flag;

internal u64              GetLayoutTreeFootprint   (u64 NodeCount);
internal ui_layout_tree * PlaceLayoutTreeInMemory  (u64 NodeCount, void *Memory);
internal ui_node          AllocateUINode           (bit_field Flags, ui_subtree *Subtree);
internal void             UIEnd                    (void);

internal ui_node FindLayoutChild        (u32 NodeIndex, u32 ChildIndex, ui_subtree *Subtree);
internal void    AppendLayoutChild      (u32 ParentIndex, u32 ChildIndex, ui_subtree *Subtree);
internal void    ReserveLayoutChildren  (ui_node Node, u32 Amount, ui_subtree *Subtree);

internal b32 IsMouseInsideOuterBox  (vec2_f32 MousePosition, u32 NodeIndex, ui_subtree *Subtree);

internal void UpdateNodeIfNeeded    (u32 NodeIndex, ui_subtree *Subtree);
internal void SetLayoutNodeFlags    (u32 NodeIndex, bit_field Flags, ui_subtree *Subtree);
internal void ClearLayoutNodeFlags  (u32 NodeIndex, bit_field Flags, ui_subtree *Subtree);

// ------------------------------------------------------------------------------------

internal void ComputeSubtreeLayout  (ui_subtree *Subtree);
internal void UpdateSubtreeState    (ui_subtree *Subtree);

internal void PaintSubtree           (ui_subtree *Subtree);
