// [Components]

internal void  UIWindow      (byte_string Id, u32 Style, ui_pipeline *Pipeline);
internal void  UIButton      (byte_string Id, u32 Style, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void  UILabel       (byte_string Id, u32 Style, byte_string Text, ui_pipeline *Pipeline);
internal void  UIHeader      (byte_string Id, u32 Style, ui_pipeline *Pipeline);
internal void  UIScrollView  (byte_string Id, u32 Style, ui_pipeline *Pipeline);

// [Macros]

#define ui_id(Id) byte_string_literal(Id)

#define UIWindow(Style, Pipeline)            UIWindow_      (ui_id(""), Style, Pipeline)
#define UIHeader(Style, Pipeline)            UIHeader_      (ui_id(""), Style, Pipeline)
#define UIScrollView(Style, Pipeline)        UIScrollView_  (ui_id(""), Style, Pipeline)
#define UIButton(Style, Callback, Pipeline)  UIButton_      (ui_id(""), Style, Callback, Pipeline)
#define UILabel(Style, Text, Pipeline)       UILabel_       (ui_id(""), Style, Text, Pipeline)

#define UIWindowID(Id, Style, Pipeline)            UIWindow_      (Id, Style, Pipeline)
#define UIHeaderID(Id, Style, Pipeline)            UIHeader_      (Id, Style, Pipeline)
#define UIScrollViewID(Id, Style, Pipeline)        UIScrollView_  (Id, Style, Pipeline)
#define UIButtonID(Id, Style, Callback, Pipeline)  UIButton_      (Id, Style, Callback,Pipeline)
#define UILabelID(Id, Style, Text, Pipeline)       UILabel_       (Id, Style, Text, Pipeline)

#define UISubtreeBlock(Parent, Pipeline) DeferLoop(UIEnterSubtree(Parent, Pipeline), UILeaveSubtree(Pipeline))

#define UIWindowBlock(Style, Pipeline) DeferLoop(UIWindow_(ui_id(""), Style, Pipeline), PopLayoutParentStack(&Pipeline->LayoutTree->Parents))

#define UIScrollViewBlockID(Id, Style, Pipeline) DeferLoop(UIScrollView_(Id, Style, Pipeline), PopLayoutParentStack(&Pipeline->LayoutTree->Parents))

// [Components - Internal Implementations]

internal void UIWindow_     (byte_string Id, u32 Style, ui_pipeline *Pipeline);
internal void UIButton_     (byte_string Id, u32 Style, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void UILabel_      (byte_string Id, u32 Style, byte_string Text, ui_pipeline *Pipeline);
internal void UIHeader_     (byte_string Id, u32 Style, ui_pipeline *Pipeline);
internal void UIScrollView_ (byte_string Id, u32 Style, ui_pipeline *Pipeline);
