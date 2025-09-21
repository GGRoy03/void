#pragma once

// [Constants]

#define OS_KeyboardButtonCount 256
#define OS_MouseButtonCount 5

typedef enum OSMouseButton_Type
{
    OSMouseButton_None  = 0,
    OSMouseButton_Left  = 1,
    OSMouseButton_Right = 2,
} OSMouseButton_Type;

typedef enum OSMessage_Severity
{
    OSMessage_None  = 0,
    OSMessage_Info  = 1,
    OSMessage_Warn  = 2,
    OSMessage_Error = 3,
    OSMessage_Fatal = 4,
} OSMessage_Severity;

// [Core Types]

typedef struct os_system_info
{
    u64 PageSize;
} os_system_info;

typedef struct os_button_state
{
    b32 EndedDown;
    u32 HalfTransitionCount;
} os_button_state;

typedef struct os_inputs
{
    os_button_state KeyboardButtons[OS_KeyboardButtonCount];
    f32             ScrollDelta;
    vec2_f32        MousePosition;
    vec2_f32        MouseDelta;
    os_button_state MouseButtons[OS_MouseButtonCount];
} os_inputs;

typedef struct os_file
{
    byte_string Content;
    u64         At;
    b32         FullyRead;
} os_file;

typedef struct os_glyph_layout
{
    vec2_i32 Size;
    vec2_f32 Offset;
    f32      AdvanceX;
} os_glyph_layout;

typedef struct os_glyph_raster_info
{
    b32      IsRasterized;
    rect_f32 SampleSource;
} os_glyph_raster_info;

// [FORWARD DECLARATION]

typedef struct gpu_font_objects gpu_font_objects;
typedef struct os_font_objects  os_font_objects;
typedef struct os_text_backend  os_text_backend;
typedef struct ui_font          ui_font;

// [CORE API]

// [Inputs]

internal void ProccessInputMessage(os_button_state *NewState, b32 IsDown);

// [Files]

internal b32  OSFileIsValid            (os_file *File);
internal void OSFileIgnoreWhiteSpaces  (os_file *File);
internal u8   OSFileGetChar            (os_file *File);
internal u8   OSFileGetNextChar        (os_file *File);

internal os_handle OSFindFile  (byte_string Path);
internal os_file   OSReadFile  (os_handle Handle, memory_arena *Arena);

// [Info]

internal os_system_info *OSGetSystemInfo  (void);
internal vec2_i32        OSGetClientSize  (void);

// [Memory]

internal void *OSReserveMemory  (u64 Size);
internal b32   OSCommitMemory   (void *Memory, u64 Size);
internal void  OSRelease        (void *Memory);

// [Misc]

internal b32   OSUpdateWindow  (void);
internal void  OSSleep         (u32 Milliseconds);
external void  OSLogMessage    (byte_string ANSISequence, OSMessage_Severity Severity);
internal void  OSAbort         (i32 ExitCode);

// [Text]

external b32                  OSAcquireFontObjects  (byte_string Name, f32 Size, gpu_font_objects *GPUObjects, os_font_objects *OSObjects);
external void                 OSReleaseFontObjects  (os_font_objects *Objects);
external os_glyph_layout      OSGetGlyphLayout      (u8 Character, os_font_objects *FontObjects, vec2_f32 TextureSize, f32 Size);
external os_glyph_raster_info OSRasterizeGlyph      (u8 Character, rect_f32 Rect, os_font_objects *OSFontObjects, gpu_font_objects *GPUFontObjects, render_handle RendererHandle);
extern   f32                  OSGetLineHeight       (f32 FontSize, os_font_objects *OSFontObjects);