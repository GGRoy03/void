#pragma once

// [Constants]

#define OS_KeyboardButtonCount 256
#define OS_MouseButtonCount 5
#define OS_MAX_PATH 256

typedef enum OSMouseButton_Type
{
    OSMouseButton_None  = 0,
    OSMouseButton_Left  = 1,
    OSMouseButton_Right = 2,
} OSMouseButton_Type;

typedef enum OSCursor_Type
{
    OSCursor_None                      = 0,
    OSCursor_Default                   = 1,
    OSCursor_EditText                  = 2,
    OSCursor_Waiting                   = 3,
    OSCursor_Loading                   = 4,
    OSCursor_ResizeVertical            = 5,
    OSCursor_ResizeHorizontal          = 6,
    OSCursor_ResizeDiagonalLeftToRight = 7,
    OSCursor_ResizeDiagonalRightToLeft = 8,
    OSCursor_GrabHand                  = 9,
} OSCursor_Type;

typedef enum OSResource_Type
{
    OSResource_None = 0,
    OSResource_File = 1,
} OSResource_Type;

// [FORWARD DECLARATION]

typedef struct gpu_font_objects  gpu_font_objects;
typedef struct os_font_objects   os_font_objects;
typedef struct os_text_backend   os_text_backend;
typedef struct ui_font           ui_font;
typedef struct ui_style_registry ui_style_registry;

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

typedef struct os_read_file
{
    byte_string Content;
    u64         At;
    b32         FullyRead;
} os_read_file;

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

typedef struct os_watched_registry os_watched_registry;
struct os_watched_registry
{
    os_watched_registry *Next;
    ui_style_registry   *Registry;
};

// [CORE API]

// [Inputs]

internal void ProcessInputMessage(os_button_state *NewState, b32 IsDown);

// [Files]

internal b32   IsValidFile      (os_read_file *File);
internal void  SkipWhiteSpaces  (os_read_file *File);
internal u8  * PeekFilePointer  (os_read_file *File);
internal u8    PeekFile         (os_read_file *File, u32 Offset);
internal void  AdvanceFile      (os_read_file *File, u32 Count);

internal os_handle    OSFindFile     (byte_string Path);
internal u64          OSFileSize     (os_handle Handle);
internal os_read_file OSReadFile     (os_handle Handle, memory_arena *Arena);
internal void         OSReleaseFile  (os_handle Handle);

// [OS State]

internal os_system_info *OSGetSystemInfo     (void);
internal vec2_i32        OSGetClientSize     (void);
internal vec2_f32        OSGetMousePosition  (void);
internal vec2_f32        OSGetMouseDelta     (void);
internal b32             OSIsMouseClicked    (OSMouseButton_Type Button);
internal b32             OSIsMouseHeld       (OSMouseButton_Type Button);
internal b32             OSIsMouseReleased   (OSMouseButton_Type Button);
internal void            OSClearInputs       (void);

// [Memory]

internal void *OSReserveMemory  (u64 Size);
internal b32   OSCommitMemory   (void *Memory, u64 Size);
internal void  OSRelease        (void *Memory);

// [Windowing]

internal b32  OSUpdateWindow  (void);
internal void OSSetCursor     (OSCursor_Type Type);


// [Misc]

internal void        OSSleep                    (u32 Milliseconds);
internal void        OSAbort                    (i32 ExitCode);
internal void        OSListenToRegistry         (ui_style_registry *Registry);
internal b32         OSIsValidHandle            (os_handle Handle);
internal byte_string OSAppendToLaunchDirectory  (byte_string Input, memory_arena *Arena);

// [Text]

external b32                  OSAcquireFontObjects  (byte_string Name, f32 Size, gpu_font_objects *GPUObjects, os_font_objects *OSObjects);
external void                 OSReleaseFontObjects  (os_font_objects *Objects);
external os_glyph_layout      OSGetGlyphLayout      (u8 Character, os_font_objects *FontObjects, vec2_f32 TextureSize, f32 Size);
external os_glyph_raster_info OSRasterizeGlyph      (u8 Character, rect_f32 Rect, os_font_objects *OSFontObjects, gpu_font_objects *GPUFontObjects, render_handle RendererHandle);
extern   f32                  OSGetLineHeight       (f32 FontSize, os_font_objects *OSFontObjects);