#pragma once

#include <atomic>

// [CONSTANTS]

enum ConsoleMessage_Severity : uint32_t
{
    ConsoleMessage_None  = 0,
    ConsoleMessage_Info  = 1,
    ConsoleMessage_Warn  = 2,
    ConsoleMessage_Error = 3,
    ConsoleMessage_Fatal = 4,
};

enum ConsoleMessagePoll_Result : uint32_t
{
    ConsoleMessagePoll_Empty = 0,
    ConsoleMessagePoll_Item  = 1,
    ConsoleMessagePoll_Retry = 2,
};

enum ConsoleMessage_Constant : uint32_t
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
    uint64_t                     TimeStamp;
    uint8_t                      Text[ConsoleMessage_MaximumLength];
    uint64_t                     TextSize;
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

static void ConsolePushMessage       (console_queue_node *Node, console_queue *Queue);
static void ConsolePushMessageList   (console_queue_node *First, console_queue_node *Last, console_queue *Queue);
static void ConsolePushMessageBatch  (uint64_t NodeCount, console_queue_node *Nodes[], console_queue *Queue);

// [CONSUMER API]

static void                       InitializeConsoleMessageQueue  (console_queue *Queue);
static void                       ConsoleMessageQueuePushFront   (console_queue_node *Node, console_queue *Queue);
static bool                        ConsoleMessageQueueIsEmpty     (console_queue *Queue);
static ConsoleMessagePoll_Result  PollConsoleMessageQueue        (console_queue_node **OutNode, console_queue *Queue);
static console_queue_node       * PopConsoleMessageQueue         (console_queue *Queue);
static void                       FreeConsoleMessageNode         (console_queue_node *Node);

// [NON-ATOMIC API]

static uint64_t   GetTimeStamp         (void);
static void  ConsoleWriteMessage  (ConsoleMessage_Severity Severity, byte_string Message, console_queue *Queue);
static void  FreeConsoleNode      (console_queue_node *Node);
