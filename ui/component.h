// [Macros]

#define ui_id(Id)  byte_string_literal(Id)
#define UIBlock(X) DeferLoop(X, UIEnd())


internal ui_node_chain * UIWindow_     (u32 Style);
internal ui_node_chain * UILabel_      (u32 Style, byte_string Text);
internal ui_node_chain * UIScrollView_ (u32 Style);
