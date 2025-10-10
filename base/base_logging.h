#pragma once

#include <stdatomic.h>

// [CONSTANTS]

typedef enum ConsoleMessage_Severity
{
    ConsoleMessage_None  = 0,
    ConsoleMessage_Info  = 1,
    ConsoleMessage_Warn  = 2,
    ConsoleMessage_Error = 3,
    ConsoleMessage_Fatal = 4,
} ConsoleMessage_Severity;

typedef enum ConsoleMessagePoll_Result
{
    ConsoleMessagePoll_Empty = 0,
    ConsoleMessagePoll_Item  = 1,
    ConsoleMessagePoll_Retry = 2,
} ConsoleMessagePoll_Result;

typedef enum ConsoleMessage_Constant
{
    ConsoleMessage_CountPerPool  = 64,
    ConsoleMessage_MaximumLength = 512,
} ConsoleMessage_Constant;

#define error_message(Message) ConsoleMessage_Error, byte_string_literal(Message)
#define warn_message(Message)  ConsoleMessage_Warn , byte_string_literal(Message)

// [CORE TYPES]

typedef struct console_message console_message;
struct console_message
{
    ConsoleMessage_Severity Severity;
    u64                     TimeStamp;
    u8                      Text[ConsoleMessage_MaximumLength];
    u64                     TextSize;
};

typedef struct console_queue_node console_queue_node;
struct console_queue_node
{
    _Atomic(console_queue_node *) Next;
    console_message               Value;
};

typedef struct console_queue console_queue;
struct console_queue
{
    _Atomic(console_queue_node *) Head;
    _Atomic(console_queue_node *) Tail;

    console_queue_node Sentinel;
};

// [Globals]

global console_queue Console;

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
external void  ConsoleWriteMessage  (byte_string Message, ConsoleMessage_Severity Severity, console_queue *Queue);
internal void  FreeConsoleNode      (console_queue_node *Node);
