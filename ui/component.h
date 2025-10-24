// [Macros]

#define ui_id(Id) byte_string_literal(Id)

#define UIWindow(S, P)            DeferLoop(UIWindow_(S, P)    , UIEnd(P))
#define UIScrollView(S, P)        DeferLoop(UIScrollView_(S, P), UIEnd(P))
#define UILabel(S, Text, P)       UILabel_       (S, Text, P)

// [Components - Internal Implementations]

internal ui_node UIWindow_     (u32 Style, ui_pipeline *Pipeline);
internal ui_node UILabel_      (u32 Style, byte_string Text, ui_pipeline *Pipeline);
internal ui_node UIScrollView_ (u32 Style, ui_pipeline *Pipeline);
