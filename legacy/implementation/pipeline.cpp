// [Pipeline]

typedef struct ui_frame_pipeline_stats
{
    void *TBD;
} ui_frame_pipeline_stats;

// Forward Declaration
typedef struct ui_style_registery ui_style_registery;
typedef struct ui_style_tree ui_style_tree;
typedef struct ui_style_update_queue ui_style_update_queue;

typedef struct ui_frame_pipeline
{
    ui_frame_pipeline_stats Stats;

    ui_style_registery    *StyleRegistery;
    ui_style_tree         *StyleTree;
    ui_style_update_queue *Queue;
} ui_frame_pipeline;

// NOTE: Temporary global for simplicity.
static ui_frame_pipeline *ActivePipeline;

// Public API

static void UISetPipeline  (ui_frame_pipeline *Pipeline);
static void UIEndPipeline  (void);

// Public API Implementation

static void
UISetPipeline(ui_frame_pipeline *Pipeline)
{
    ActivePipeline = Pipeline;
}

static void
UIEndPipeline(void)
{
    ActivePipeline = NULL;
}

// [Style Tree]

typedef enum UIStyleUpdate_Type
{
    UIStyleUpdate_None    = 0,
    UIStyleUpdate_Color   = 1,
    UIStyleUpdate_Border  = 2,
    UIStyleUpdate_Size    = 3,
    UIStyleUpdate_Padding = 4,
    UIStyleUpdate_Spacing = 5,
} UIStyleUpdate_Type;

typedef struct ui_color_style
{
    bool    SetAtRuntime;
    cim_f32 R;
    cim_f32 G;
    cim_f32 B;
    cim_f32 A;
} ui_color_style;

typedef struct ui_padding_style
{
    bool    SetAtRuntime;
    cim_f32 Left;
    cim_f32 Top;
    cim_f32 Right;
    cim_f32 Bottom;
} ui_padding_style;

typedef struct ui_spacing_style
{
    bool    SetAtRuntime;
    cim_f32 Horizontal;
    cim_f32 Vertical;
} ui_spacing_style;

typedef struct ui_size_style
{
    bool    SetAtRuntime;
    cim_f32 Width;
    cim_f32 Height;
} ui_size_style;

typedef struct ui_border_style
{
    bool    SetAtRuntime;
    cim_f32 Width;
} ui_border_style;

typedef struct ui_style_name
{
    char *Pointer;
} ui_style_name;

typedef struct ui_style
{
    ui_color_style   Color;
    ui_border_style  Border;
    ui_size_style    Size;
    ui_padding_style Padding;
    ui_spacing_style Spacing;
} ui_style;

typedef struct ui_style_node
{
    ui_style      Style;
    ui_style_name StyleName;

    ui_style_node *Parent;
    ui_style_node *Next;
    ui_style_node *Prev;
    ui_style_node *First;
    ui_style_node *Last;
} ui_style_node;

typedef struct ui_style_tree_params
{
	cim_u32 NestedDepth;
	cim_u32 NodeCount;
} ui_style_tree_params;

typedef struct ui_style_tree
{
    ui_style_node *NodePool;
    cim_u32        NodeCount;
    cim_u32        NodeCapacity;

    ui_style_node **ParentStack;
    cim_u32         ParentTop;

    cim_u32 NestedDepth;
} ui_style_tree;

typedef struct ui_style_update
{
    UIStyleUpdate_Type Type;
    union
    {
        ui_color_style   Color;
        ui_border_style  Border;
        ui_size_style    Size;
        ui_padding_style Padding;
        ui_spacing_style Spacing;
    } New;

    ui_style_name InternedStyleName;
} ui_style_update;

typedef struct ui_style_update_queue_params
{
    cim_u32 UpdatesInFlight;
} ui_style_update_queue_params;

typedef struct ui_style_update_queue
{
    ui_style_update *Updates;
    cim_u32          NextUpdateToRead;
    cim_u32          NextUpdateToWrite;

    cim_u32 UpdateCount;
    cim_u32 UpdateGoal;

    cim_u32 UpdatesInFlight;
} ui_style_update_queue;

typedef struct ui_style_registery
{
    ui_style_name *InternedStrings;
    ui_style      *Styles;
    cim_u32        Count;
} ui_style_registery;

// Internal API

static ui_style_node  *AllocateStyleNode  (ui_style_tree *Tree);

static ui_style *GetStyleFromName(ui_style_name StyleName);

static ui_style_update  *PopStyleUpdate                (ui_style_update_queue *Queue);
static void              PushStyleUpdate               (ui_style_update_queue *Queue, ui_style_update Update);
static void              ProcessStyleUpdates           (ui_style_update_queue *Queue);

static size_t                 GetStyleUpdateQueueFootprint  (ui_style_update_queue_params Params);
static ui_style_update_queue *PackStyleUpdateQueue          (ui_style_update_queue_params Params, void *Memory);
static size_t                 GetStyleTreeFootprint         (ui_style_tree_params Params);
static ui_style_tree         *PackStyleTree                 (ui_style_tree_params Params, void *Memory);

// User API

static ui_style_name UIGetStyleName(const char *UTF8Name);

static void UISetColor    (ui_style_name StyleName, ui_color_style   Color  );
static void UISetBorder   (ui_style_name StyleName, ui_border_style  Border );
static void UISetSize     (ui_style_name StyleName, ui_size_style    Size   );
static void UISetPadding  (ui_style_name StyleName, ui_padding_style Padding);
static void UISetSpacing  (ui_style_name StyleName, ui_spacing_style Spacing);

// Internal API Implementation

static ui_style_node *
AllocateStyleNode(ui_style_tree *Tree)
{
    ui_style_node *Result = NULL;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->NodePool + Tree->NodeCount++;
    }

    return Result;
}

static ui_style *
GetStyleFromName(ui_style_name StyleName)
{
    ui_style_registery *Registery = ActivePipeline->StyleRegistery;
    Cim_Assert(Registery && StyleName.Pointer);

    ui_style *Result = NULL;

    for (cim_u32 Index = 0; Index < Registery->Count; Index++)
    {
        if (Registery->InternedStrings[Index].Pointer == StyleName.Pointer)
        {
            Result = &Registery->Styles[Index];
            break;
        }
    }

    return Result;
}

static ui_style_update *
PopStyleUpdate(ui_style_update_queue *Queue)
{
    ui_style_update *Result = NULL;

    if (Queue->NextUpdateToRead != Queue->NextUpdateToWrite)
    {
        Result = Queue->Updates + Queue->NextUpdateToRead;

        Queue->NextUpdateToRead = (Queue->NextUpdateToRead + 1) % Queue->UpdatesInFlight;
        Queue->UpdateGoal      -= 1;
    }

    return Result;
}

static void
PushStyleUpdate(ui_style_update_queue *Queue, ui_style_update NewStyle)
{
    Cim_Assert(((Queue->NextUpdateToWrite + 1) % Queue->UpdatesInFlight) != Queue->NextUpdateToRead);

    Queue->Updates[Queue->NextUpdateToWrite] = NewStyle;

    Queue->NextUpdateToWrite = (Queue->NextUpdateToWrite + 1) % Queue->UpdatesInFlight;
    Queue->UpdateGoal       += 1;
}

static void
ProcessStyleUpdates(ui_style_update_queue *Queue)
{
    while (Queue->UpdateCount != Queue->UpdateGoal)
    {
        ui_style_update *Update = PopStyleUpdate(Queue);
        ui_style        *Style  = GetStyleFromName(Update->InternedStyleName);
        Cim_Assert(Style && Update);

        switch (Update->Type)
        {

        case UIStyleUpdate_Color  : Style->Color   = Update->New.Color;   Style->Color.SetAtRuntime   = true; break;
        case UIStyleUpdate_Border : Style->Border  = Update->New.Border;  Style->Border.SetAtRuntime  = true; break;
        case UIStyleUpdate_Size   : Style->Size    = Update->New.Size;    Style->Size.SetAtRuntime    = true; break;
        case UIStyleUpdate_Padding: Style->Padding = Update->New.Padding; Style->Padding.SetAtRuntime = true; break;
        case UIStyleUpdate_Spacing: Style->Spacing = Update->New.Spacing; Style->Spacing.SetAtRuntime = true; break;

        default: Cim_Assert(!"Invalid style update"); break;
        }
    }

    Queue->UpdateCount = 0;
    Queue->UpdateGoal  = 0;
}

static size_t
GetStyleUpdateQueueFootprint(ui_style_update_queue_params Params)
{
    size_t UpdatesSize = Params.UpdatesInFlight * sizeof(ui_style_update);
    size_t Result      = sizeof(ui_style_update_queue) + UpdatesSize;
    return Result;
}

static ui_style_update_queue *
PackStyleUpdateQueue(ui_style_update_queue_params Params, void *Memory)
{
    Cim_Assert(Params.UpdatesInFlight > 0);
    Cim_Assert(Cim_IsPowerOfTwo(Params.UpdatesInFlight));

    ui_style_update_queue *Result = NULL;

    if (Memory)
    {
        ui_style_update *Updates = (ui_style_update *)Memory;
        Result          = (ui_style_update_queue *)(Updates + Params.UpdatesInFlight);
        Result->Updates = Updates;

        Result->NextUpdateToRead  = 0;
        Result->NextUpdateToWrite = 0;
        Result->UpdateCount       = 0;
        Result->UpdateGoal        = 0;

        Result->UpdatesInFlight = Params.UpdatesInFlight;
    }

    return Result;
}

static size_t
GetStyleTreeFootprint(ui_style_tree_params Params)
{
    size_t ParentStackSize = Params.NestedDepth * sizeof(ui_style_node*);
    size_t NodePoolSize    = Params.NodeCount   * sizeof(ui_style_node);
    size_t Result          = sizeof(ui_style_tree) + ParentStackSize + NodePoolSize;
    return Result;
}

static ui_style_tree *
PackStyleTree(ui_style_tree_params Params, void *Memory)
{
    Cim_Assert(Params.NestedDepth > 0);
    Cim_Assert(Params.NodeCount   > 0);

    ui_style_tree *Result = NULL;

    if(Memory)
    {
        ui_style_node *NodePool = (ui_style_node *)Memory;
        Result              = (ui_style_tree *)(NodePool + Params.NodeCount);
        Result->NodePool    = NodePool;
        Result->ParentStack = (ui_style_node**)(Result + 1);

        Result->ParentTop    = 0;
        Result->NodeCount    = 0;
        Result->NodeCapacity = Params.NodeCount;

        Result->NestedDepth = Params.NestedDepth;
    }

    return Result;
}

// Public API Implementation

static ui_style_name 
UIGetStyleName(const char *UTF8Name)
{
    ui_style_registery *Registery = ActivePipeline->StyleRegistery;
    Cim_Assert(Registery);

    ui_style_name Result = {};

    for (cim_u32 Index = 0; Index < Registery->Count; Index++)
    {
        if (strcmp(UTF8Name, Registery->InternedStrings[Index].Pointer) == 0)
        {
            Result.Pointer = Registery->InternedStrings[Index].Pointer;
            break;
        }
    }

    return Result;
}

static void 
UISetColor(ui_style_name StyleName, ui_color_style Color)
{
    ui_style_update_queue *Queue = ActivePipeline->Queue;
    Cim_Assert(Queue);

    ui_style_update Update = {};
    Update.InternedStyleName = StyleName;
    Update.New.Color         = Color;
    Update.Type              = UIStyleUpdate_Color;

    PushStyleUpdate(Queue, Update);
}

static void 
UISetBorder(ui_style_name StyleName, ui_border_style Border)
{
    ui_style_update_queue *Queue = ActivePipeline->Queue;
    Cim_Assert(Queue);

    ui_style_update Update = {};
    Update.InternedStyleName = StyleName;
    Update.New.Border        = Border;
    Update.Type              = UIStyleUpdate_Border;

    PushStyleUpdate(Queue, Update);
}

static void 
UISetSize(ui_style_name StyleName, ui_size_style Size)
{
    ui_style_update_queue *Queue = ActivePipeline->Queue;
    Cim_Assert(Queue);

    ui_style_update Update = {};
    Update.InternedStyleName = StyleName;
    Update.New.Size          = Size;
    Update.Type              = UIStyleUpdate_Size;

    PushStyleUpdate(Queue, Update);
}

static void 
UISetPadding(ui_style_name StyleName, ui_padding_style Padding)
{
    ui_style_update_queue *Queue = ActivePipeline->Queue;
    Cim_Assert(Queue);

    ui_style_update Update = {};
    Update.InternedStyleName = StyleName;
    Update.New.Padding       = Padding;
    Update.Type              = UIStyleUpdate_Padding;

    PushStyleUpdate(Queue, Update);
}

static void 
UISetSpacing(ui_style_name StyleName, ui_spacing_style Spacing)
{
    ui_style_update_queue *Queue = ActivePipeline->Queue;
    Cim_Assert(Queue);

    ui_style_update Update = {};
    Update.InternedStyleName = StyleName;
    Update.New.Spacing       = Spacing;
    Update.Type              = UIStyleUpdate_Spacing;

    PushStyleUpdate(Queue, Update);
}

// [Layout]

typedef enum UILayoutNode_Type
{
    UILayoutNode_None   = 0,
    UILayoutNode_Window = 1,
    UILayoutNode_Button = 2,
    UILayoutNode_Label  = 3,
} UILayoutNode_Type;

typedef enum UILayoutDirty_Flag
{
    UILayoutDirty_None      = 0,
    UILayoutDirty_PreOrder  = 1 << 0,
    UILayoutDirty_PostOrder = 1 << 1,
} UILayoutDirty_Flag;

typedef struct ui_layout_group
{
    ui_style_name    OriginalStyle;
    ui_size_style    Size;
    ui_padding_style Padding;
    ui_spacing_style Spacing;
} ui_layout_group;

typedef struct ui_layout_store
{

} ui_layout_store;

typedef struct ui_layout_padding
{
    cim_f32 Left;
    cim_f32 Top;
    cim_f32 Right;
    cim_f32 Bottom;
} ui_layout_padding;

typedef struct ui_layout_spacing
{
    cim_f32 Horizontal;
    cim_f32 Vertical;
} ui_layout_spacing;

typedef void ui_button_callback(void *UserData);
typedef struct ui_layout_button
{
    ui_button_callback *UserCallback;
    void               *UserData;
} ui_layout_button;

typedef struct ui_layout_node2
{
    UILayoutNode_Type Type;
    union
    {
        ui_layout_button Button;
    } Data;

    ui_layout_group *Layout; // NOTE: Pointer into the store.

    cim_f32 Width;
    cim_f32 Height;
    cim_f32 ClientX;
    cim_f32 ClientY;

    ui_layout_node2 *First;
    ui_layout_node2 *Last;
    ui_layout_node2 *Parent;
    ui_layout_node2 *Next;
    ui_layout_node2 *Prev;
} ui_layout_node2;

typedef struct ui_layout_dirty_node
{
    cim_bit_field    Flags;
    ui_layout_node2 *Node;
} ui_layout_dirty_node;

typedef struct ui_layout_dirty_queue
{
    ui_layout_dirty_node DirtyNodes[16];
    cim_u32              DirtyCount;
} ui_layout_dirty_queue;

typedef struct ui_layout_tree
{
    // Internal Arena.
    ui_layout_node2 *NodePool;
    cim_u32          At;
    cim_u32          Size;

    // Meta-Data
    cim_u32 NodeCount;

    // Parent Stack
    ui_layout_node2 *ParentStack[32];
    cim_u32          ParentTop;

    // Dirty Queue (Incremental Layout)
    ui_layout_dirty_queue DirtyQueue;
} ui_layout_tree;

static ui_layout_tree UITree;

static ui_layout_node2 *
AppendNodeToTree(ui_layout_tree *Tree)
{
    ui_layout_node2 *Result = NULL;

    if (Tree->At < Tree->Size)
    {
        Result = Tree->NodePool + Tree->NodeCount++;
    }

    return Result;
}

static ui_layout_node2 *
GetParentNode(ui_layout_tree *Tree)
{
    ui_layout_node2 *Result = NULL;

    if (Tree->ParentTop >= 0 && Tree->ParentTop < 32)
    {
        Result = Tree->ParentStack[Tree->ParentTop];
    }

    return Result;
}

static void
LinkChildren(ui_layout_node2 *ParentNode, ui_layout_node2 *ChildNode)
{
    if (ParentNode->Last == NULL)
    {
        ParentNode->First = ChildNode;
        ParentNode->Last = ChildNode;
    }
    else
    {
        ChildNode->Prev = ParentNode->Last;
        ParentNode->Last->Next = ChildNode;
        ParentNode->Last = ChildNode;
    }
}

static void
UpdateLayoutTree()
{
    ui_layout_dirty_queue *Queue = &UITree.DirtyQueue;

    while (Queue->DirtyCount > 0)
    {
        ui_layout_dirty_node *DirtyNode = Queue->DirtyNodes + 0;

        if (DirtyNode->Flags & UILayoutDirty_PreOrder)
        {
            ui_layout_node2 *Node  = DirtyNode->Node;
            ui_style        *Style = GetStyleFromName(Node->StyleName);

            switch (Node->Type)
            {

            case UILayoutNode_Window:
            {
                // Here we must simply implement what we want. For example, if we want the window
                // to lay its children in a column, then why not. Applying padding and spacing.
                // No fancy layouts, nothing. Just simple placement.

                cim_f32 AvailableWidth  = Node->Width  - (Style->Padding.Left + Style->Padding.Right);
                cim_f32 AvailableHeight = Node->Height - (Style->Padding.Top  + Style->Padding.Bottom);

                cim_f32 ClientX = Node->ClientX + Style->Padding.Left;
                cim_f32 ClientY = Node->ClientY + Style->Padding.Top;

                ui_layout_node2 *ChildNode = Node->First;
                while (ChildNode)
                {
                    ui_style *ChildStyle = GetStyleFromName(ChildNode->StyleName);

                    ChildNode->ClientX = ClientX;
                    ChildNode->ClientY = ClientY;
                    ChildNode->Width   = ChildStyle->Size.Width  <= AvailableWidth  ? ChildStyle->Size.Width  : AvailableWidth;
                    ChildNode->Height  = ChildStyle->Size.Height <= AvailableHeight ? ChildStyle->Size.Height : AvailableHeight;

                    ClientY         += ChildNode->Height + Style->Spacing.Vertical;
                    AvailableHeight -= ChildNode->Height + Style->Spacing.Vertical;

                    ChildNode = ChildNode->Next;
                }

            } break;

            case UILayoutNode_Button:
            {
                // We do not yet handle this possibility.
            } break;

            }
        }

        if (DirtyNode->Flags & UILayoutDirty_PostOrder)
        {
            // We do not yet handle this possibility.
        }

        --Queue->DirtyCount;
    }
}

// [UI Builder]

// Internal API

static void           PushParentStyleNode  (ui_style_tree *Tree, ui_style_node *Parent);
static ui_style_node *GetParentStyleNode   (ui_style_tree *Tree);
static void           LinkStyleNode        (ui_style_node *Parent, ui_style_node *Child);

// Public API

static void UIWindow3  (ui_style_name Name);
static void UIButton3  (ui_style_name Name);

// Internal API Implemenation

static ui_style_node *
GetParentStyleNode(ui_style_tree *Tree)
{
    ui_style_node *Result = NULL;

    if (Tree->ParentTop < Tree->NestedDepth)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

static void
PushParentStyleNode(ui_style_tree *Tree, ui_style_node *Parent)
{
    if (Tree->ParentTop < Tree->NestedDepth)
    {
        Tree->ParentStack[Tree->ParentTop++] = Parent;
    }
}

static void
LinkStyleNode( ui_style_node *Parent, ui_style_node *Child)
{
    if (Parent->First == NULL)
    {
        Parent->First = Child;
        Parent->Last  = Child;
    }
    else
    {
        Parent->Last->Next = Child;
        Child->Prev        = Parent->Last;	
        Parent->Last       = Child;
    }
}

// Public API Implementation

static void 
UIWindow3(ui_style_name Name)
{
    ui_style_tree *Tree = ActivePipeline->StyleTree;
    Cim_Assert(Tree);

    ui_style *Style = GetStyleFromName(Name);
    Cim_Assert(Style);

    ui_style_node *StyleNode = AllocateStyleNode(Tree);
    StyleNode->Style     = *Style;
    StyleNode->StyleName = Name;
    StyleNode->Parent    = NULL;
    StyleNode->Prev      = NULL;
    StyleNode->Next      = NULL;
    StyleNode->First     = NULL;
    StyleNode->Prev      = NULL;

    PushParentStyleNode(Tree, StyleNode);
}

static void
UIButton3(ui_style_name Name)
{
    ui_style_tree *Tree = ActivePipeline->StyleTree;
    Cim_Assert(Tree);

    ui_style *Style = GetStyleFromName(Name);
    Cim_Assert(Style);

    ui_style_node *StyleNode = AllocateStyleNode(Tree);
    StyleNode->Style     = *Style;
    StyleNode->StyleName = Name;
    StyleNode->Parent    = GetParentStyleNode(Tree);
    StyleNode->Prev      = NULL;
    StyleNode->Next      = NULL;
    StyleNode->First     = NULL;
    StyleNode->Prev      = NULL;

    LinkStyleNode(StyleNode->Parent, StyleNode);
}