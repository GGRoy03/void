// [PRODUCER API]

internal void
ConsolePushMessage(console_queue_node *Node, console_queue *Queue)
{
    ConsolePushMessageList(Node, Node, Queue);
}

internal void
ConsolePushMessageList(console_queue_node *First, console_queue_node *Last, console_queue *Queue)
{
    atomic_store_explicit(&Last->Next, 0, memory_order_relaxed);
    console_queue_node *Prev = atomic_exchange_explicit(&Queue->Head, Last, memory_order_acq_rel);
    atomic_store_explicit(&Prev->Next, First, memory_order_release);
}

internal void
ConsolePushMessageBatch(u64 NodeCount, console_queue_node *Nodes[], console_queue *Queue)
{
    if(NodeCount == 0)
    {
        return;
    }

    console_queue_node *First = Nodes[0];
    console_queue_node *Last  = Nodes[NodeCount - 1];

    for(u64 Idx = 0; Idx < NodeCount; Idx++)
    {
        console_queue_node *Node = Nodes[Idx];
        atomic_store_explicit(&Node->Next, Nodes[Idx + 1], memory_order_relaxed);
    }

    ConsolePushMessageList(First, Last, Queue);
}

// [CONSUMER API]

internal void
InitializeConsoleMessageQueue(console_queue *Queue)
{
    atomic_store_explicit(&Queue->Head         , &Queue->Sentinel, memory_order_relaxed);
    atomic_store_explicit(&Queue->Tail         , &Queue->Sentinel, memory_order_relaxed);
    atomic_store_explicit(&Queue->Sentinel.Next,                0, memory_order_relaxed);
}

internal void
ConsoleMessageQueuePushFront(console_queue_node *Node, console_queue *Queue)
{
    console_queue_node *Tail = atomic_load_explicit(&Queue->Tail, memory_order_relaxed);
    atomic_store_explicit(&Node->Next , Tail, memory_order_relaxed);
    atomic_store_explicit(&Queue->Tail, Node, memory_order_relaxed);
}

internal b32
ConsoleMessageQueueIsEmpty(console_queue *Queue)
{
    console_queue_node *Tail = atomic_load_explicit(&Queue->Tail, memory_order_relaxed); // Consumer Only Read/Write
    console_queue_node *Next = atomic_load_explicit(&Tail->Next , memory_order_acquire); // Written By Producer / Read By Consumer
    console_queue_node *Head = atomic_load_explicit(&Queue->Head, memory_order_acquire); // Written By Producer / Read By Consumer

    b32 Result = (Tail == &Queue->Sentinel) && (Next == 0) && (Tail == Head);
    return Result;
}

internal ConsoleMessagePoll_Result
PollConsoleMessageQueue(console_queue_node **OutNode, console_queue *Queue)
{
    console_queue_node *Tail = atomic_load_explicit(&Queue->Tail, memory_order_relaxed); // Consumer Only Read/Write
    console_queue_node *Next = atomic_load_explicit(&Tail->Next , memory_order_acquire); // Written By Producer(Heah=Tail)/ Read By C
    console_queue_node *Head = 0;

    if(Tail == &Queue->Sentinel)
    {
        if(Next == 0)
        {
            // Retry to fetch the head to see if any new content was created by a producer thread
            // while polling. If it was, retry the poll.
            Head = atomic_load_explicit(&Queue->Head, memory_order_acquire);
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
        atomic_store_explicit(&Queue->Tail, Next, memory_order_relaxed); // Consumer Only Read/Write
        Tail = Next;
        Next = atomic_load_explicit(&Tail->Next, memory_order_acquire);  // Why acquire?
    }

    // If we have a next node (node after tail), return the tail
    // and set the tail to the next node.
    if(Next != 0)
    {
        atomic_store_explicit(&Queue->Tail, Next, memory_order_relaxed);
        *OutNode = Tail;
        return ConsoleMessagePoll_Item;
    }

    // Attempt to reload the head to see if any work was created by a producer.
    // At this point, if no new work is done, we expect the head to equal the tail
    // since there should only be one node (not counting the dummy) in the queue.
    Head = atomic_load_explicit(&Queue->Head, memory_order_acquire);
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
    Next = atomic_load_explicit(&Tail->Next, memory_order_acquire);
    if(Next != 0)
    {
        atomic_store_explicit(&Queue->Tail, Next, memory_order_relaxed);
        *OutNode = Tail;
        return ConsoleMessagePoll_Item;
    }

    // This should only be the case when ConsoleWriteMessage wasn't fast enough to
    // produce its output.
    return ConsoleMessagePoll_Retry;
}

internal console_queue_node *
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

internal void
FreeConsoleMessageNode(console_queue_node *Node)
{
    OSRelease(Node);
}

// [NON-ATOMIC PRODUCER API]

internal u64
GetTimeStamp(void)
{
    return 0;
}

// NOTE: I cannot find a better way to do this allocation.
// This makes it very simple to malloc/free, but is probably
// not the best way. At least the behavior is simple and clear
// producer allocates and consumer frees. This also uses a malloc/free
// instead of the classic OSReserve/OSCommit/OSRelease which is annoying.

external void
ConsoleWriteMessage(ConsoleMessage_Severity Severity, byte_string Message, console_queue *Queue)
{
    u64 MessageSize = Message.Size;
    u64 Footprint   = sizeof(console_queue_node) + MessageSize;

    console_queue_node *Node = malloc(Footprint);
    if(Node)
    {
        Node->Value.Severity  = Severity;
        Node->Value.TimeStamp = GetTimeStamp();
        Node->Value.TextSize  = MessageSize;

        memcpy(Node->Value.Text, Message.String, Message.Size);

        ConsolePushMessage(Node, Queue);
    }
}

internal void
FreeConsoleNode(console_queue_node *Node)
{
    free(Node);
}
