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

static uint64_t         GetLayoutTreeAlignment   (void);
static uint64_t         GetLayoutTreeFootprint   (uint64_t NodeCount);
static ui_layout_tree * PlaceLayoutTreeInMemory  (uint64_t NodeCount, void *Memory);
static uint32_t         AllocateLayoutNode       (uint32_t Flags, ui_layout_tree *Tree);
static bool             PushLayoutParent         (uint32_t Index, ui_layout_tree *Tree, memory_arena *Arena);
static bool             PopLayoutParent          (uint32_t Index, ui_layout_tree *Tree);
static void             PreOrderMeasureTree      (ui_layout_tree *Tree, memory_arena *Arena);
static void             PostOrderMeasureTree     (uint32_t NodeIndex , ui_layout_tree *Tree);
static void             PlaceLayoutTree          (ui_layout_tree *Tree, memory_arena *Arena);

static void SetLayoutNodeFlags    (uint32_t NodeIndex, uint32_t Flags, ui_layout_tree *Tree);
static void ClearLayoutNodeFlags  (uint32_t NodeIndex, uint32_t Flags, ui_layout_tree *Tree);

// -----------------------------------------------------------------------------------
// @internal: Node Queries

static rect_float
GetNodeOuterRect(ui_layout_node *Node);

static rect_float
GetNodeInnerRect(ui_layout_node *Node);

static rect_float
GetNodeContentRect(ui_layout_node *Node);

static void
SetNodeProperties(uint32_t NodeIndex, uint32_t StyleIndex, const ui_cached_style &Cached, ui_layout_tree *Tree);

// ------------------------------------------------------------------------------------
// @internal: Tree Queries

static uint32_t UITreeFindChild    (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);
static void     UITreeAppendChild  (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);
static void     UITreeReserve      (uint32_t NodeIndex  , uint32_t Amount    , ui_layout_tree *Tree);

// ------------------------------------------------------------------------------------
// @internal: Layout Resources
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
//   Note that you may re-use the same memory with different parameters to modify the behavior with new parameters.
//
//   Example Usage:
//   uint64_t   Size   = GetScrollRegionFootprint(); -> Get the size needed to allocate
//   void *Memory = malloc(Size);               -> Allocate (Do not check for null yet!)
//
//   scroll_region_params Params = {.PixelPerLine = ScrollSpeed, .Axis = Axis};  -> Prepare the params
//   ui_scroll_region *ScrollRegion = PlaceScrollRegionInMemory(Params, Memory); -> Initialize
//   if(ScrollRegion)                                                            -> Now check if it succeeded

struct ui_scroll_region;

struct scroll_region_params
{
    float       PixelPerLine;
    UIAxis_Type Axis;
};

static uint64_t           GetScrollRegionFootprint   (void);
static ui_scroll_region * PlaceScrollRegionInMemory  (scroll_region_params Params, void *Memory);

// ------------------------------------------------------------------------------------

static void ComputeSubtreeLayout  (void *Subtree);
static void UpdateSubtreeState    (void *Subtree);

static void PaintSubtree           (void *Subtree);
