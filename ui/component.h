internal ui_node UIWindow             (u32 Style);
internal ui_node UIScrollableContent  (UIAxis_Type Axis, u32 StyleIndex);
internal ui_node UILabel              (byte_string Text, u32 Style);
internal ui_node UITextInput          (u8 *Buffer, u64 BufferSize, u32 StyleIndex);
internal ui_node UIImage              (byte_string Path, byte_string Group, u32 Style);
