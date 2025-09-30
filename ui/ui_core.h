// [CONSTANTS]

typedef enum UIStyleNode_Flag
{
    UIStyleNode_NoFlag            = 0,
    UIStyleNode_HasSize           = 1 << 0,
    UIStyleNode_HasColor          = 1 << 1,
    UIStyleNode_HasPadding        = 1 << 2,
    UIStyleNode_HasSpacing        = 1 << 3,
    UIStyleNode_HasFontName       = 1 << 4,
    UIStyleNode_HasFontSize       = 1 << 5,
    UIStyleNode_HasSoftness       = 1 << 6,
    UIStyleNode_HasBorderColor    = 1 << 7,
    UIStyleNode_HasBorderWidth    = 1 << 8,
    UIStyleNode_HasCornerRadius   = 1 << 9,
    UIStyleNode_DirtySubtree      = 1 << 10,
    UIStyleNode_DirtySpine        = 1 << 11,
    UIStyleNode_StyleSetAtRuntime = 1 << 12,
} UIStyleNode_Flag;

typedef enum UIUnit_Type
{
    UIUnit_None    = 0,
    UIUnit_Float32 = 1,
    UIUnit_Percent = 2,
} UIUnit_Type;

typedef enum UIResize_Type
{
    UIResize_None = 0,
    UIResize_X    = 1,
    UIResize_Y    = 2,
    UIResize_XY   = 3,
} UIResize_Type;

typedef enum UIConstant_Type
{
    MAXIMUM_STYLE_NAME_LENGTH            = 32,
    MAXIMUM_STYLE_COUNT_PER_SUB_REGISTRY = 128,
    MAXIMUM_STYLE_FILE_SIZE              = Gigabyte(1),
} UIConstant_Type;

// [FORWARD DECLARATIONS]

typedef struct ui_node      ui_node;
typedef struct ui_font      ui_font;
typedef struct ui_text      ui_text;
typedef struct ui_tree      ui_tree;
typedef struct ui_style     ui_style;
typedef struct ui_character ui_character;
typedef struct ui_pipeline  ui_pipeline;

// [CORE TYPES]

typedef void ui_click_callback(ui_node *Node, ui_pipeline *Pipeline);

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

typedef struct ui_style
{
    // Color
    ui_color Color;
    f32      Opacity;

    // Layout
    vec2_unit  Size;
    ui_padding Padding;
    ui_spacing Spacing;

    // Borders/Corners
    ui_color         BorderColor;
    ui_corner_radius CornerRadius;
    f32              Softness;
    f32              BorderWidth;

    // Font
    f32 FontSize;
    union
    {
        ui_font    *Ref;
        byte_string Name;
    } Font;

    // Effects
    ui_style *ClickOverride;
    ui_style *HoverOverride;

    // Misc
    u32           Version;
    bit_field     Flags;
    ui_style_name Name;
} ui_style;

typedef struct ui_cached_style ui_cached_style;
struct ui_cached_style
{
    u32              Index;
    ui_cached_style *Next;
    ui_style         Style;
};

typedef struct ui_style_subregistry ui_style_subregistry;
typedef struct ui_style_subregistry
{
    b32                   HadError;
    u32                   Count;
    ui_style_subregistry *Next;
    u8                    FileName[OS_MAX_PATH];
    u64                   FileNameSize;

    ui_style_name   *CachedNames;
    ui_cached_style *CachedStyles;
    ui_cached_style *Sentinels;
    u8              *StringBuffer;
    u64              StringBufferOffset;
} ui_style_subregistry;

typedef struct ui_style_registry
{
    u32                   Count;
    ui_style_subregistry *First;
    ui_style_subregistry *Last;
} ui_style_registry;

// [Pipeline Types]

typedef struct ui_pipeline_params
{
    byte_string  *ThemeFiles;
    u32           ThemeCount;
    u32           TreeDepth;
    u32           TreeNodeCount;
    render_handle RendererHandle;
} ui_pipeline_params;

typedef struct ui_pipeline
{
    ui_tree           *StyleTree;
    ui_tree           *LayoutTree;
    ui_style_registry *StyleRegistery;

    ui_node      *DragCaptureNode;
    ui_node      *ResizeCaptureNode;
    UIResize_Type ResizeType;

    render_handle RendererHandle;

    memory_arena *FrameArena;
    memory_arena *StaticArena;
} ui_pipeline;

// [State]

typedef struct ui_state
{
    ui_font *First;
    ui_font *Last;
    u32      FontCount;

    memory_arena *StaticData;
} ui_state;

// [API]

// [Helpers]

internal ui_color         UIColor         (f32 R, f32 G, f32 B, f32 A);
internal ui_spacing       UISpacing       (f32 Horizontal, f32 Vertical);
internal ui_padding       UIPadding       (f32 Left, f32 Top, f32 Right, f32 Bot);
internal ui_corner_radius UICornerRadius  (f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight);
internal vec2_unit        Vec2Unit        (ui_unit U0, ui_unit U1);

internal b32 IsNormalizedColor(ui_color Color);

// [Components]

internal void UIWindow  (ui_style_name StyleName, ui_pipeline *Pipeline);
internal void UIButton  (ui_style_name StyleName, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void UILabel   (ui_style_name StyleName, byte_string Text, ui_pipeline *Pipeline);
internal void UIHeader  (ui_style_name StyleName, ui_pipeline *Pipeline);

#define UIHeaderEx(StyleName, Pipeline) DeferLoop(UIHeader(StyleName, Pipeline), UITree_PopParent(Pipeline->StyleTree))

// [Style]

internal ui_cached_style * UIGetStyleSentinel              (byte_string   Name, ui_style_subregistry *Registry);
internal ui_style_name     UIGetCachedName                 (byte_string   Name, ui_style_registry    *Registry);
internal ui_style_name     UIGetCachedNameFromSubregistry  (byte_string   Name, ui_style_subregistry *Registry);
internal ui_style          UIGetStyle                      (ui_style_name Name, ui_style_registry    *Registry);

internal void UISetColor(ui_node *Node, ui_color Color);

// [Pipeline]

internal ui_pipeline UICreatePipeline         (ui_pipeline_params Params);
internal void        UIPipelineBegin          (ui_pipeline *Pipeline);
internal void        UIPipelineExecute        (ui_pipeline *Pipeline, render_pass_list *PassList);

internal void        UIPipelineDragNodes      (vec2_f32 MouseDelta, ui_pipeline *Pipeline, ui_node *LRoot);
internal void        UIPipelineBuildDrawList  (ui_pipeline *Pipeline, render_pass *Pass, ui_node *SRoot, ui_node *LRoot);
