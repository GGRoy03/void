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

typedef struct os_handle
{
    u64 u64[1];
} os_handle;

// [Core API]

// OS-Agnostic Functions.

internal void ProccessInputMessage(os_button_state *NewState, b32 IsDown);

// Per-OS Functions.

internal os_system_info *OSGetSystemInfo  (void);
internal vec2_i32        OSGetClientSize  (os_handle Window);

internal void *OSReserveMemory  (u64 Size);
internal b32   OSCommitMemory   (void *Memory, u64 Size);
internal void  OSRelease        (void *Memory);

internal b32   OSUpdateWindow  (void);
internal void  OSSleep         (u32 Milliseconds);

internal void OSAbort  (i32 ExitCode);
