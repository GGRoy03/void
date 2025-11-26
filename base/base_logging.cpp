// [PRODUCER API]

static void
ConsolePushMessage(console_queue_node *Node, console_queue *Queue)
{
    ConsolePushMessageList(Node, Node, Queue);
}

static void
ConsolePushMessageList(console_queue_node *First, console_queue_node *Last, console_queue *Queue)
{
    atomic_store_explicit(&Last->Next, 0, std::memory_order_relaxed);
    console_queue_node *Prev = atomic_exchange_explicit(&Queue->Head, Last, std::memory_order_acq_rel);
    atomic_store_explicit(&Prev->Next, First, std::memory_order_release);
}

static void
ConsolePushMessageBatch(uint64_t NodeCount, console_queue_node *Nodes[], console_queue *Queue)
{
    if(NodeCount == 0)
    {
        return;
    }

    console_queue_node *First = Nodes[0];
    console_queue_node *Last  = Nodes[NodeCount - 1];

    for(uint64_t Idx = 0; Idx < NodeCount; Idx++)
    {
        console_queue_node *Node = Nodes[Idx];
        atomic_store_explicit(&Node->Next, Nodes[Idx + 1], std::memory_order_relaxed);
    }

    ConsolePushMessageList(First, Last, Queue);
}

// [CONSUMER API]

static void
InitializeConsoleMessageQueue(console_queue *Queue)
{
    atomic_store_explicit(&Queue->Head         , &Queue->Sentinel, std::memory_order_relaxed);
    atomic_store_explicit(&Queue->Tail         , &Queue->Sentinel, std::memory_order_relaxed);
    atomic_store_explicit(&Queue->Sentinel.Next,                0, std::memory_order_relaxed);
}

static void
ConsoleMessageQueuePushFront(console_queue_node *Node, console_queue *Queue)
{
    console_queue_node *Tail = atomic_load_explicit(&Queue->Tail, std::memory_order_relaxed);
    atomic_store_explicit(&Node->Next , Tail, std::memory_order_relaxed);
    atomic_store_explicit(&Queue->Tail, Node, std::memory_order_relaxed);
}

static bool
ConsoleMessageQueueIsEmpty(console_queue *Queue)
{
    console_queue_node *Tail = atomic_load_explicit(&Queue->Tail, std::memory_order_relaxed); // Consumer Only Read/Write
    console_queue_node *Next = atomic_load_explicit(&Tail->Next , std::memory_order_acquire); // Written By Producer / Read By Consumer
    console_queue_node *Head = atomic_load_explicit(&Queue->Head, std::memory_order_acquire); // Written By Producer / Read By Consumer

    bool Result = (Tail == &Queue->Sentinel) && (Next == 0) && (Tail == Head);
    return Result;
}

static ConsoleMessagePoll_Result
PollConsoleMessageQueue(console_queue_node **OutNode, console_queue *Queue)
{
    console_queue_node *Tail = atomic_load_explicit(&Queue->Tail, std::memory_order_relaxed); // Consumer Only Read/Write
    console_queue_node *Next = atomic_load_explicit(&Tail->Next , std::memory_order_acquire); // Written By Producer(Heah=Tail)/ Read By C
    console_queue_node *Head = 0;

    if(Tail == &Queue->Sentinel)
    {
        if(Next == 0)
        {
            // Retry to fetch the head to see if any new content was created by a producer thread
            // while polling. If it was, retry the poll.
            Head = atomic_load_explicit(&Queue->Head, std::memory_order_acquire);
            if(Tail != Head)
            {
                return ConsoleMessagePoll_Retry;
            }
            else
            {
                return ConsoleMessagePoll_Empty;
            }
        }

        // Skip over the sentinel
        atomic_store_explicit(&Queue->Tail, Next, std::memory_order_relaxed); // Consumer Only Read/Write
        Tail = Next;
        Next = atomic_load_explicit(&Tail->Next, std::memory_order_acquire);  // Why acquire?
    }

    // If we have a next node (node after tail), return the tail
    // and set the tail to the next node.
    if(Next != 0)
    {
        atomic_store_explicit(&Queue->Tail, Next, std::memory_order_relaxed);
        *OutNode = Tail;
        return ConsoleMessagePoll_Item;
    }

    // Attempt to reload the head to see if any work was created by a producer.
    // At this point, if no new work is done, we expect the head to equal the tail
    // since there should only be one node (not counting the dummy) in the queue.
    Head = atomic_load_explicit(&Queue->Head, std::memory_order_acquire);
    if(Tail != Head)
    {
        return ConsoleMessagePoll_Retry;
    }

    // Re-Orders the list such that: (T->SH)
    ConsolePushMessage(&Queue->Sentinel, Queue);

    // Reloads next such that we read the most recent work.
    // Most of the time, it will be (T->SH), meaning we are setting
    // the tail to the sentinel after returning the last node, but
    // it could also be new work.
    Next = atomic_load_explicit(&Tail->Next, std::memory_order_acquire);
    if(Next != 0)
    {
        atomic_store_explicit(&Queue->Tail, Next, std::memory_order_relaxed);
        *OutNode = Tail;
        return ConsoleMessagePoll_Item;
    }

    // This should only be the case when ConsoleWriteMessage wasn't fast enough to
    // produce its output.
    return ConsoleMessagePoll_Retry;
}

static console_queue_node *
PopConsoleMessageQueue(console_queue *Queue)
{
    console_queue_node       *Result = 0;
    ConsoleMessagePoll_Result PollResult;

    do
    {
        PollResult = PollConsoleMessageQueue(&Result, Queue);
        if(PollResult == ConsoleMessagePoll_Empty)
        {
            return 0;
        }
    } while(PollResult == ConsoleMessagePoll_Retry);

    return Result;
}

static void
FreeConsoleMessageNode(console_queue_node *Node)
{
    OSRelease(Node);
}

// [NON-ATOMIC PRODUCER API]

static uint64_t
GetTimeStamp(void)
{
    return 0;
}

// WARN: I cannot find a better way to do this allocation.
// This makes it very simple to malloc/free, but is probably
// not the best way. At least the behavior is simple and clear
// producer allocates and consumer frees.

static void
ConsoleWriteMessage(ConsoleMessage_Severity Severity, byte_string Message, console_queue *Queue)
{
    uint64_t MessageSize = Message.Size;
    uint64_t Footprint   = sizeof(console_queue_node) + MessageSize;

    console_queue_node *Node = (console_queue_node *)malloc(Footprint);
    if(Node)
    {
        Node->Value.Severity  = Severity;
        Node->Value.TimeStamp = GetTimeStamp();
        Node->Value.TextSize  = MessageSize;

        memcpy(Node->Value.Text, Message.String, Message.Size);

        ConsolePushMessage(Node, Queue);
    }
}

static void
FreeConsoleNode(console_queue_node *Node)
{
    free(Node);
}
