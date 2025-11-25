// ui_layout_tree:
//   Opaque type representing the internal layout tree structure. Users interact with
//   layout nodes through indices and the subtree API, never directly with this type.
//
// GetLayoutTreeFootprint(NodeCount):
//   Use this to pre-allocate memory before calling PlaceLayoutTreeInMemory. (See PlaceLayoutTreeInMemory)
//
//   Returns:
//      The total memory required (in bytes) to initialize a layout tree that can hold {NodeCount} layout nodes.
//
// PlaceLayoutTreeInMemory(NodeCount, Memory)
//   Initializes a layout tree structure within the provided {Memory}.
//   The buffer must be at least GetLayoutTreeFootprint({NodeCount}) bytes large.
//
//   Returns:
//      A pointer to the initialized tree, or 0 if {Memory} is null.
//
//   Example:
//      u64 NodeCount = 64;
//      u64 Footprint = GetLayoutTreeFootprint(NodeCount);
//
//      void *Memory = PlaceLayoutTreeInMemory(NodeCount, malloc(Footprint));
//      if(Memory)  // -> If this is false, then we know the malloc (Or any allocator) failed
//      {
//      }
//
// UIEnterSubtree & UILeaveSubtree & UISubtree:
//
// AllocateLayoutNode:
//
// UIEnd:
//
// FindLayoutChild(ParentIndex, ChildIndex, Subtree)
//   Searches for the {ChildIndex}-th child of the node at {ParentIndex} within the {Subtree}.
//
//   Returns:
//      The node index of the child, or InvalidLayoutNodeIndex if not found or if ChildIndex exceeds the number of children.
//
// AppendLayoutChild(ParentIndex, ChildIndex, Subtree)
//   Adds {ChildIndex} as a child of {ParentIndex}, establishing a parent-child relationship.
//
// ReserveLayoutChildren(NodeIndex)
//   Pre-allocates space for Amount child nodes under the node at Index. This is an
//   optimization hint to reduce fragmentation when adding many children incrementally.
//   You may find these nodes using FindLayoutChild(...)
//
// IsMouseInsideOuterBox(MousePosition, NodeIndex, Subtree)
//   Tests whether the mouse at MousePosition is within the outer box bounds of the node
//   at NodeIndex.
//   Returns true if the mouse is inside, false otherwise. Useful for
//   hit-testing and input handling.
//
// UpdateNodeIfNeeded:
//   Re-calculates layout for the node at NodeIndex if its layout state is dirty.
//   Call this when node properties have changed and layout must be recalculated.
//   Does nothing if the node is not marked as needing an update.
//
// SetLayoutNodeFlags & ClearLayoutNodeFlags:
//   Modify the flags bitfield of the node at NodeIndex. SetLayoutNodeFlags enables the
//   specified flags, ClearLayoutNodeFlags disables them. Flags control rendering behavior,
//   interactivity, and debug visualization.
//   Opaque type representing the internal layout tree structure. Users interact with
//   layout nodes through indices and the subtree API, never directly with this type.
//
// GetLayoutTreeFootprint:
//   Returns the total memory required (in bytes) to initialize a layout tree that can
//   hold NodeCount layout nodes. Use this to pre-allocate memory before calling
//   PlaceLayoutTreeInMemory.
//
// PlaceLayoutTreeInMemory:
//   Initializes a layout tree structure within the provided memory buffer.
//   The buffer must be at least GetLayoutTreeFootprint(NodeCount) bytes large.
//   Returns a pointer to the initialized tree, or 0 if Memory is null.
//   Assert fails if NodeCount is 0.
//   Preconditions:
//      NodeCount > 0
//      Memory is either null or points to valid allocated memory of sufficient size
//
// UIEnterSubtree & UILeaveSubtree & UISubtree:
//   Control the lifetime and scope of a UI subtree. UIEnterSubtree marks the beginning
//   of a subtree block, UILeaveSubtree marks the end. UISubtree is a convenience macro
//   that wraps both, establishing a scoped block for hierarchical UI construction.
//
// AllocateLayoutNode:
//   Creates a new layout node with the specified initial flags and adds it to the subtree.
//   Returns the index of the newly allocated node. The node is not yet attached to the
//   tree hierarchy; use AppendLayoutChild to establish parent-child relationships.
//
// UIEnd:
//   Finalizes the current UI construction phase. Must be called after all UI blocks
//   and subtrees have been defined to commit layout changes.
//
// FindLayoutChild:
//   Searches for the ChildIndex-th child of the node at NodeIndex within the subtree.
//   Returns the node index of the child, or InvalidLayoutNodeIndex if not found or if
//   ChildIndex exceeds the number of children.
//
// AppendLayoutChild:
//   Adds ChildIndex as a child of ParentIndex, establishing a parent-child relationship.
//   The child is appended to the parent's existing children list. The child must have
//   been previously allocated with AllocateLayoutNode.
//
// ReserveLayoutChildren:
//   Pre-allocates space for Amount child nodes under the node at Index. This is an
//   optimization hint to reduce reallocations when adding many children incrementally.
//   Does not create actual nodes; use AppendLayoutChild to add nodes to the reserved space.
//
// IsMouseInsideOuterBox:
//   Tests whether the mouse at MousePosition is within the outer box bounds of the node
//   at NodeIndex. Returns true if the mouse is inside, false otherwise. Useful for
//   hit-testing and input handling.
//
// UpdateNodeIfNeeded:
//   Re-calculates layout for the node at NodeIndex if its layout state is dirty.
//   Call this when node properties have changed and layout must be recalculated.
//   Does nothing if the node is not marked as needing an update.
//
// SetLayoutNodeFlags & ClearLayoutNodeFlags:
//   Modify the flags bitfield of the node at NodeIndex. SetLayoutNodeFlags enables the
//   specified flags, ClearLayoutNodeFlags disables them. Flags control rendering behavior,
//   interactivity, and debug visualization.

#define InvalidLayoutNodeIndex 0xFFFFFFFF

// NOTE:
// Should not be exposed in the header?

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
    UILayoutNode_HasImage        = 1 << 10,

    // Debug
    UILayoutNode_DebugOuterBox   = 1 << 11,
    UILayoutNode_DebugInnerBox   = 1 << 12,
    UILayoutNode_DebugContentBox = 1 << 13,
} UILayoutNode_Flag;

internal u64              GetLayoutTreeFootprint   (u64 NodeCount);
internal ui_layout_tree * PlaceLayoutTreeInMemory  (u64 NodeCount, void *Memory);
internal u32              AllocateLayoutNode       (bit_field Flags, ui_subtree *Subtree);
internal void             UIEnd                    (void);

internal b32 IsMouseInsideOuterBox  (vec2_f32 MousePosition, u32 NodeIndex, ui_subtree *Subtree);

internal void UpdateNodeIfNeeded    (u32 NodeIndex, ui_subtree *Subtree);
internal void SetLayoutNodeFlags    (u32 NodeIndex, bit_field Flags, ui_subtree *Subtree);
internal void ClearLayoutNodeFlags  (u32 NodeIndex, bit_field Flags, ui_subtree *Subtree);


// ------------------------------------------------------------------------------------
// @Internal: Tree Queries

internal u32  UITreeFindChild    (u32 ParentIndex, u32 ChildIndex, ui_subtree *Subtree);
internal void UITreeAppendChild  (u32 ParentIndex, u32 ChildIndex, ui_subtree *Subtree);
internal void UITreeReserve      (u32 NodeIndex  , u32 Amount    , ui_subtree *Subtree);

// ------------------------------------------------------------------------------------
// @Internal: Layout Resources
//
// ui_scroll_region:
//  Opaque pointers the user shouldn't care about.
//
// scroll_region_params:
//  Parameters structure used when calling PlaceScrollRegionInMemory.
//  PixelPerLine: Specifies the speed at which the content will scroll.
//  Axis:         Specifies the axis along which the content scrolls.
//
// GetScrollRegionFootprint & PlaceScrollRegionInMemory
//   Used to initilialize in memory a scroll region. You may specify parameters to modify the behavior of the scroll region.
//   Note that you may re-use the same memory with different parameters to modify the behaviors with new parameters.
//
//   Example Usage:
//   u64   Size   = GetScrollRegionFootprint(); -> Get the size needed to allocate
//   void *Memory = malloc(Size);               -> Allocate (Do not check for 0s yet!)
//
//   scroll_region_params Params = {.PixelPerLine = ScrollSpeed, .Axis = Axis};  -> Prepare the params
//   ui_scroll_region *ScrollRegion = PlaceScrollRegionInMemory(Params, Memory); -> Allocate!
//   if(ScrollRegion)                                                            -> Now check if it succeeded

struct ui_scroll_region;

struct scroll_region_params
{
    f32         PixelPerLine;
    UIAxis_Type Axis;
};

internal u64                GetScrollRegionFootprint   (void);
internal ui_scroll_region * PlaceScrollRegionInMemory  (scroll_region_params Params, void *Memory);

// ------------------------------------------------------------------------------------

internal void ComputeSubtreeLayout  (ui_subtree *Subtree);
internal void UpdateSubtreeState    (ui_subtree *Subtree);

internal void PaintSubtree           (ui_subtree *Subtree);
