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

typedef struct os_handle
{
    u64 u64[1];
} os_handle;

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

// [Core API]

// OS-Agnostic Functions.

internal void ProccessInputMessage(os_button_state *NewState, b32 IsDown);

internal b32  OSIsValidFile        (os_file *File);
internal void OSIgnoreWhiteSpaces  (os_file *File);
internal u8   OSGetFileChar        (os_file *File);
internal u8   OSGetNextFileChar    (os_file *File);

// Per-OS Functions.

internal os_system_info *OSGetSystemInfo  (void);
internal vec2_i32        OSGetClientSize  (void);

internal os_handle OSFindFile  (byte_string Path);
internal os_file   OSReadFile  (os_handle Handle, memory_arena *Arena);

internal void *OSReserveMemory  (u64 Size);
internal b32   OSCommitMemory   (void *Memory, u64 Size);
internal void  OSRelease        (void *Memory);

internal b32   OSUpdateWindow  (void);
internal void  OSSleep         (u32 Milliseconds);

internal void OSWriteToConsole  (byte_string ANSISequence, OSMessage_Severity Severity);

internal void OSAbort  (i32 ExitCode);
