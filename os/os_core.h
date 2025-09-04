#pragma once

// [Constants]

#define OS_KeyboardButtonCount 256
#define OS_MouseButtonCount 5

typedef enum OSWindow_Status
{
    OSWindow_None     = 0,
    OSWindow_Continue = 1,
    OSWindow_Exit     = 2,
    OSWindow_Resize   = 3,
} OSWindow_Status;

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

// [Core API]

// OS-Agnostic Functions.

internal void ProccessInputMessage(os_button_state *NewState, b32 IsDown);

// Per-OS Functions.

internal os_system_info *OSGetSystemInfo  (void);

internal void *OSReserveMemory  (u64 Size);
internal b32   OSCommitMemory   (void *Memory, u64 Size);

internal OSWindow_Status OSUpdateWindow  (void);
internal void            OSSleep         (u32 Milliseconds);

internal void OSAbort (i32 ExitCode);
