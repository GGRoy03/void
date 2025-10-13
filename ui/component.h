// [Components]

internal void  UIWindow      (byte_string Id, u32 Style, ui_pipeline *Pipeline);
internal void  UIButton      (byte_string Id, u32 Style, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void  UILabel       (byte_string Id, u32 Style, byte_string Text, ui_pipeline *Pipeline);
internal void  UIHeader      (byte_string Id, u32 Style, ui_pipeline *Pipeline);
internal void  UIScrollView  (byte_string Id, u32 Style, ui_pipeline *Pipeline);

// [Macros]

#define ui_id(Id) byte_string_literal(Id)

#define UIWindow(Style, Pipeline)            UIWindow_      (ui_id(""), Style, 0, Pipeline)
#define UIHeader(Style, Pipeline)            UIHeader_      (ui_id(""), Style, 0, Pipeline)
#define UIScrollView(Style, Pipeline)        UIScrollView_  (ui_id(""), Style, 0, Pipeline)
#define UIButton(Style, Callback, Pipeline)  UIButton_      (ui_id(""), Style, Callback, 0, Pipeline)
#define UILabel(Style, Text, Pipeline)       UILabel_       (ui_id(""), Style, Text, 0, Pipeline)

#define UIWindowID(Id, Style, Pipeline)            UIWindow_      (Id, Style, 0, Pipeline)
#define UIHeaderID(Id, Style, Pipeline)            UIHeader_      (Id, Style, 0, Pipeline)
#define UIScrollViewID(Id, Style, Pipeline)        UIScrollView_  (Id, Style, 0, Pipeline)
#define UIButtonID(Id, Style, Callback, Pipeline)  UIButton_      (Id, Style, Callback, 0, Pipeline)
#define UILabelID(Id, Style, Text, Pipeline)       UILabel_       (Id, Style, Text, 0, Pipeline)

#define UIWindowAttach(Style, Parent, Pipeline)            UIWindow_      (ui_id(""), Style, Parent, Pipeline)
#define UIHeaderAttach(Style, Parent, Pipeline)            UIHeader_      (ui_id(""), Style, Parent, Pipeline)
#define UIScrollViewAttach(Id, Style, Parent, Pipeline)    UIScrollView_  (ui_id(""), Style, Parent, Pipeline)
#define UIButtonAttach(Style, Callback, Parent, Pipeline)  UIButton_      (ui_id(""), Style, Parent, Callback, Pipeline)
#define UILabelAttach(Style, Text, Parent, Pipeline)       UILabel_       (ui_id(""), Style, Parent, Text, Pipeline)

#define UIWindowAttachID(Id, Style, Parent, Pipeline)           UIWindow_      (Id, Style, Parent, Pipeline)
#define UIHeaderAttachID(Id, Style, Parent, Pipeline)           UIHeader_      (Id, Style, Parent, Pipeline)
#define UIScrollViewAttachID(Id, Style, Parent, Pipeline)       UIScrollView_  (Id, Style, Parent, Pipeline)
#define UIButtonAttachID(Id, Style, Callback, Parent, Pipeline) UIButton_      (Id, Style, Callback, Parent, Pipeline)
#define UILabelAttachID(Id, Style, Text, Parent, Pipeline)      UILabel_       (Id, Style, Text, Parent, Pipeline)

#define UIScrollViewBlockID(Id, Style, Pipeline) DeferLoop(UIScrollView_(Id, Style, 0, Pipeline), PopLayoutNodeParent(Pipeline->LayoutTree))

// [Components - Internal Implementations]

internal void UIWindow_     (byte_string Id, u32 Style, ui_layout_node *Parent, ui_pipeline *Pipeline);
internal void UIButton_     (byte_string Id, u32 Style, ui_layout_node *Parent, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void UILabel_      (byte_string Id, u32 Style, ui_layout_node *Parent, byte_string Text, ui_pipeline *Pipeline);
internal void UIHeader_     (byte_string Id, u32 Style, ui_layout_node *Parent, ui_pipeline *Pipeline);
internal void UIScrollView_ (byte_string Id, u32 Style, ui_layout_node *Parent, ui_pipeline *Pipeline);
