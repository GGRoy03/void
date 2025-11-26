static ui_node * UIWindow             (uint32_t Style);
static ui_node * UIScrollableContent  (float ScrollSpeed, UIAxis_Type Axis, uint32_t StyleIndex);
static ui_node * UILabel              (byte_string Text, uint32_t Style);
static ui_node * UITextInput          (uint8_t *Buffer, uint64_t BufferSize, uint32_t StyleIndex);
static ui_node * UIImage              (byte_string Path, byte_string Group, uint32_t Style);
