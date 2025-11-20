#pragma once

#include <atomic>

// [CONSTANTS]

enum ConsoleMessage_Severity : u32
{
    ConsoleMessage_None  = 0,
    ConsoleMessage_Info  = 1,
    ConsoleMessage_Warn  = 2,
    ConsoleMessage_Error = 3,
    ConsoleMessage_Fatal = 4,
};

enum ConsoleMessagePoll_Result : u32
{
    ConsoleMessagePoll_Empty = 0,
    ConsoleMessagePoll_Item  = 1,
    ConsoleMessagePoll_Retry = 2,
};

enum ConsoleMessage_Constant : u32
{
    ConsoleMessage_MaximumLength = 512,
};

#define info_message(Message)  ConsoleMessage_Info , byte_string_literal(Message), &UIState.Console
#define error_message(Message) ConsoleMessage_Error, byte_string_literal(Message), &UIState.Console
#define warn_message(Message)  ConsoleMessage_Warn , byte_string_literal(Message), &UIState.Console

// [CORE TYPES]

struct console_message
{
    ConsoleMessage_Severity Severity;
    u64                     TimeStamp;
    u8                      Text[ConsoleMessage_MaximumLength];
    u64                     TextSize;
};

struct console_queue_node
{
    std::atomic<console_queue_node *> Next { nullptr };
    console_message                   Value;
};

struct console_queue
{
    std::atomic<console_queue_node *> Head { nullptr };
    std::atomic<console_queue_node *> Tail { nullptr };

    console_queue_node Sentinel;
};

// [PRODUCER API]

internal void ConsolePushMessage       (console_queue_node *Node, console_queue *Queue);
internal void ConsolePushMessageList   (console_queue_node *First, console_queue_node *Last, console_queue *Queue);
internal void ConsolePushMessageBatch  (u64 NodeCount, console_queue_node *Nodes[], console_queue *Queue);

// [CONSUMER API]

internal void                       InitializeConsoleMessageQueue  (console_queue *Queue);
internal void                       ConsoleMessageQueuePushFront   (console_queue_node *Node, console_queue *Queue);
internal b32                        ConsoleMessageQueueIsEmpty     (console_queue *Queue);
internal ConsoleMessagePoll_Result  PollConsoleMessageQueue        (console_queue_node **OutNode, console_queue *Queue);
internal console_queue_node       * PopConsoleMessageQueue         (console_queue *Queue);
internal void                       FreeConsoleMessageNode         (console_queue_node *Node);

// [NON-ATOMIC API]

internal u64   GetTimeStamp         (void);
internal void  ConsoleWriteMessage  (ConsoleMessage_Severity Severity, byte_string Message, console_queue *Queue);
internal void  FreeConsoleNode      (console_queue_node *Node);
