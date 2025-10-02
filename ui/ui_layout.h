// [CONSTANTS]

typedef enum UILayoutNode_Flag
{
    UILayoutNode_NoFlag                  = 0,
    UILayoutNode_PlaceChildrenVertically = 1 << 0,
    UILayoutNode_DrawBackground          = 1 << 1,
    UILayoutNode_DrawBorders             = 1 << 2,
    UILayoutNode_DrawText                = 1 << 3,
    UILayoutNode_HasClip                 = 1 << 4,
    UILayoutNode_IsHovered               = 1 << 5,
    UILayoutNode_IsClicked               = 1 << 6,
    UILayoutNode_IsDraggable             = 1 << 7,
    UILayoutNode_IsResizable             = 1 << 8,
} UILayoutNode_Flag;

// [FORWARD DECLARATION]

typedef struct ui_text ui_text;

// [CORE TYPES]

typedef struct ui_hit_test_result
{
    ui_layout_node *Node;
    UIIntent_Type   Intent;
    b32             Success;
} ui_hit_test_result;

typedef struct ui_layout_box
{
    // Output
    f32 FinalX;
    f32 FinalY;
    f32 FinalWidth;
    f32 FinalHeight;

    // Layout Info
    ui_unit    Width;
    ui_unit    Height;
    ui_spacing Spacing;
    ui_padding Padding;

    // Params
    rect_f32   Clip;
    matrix_3x3 Transform;

    // Misc
    bit_field Flags;

    // Misc (Should text be callback based?)
    ui_text           *Text;
    ui_click_callback *ClickCallback;
} ui_layout_box;

// [CORE API]

internal ui_hit_test_result HitTestLayout  (vec2_f32 MousePosition, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);

internal void DragUISubtree    (vec2_f32 Delta, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void ResizeUISubtree  (vec2_f32 Delta, ui_layout_node *LayoutNode, ui_pipeline *Pipeline);

internal void TopDownLayout  (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
