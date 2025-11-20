#pragma once

// [Constants]

#define OS_KeyboardButtonCount 256
#define OS_MouseButtonCount 5
#define OS_MAX_PATH 256

#define MaxUTF8BufferSizePerFrame 2048

typedef enum OSConstant_Type
{

    OSConstant_MouseButtonCount       = 5,
    OSConstant_KeyboardButtonCount    = 256,
    OSConstant_MaxPath                = 256,
} OSConstant_Type;

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

typedef struct ui_font           ui_font;
typedef struct ui_style_registry ui_style_registry;

// [Core Types]

typedef struct os_system_info
{
    u32 PageSize;
    u32 ProcessorCount;
} os_system_info;

typedef struct os_button_state
{
    b32 EndedDown;
    u32 HalfTransitionCount;
} os_button_state;

typedef struct os_button_action
{
    b32 IsPressed;
    u32 Keycode;
} os_button_action;

typedef struct os_button_playback
{
    os_button_action Actions[256];
    u32              Count;
    u32              Size;
} os_button_playback;

typedef struct os_utf8_playback
{
    u8  UTF8[1024];
    u32 Count;
    u32 Size;
} os_utf8_playback;

typedef struct os_inputs
{
    b32             IsActiveFrame;
    os_button_state KeyboardButtons[OS_KeyboardButtonCount];
    vec2_f32        MousePosition;
    vec2_f32        MouseDelta;
    os_button_state MouseButtons[OS_MouseButtonCount];

    // Keyboard
    os_button_playback ButtonBuffer;
    os_utf8_playback   UTF8Buffer;

    // Mouse
    f32 ScrollDeltaInLines;
    i32 WheelScrollLine;
} os_inputs;

typedef struct os_read_file
{
    byte_string Content;
    u64         At;
    b32         FullyRead;
} os_read_file;

typedef struct os_glyph_info
{
    vec2_i32 Size;
    vec2_f32 Offset;
    f32      AdvanceX;
} os_glyph_info;

// [Inputs]

internal void      ProcessInputMessage  (os_button_state *NewState, b32 IsDown);
internal vec2_f32  OSGetMousePosition   (void);
internal vec2_f32  OSGetMouseDelta      (void);
internal f32       OSGetScrollDelta     (void);
internal b32       OSIsMouseClicked     (OSMouseButton_Type Button);
internal b32       OSIsMouseHeld        (OSMouseButton_Type Button);
internal b32       OSIsMouseReleased    (OSMouseButton_Type Button);
internal void      OSClearInputs        (os_inputs *Inputs);

internal os_button_playback * OSGetButtonPlayback  (void);
internal os_utf8_playback   * OSGetTextPlayback    (void);

// [Files]

internal b32   IsValidFile      (os_read_file *File);
internal void  SkipWhiteSpaces  (os_read_file *File);
internal u8  * PeekFilePointer  (os_read_file *File);
internal u8    PeekFile         (os_read_file *File, i32 Offset);
internal void  AdvanceFile      (os_read_file *File, u32 Count);

internal os_handle    OSFindFile     (byte_string Path);
internal u64          OSFileSize     (os_handle Handle);
internal os_read_file OSReadFile     (os_handle Handle, memory_arena *Arena);
internal void         OSReleaseFile  (os_handle Handle);

// [OS State]

internal os_system_info * OSGetSystemInfo  (void);
internal os_inputs      * OSGetInputs      (void);

// [Memory]

internal void *OSReserveMemory  (u64 Size);
internal b32   OSCommitMemory   (void *Memory, u64 Size);
internal void  OSRelease        (void *Memory);

// [Misc]

internal void  OSAbort              (i32 ExitCode);
internal b32   OSIsValidHandle      (os_handle Handle);
internal void  OSSetCursor          (OSCursor_Type Type);

internal u64 OSReadTimer          (void);
internal u64 OSGetTimerFrequency  (void);

// Inputs:
//   You may query input state using OSInputKey_Type.

typedef enum OSInputKey_Type
{
    OSInputKey_None,

    OSInputKey_A,
    OSInputKey_B,
    OSInputKey_C,
    OSInputKey_D,
    OSInputKey_E,
    OSInputKey_F,
    OSInputKey_G,
    OSInputKey_H,
    OSInputKey_I,
    OSInputKey_J,
    OSInputKey_K,
    OSInputKey_L,
    OSInputKey_M,
    OSInputKey_N,
    OSInputKey_O,
    OSInputKey_P,
    OSInputKey_Q,
    OSInputKey_R,
    OSInputKey_S,
    OSInputKey_T,
    OSInputKey_U,
    OSInputKey_V,
    OSInputKey_W,
    OSInputKey_X,
    OSInputKey_Y,
    OSInputKey_Z,

    OSInputKey_0,
    OSInputKey_1,
    OSInputKey_2,
    OSInputKey_3,
    OSInputKey_4,
    OSInputKey_5,
    OSInputKey_6,
    OSInputKey_7,
    OSInputKey_8,
    OSInputKey_9,

    OSInputKey_Enter,
    OSInputKey_Space,
    OSInputKey_Minus,
    OSInputKey_Equals,
    OSInputKey_LeftBracket,
    OSInputKey_RightBracket,
    OSInputKey_Backslash,
    OSInputKey_Semicolon,
    OSInputKey_Apostrophe,
    OSInputKey_Comma,
    OSInputKey_Period,
    OSInputKey_Slash,
    OSInputKey_Grave,

    OSInputKey_Count
} OSInputKey_Type;

internal b32 IsKeyClicked  (OSInputKey_Type Key);

// OSAcquireFontContext:
//   Acquires the font context for a given font.
//   Win32: GPUContext must have a valid IDXGISurface * created with DXGI_FORMAT_B8G8R8A8_UNORM (Format) & D3D11_BIND_RENDER_TARGET;
//   Will always query the system's font collection to find the specified font.
//   If failure occurs, every field will be cleared to 0s and the context will be invalid.
// OSReleaseFontContext:
//   Fully releases the font context and makes it unusable.

typedef struct os_font_context os_font_context;
typedef struct gpu_font_context gpu_font_context;

b32  OSAcquireFontContext  (byte_string FontName, f32 FontSize, gpu_font_context *GPUContext, os_font_context *OSContext);
void OSReleaseFontContext  (os_font_context *Context);

// OSGetGlyphInfo:
//   Make sure that the glyph run is valid UTF-8 string as well as the OSContext is valid.
//   Will return {0} if the glyph run size is bigger than (126, 126) or any of the arguments is invalid.
//
// OSRasterizeGlyph:
//   Rasterizes a glyph into the transfer texture on success.
//   Returns 1 on success and 0 on failure.
//   You may generate the rects however you want, but they must not overlap.
//   Typical flow: create transfer resource -> rasterize glyphs into it -> call TransferGlyph to copy into cache
//
// OSGetLineHeight:
//   Queries the LineHeight from a fully intialized context. If result == 0, then the context or the font size is invalid.

os_glyph_info  OSGetGlyphInfo    (byte_string UTF8, f32 FontSize, os_font_context *OSContext);
b32            OSRasterizeGlyph  (byte_string UTF8, rect_f32 Rect, os_font_context *OSContext);
f32            OSGetLineHeight   (f32 FontSize, os_font_context *FontContext);
