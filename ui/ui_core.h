// [CONSTANTS]

typedef enum UIUnit_Type
{
    UIUnit_None    = 0,
    UIUnit_Float32 = 1,
    UIUnit_Percent = 2,
} UIUnit_Type;

typedef enum UIIntent_Type
{
    UIIntent_None     = 0,
    UIIntent_Hover    = 1,
    UIIntent_ResizeX  = 2,
    UIIntent_ResizeY  = 3,
    UIIntent_ResizeXY = 4,
    UIIntent_Drag     = 5,
} UIIntent_Type;

// [FORWARD DECLARATIONS]

typedef struct ui_style       ui_style;
typedef struct ui_layout_node ui_layout_node;
typedef struct ui_layout_tree ui_layout_tree;
typedef struct ui_pipeline    ui_pipeline;

typedef void ui_click_callback(ui_layout_node *Node, ui_pipeline *Pipeline);

// [CORE TYPES]

typedef struct ui_color
{
    f32 R;
    f32 G;
    f32 B;
    f32 A;
} ui_color;

typedef struct ui_corner_radius
{
    f32 TopLeft;
    f32 TopRight;
    f32 BotLeft;
    f32 BotRight;
} ui_corner_radius;

typedef struct ui_spacing
{
    f32 Horizontal;
    f32 Vertical;
} ui_spacing;

typedef struct ui_padding
{
    f32 Left;
    f32 Top;
    f32 Right;
    f32 Bot;
} ui_padding;

typedef struct ui_unit
{
    UIUnit_Type Type;
    union
    {
        f32 Float32;
        f32 Percent;
    };
} ui_unit;

typedef struct vec2_unit
{
    union
    {
        struct { ui_unit X; ui_unit Y; };
        ui_unit Values[2];
    };
} vec2_unit;

typedef struct vec4_unit
{
    union
    {
        struct { ui_unit X; ui_unit Y; ui_unit Z; ui_unit W; };
        ui_unit Values[4];
    };
} vec4_unit;

// NOTE: Must be padded to 16 bytes alignment.
typedef struct ui_rect
{
    rect_f32         RectBounds;
    rect_f32         AtlasSampleSource;
    ui_color         Color;
    ui_corner_radius CornerRadii;
    f32              BorderWidth, Softness, SampleAtlas, _P0; // Style Params
} ui_rect;

typedef struct ui_style_name
{
    byte_string Value;
} ui_style_name;

typedef struct ui_pipeline_params
{
    byte_string *ThemeFiles;
    u32          ThemeCount;
    u32          TreeDepth;
    u32          TreeNodeCount;
} ui_pipeline_params;

typedef struct ui_pipeline
{
    // State
    ui_layout_tree    *LayoutTree;
    ui_style_registry *Registry;
    ui_layout_node    *CapturedNode;
    UIIntent_Type      Intent;

    // Memory
    memory_arena *FrameArena;
    memory_arena *StaticArena;
} ui_pipeline;

// One of the three program global (GAME, UI, RENDERER)

typedef struct ui_state
{
    // Fonts
    ui_font *First;
    ui_font *Last;
    u32      FontCount;

    memory_arena *StaticData;
} ui_state;

global ui_state UIState;

// [Helpers]

internal ui_color         UIColor         (f32 R, f32 G, f32 B, f32 A);
internal ui_spacing       UISpacing       (f32 Horizontal, f32 Vertical);
internal ui_padding       UIPadding       (f32 Left, f32 Top, f32 Right, f32 Bot);
internal ui_corner_radius UICornerRadius  (f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight);
internal vec2_unit        Vec2Unit        (ui_unit U0, ui_unit U1);

internal b32 IsNormalizedColor(ui_color Color);

// [Pipeline]

internal ui_pipeline UICreatePipeline         (ui_pipeline_params Params);
internal void        UIPipelineBegin          (ui_pipeline *Pipeline);
internal void        UIPipelineExecute        (ui_pipeline *Pipeline, render_pass_list *PassList);
internal void        UIPipelineBuildDrawList  (ui_pipeline *Pipeline, render_pass *Pass, ui_layout_node *LayoutRoot);
