internal void
ConsolePrintMessage(byte_string Message, ConsoleMessage_Severity Severity, memory_arena *Arena, ui_node BufferNode)
{
    editor_console_ui *ConsoleUI = &EditorUI.ConsoleUI;

    ui_color    TextColor = UIColor(1.f, 1.f, 1.f, 1.f);
    byte_string Prefix    = ByteString(0, 0);

    switch(Severity)
    {

    case ConsoleMessage_Info:
    {
        TextColor = UIColor(0.01f, 0.71f, 0.99f, 1.f);
        Prefix    = byte_string_literal("[Info] => ");
    } break;

    case ConsoleMessage_Warn:
    {
        TextColor = UIColor(0.99f, 0.62f, 0.01f, 1.f);
        Prefix    = byte_string_literal("[Warn] => ");
    } break;

    case ConsoleMessage_Error:
    {
        TextColor = UIColor(0.99f, 0.01f, 0.11f, 1.f);
        Prefix    = byte_string_literal("[Error] => ");
    } break;

    case ConsoleMessage_Fatal:
    {
        TextColor = UIColor(0.99f, 0.01f, 0.11f, 1.f);
        Prefix    = byte_string_literal("[Fatal] => ");
    } break;

    default: break;

    }

    byte_string FormattedMessage = ByteStringAppend(Message, Prefix, 0, Arena);

    // NOTE: Love this, but it also means I need a LRU to manage resources.
    // Which is somehwat easy to do. Then I don't even have to worry about
    // anything and I just override the text.

    BufferNode.FindChild(0, ConsoleUI->Pipeline)
        .SetTextColor(TextColor, ConsoleUI->Pipeline);
//      .SetText(FormattedMessage, ConsoleUI->Pipeline)
}

internal void
ConsoleUI(editor_console_ui *ConsoleUI)
{
    // TODO: Clear the Console's arena every frame

    if(!ConsoleUI->IsInitialized)
    {
        memory_arena_params ArenaParams = {0};
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Kilobyte(64);
            ArenaParams.CommitSize        = Kilobyte(4);
        }

        ConsoleUI->Pipeline      = UICreatePipeline((ui_pipeline_params){.ThemeFile = byte_string_literal("styles/console.cim")});
        ConsoleUI->MessageLimit  = ConsoleConstant_MessageCountLimit;
        ConsoleUI->Arena         = AllocateArena(ArenaParams);
        ConsoleUI->IsInitialized = 1;

        // Default Layout
        {
            ui_subtree_params SubtreeParams = {.CreateNew = 1};

            UISubtree(SubtreeParams, ConsoleUI->Pipeline)
            {
                UIWindow(ConsoleStyle_Window, ConsoleUI->Pipeline)
                {
                    UIScrollView(ConsoleStyle_MessageView, ConsoleUI->Pipeline)
                    {
                        UIGetLast(ConsoleUI->Pipeline)
                            .SetId(ui_id("Console_Buffer"), ConsoleUI->Pipeline);
                    //      .ReserveChildren(60, ConsoleUI->Pipeline)
                    }
                }
            }
        }

        // Scroll Test
        {
            ConsoleWriteMessage(info_message("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
            ConsoleWriteMessage(info_message("abcdefghijklmnopqrstuvwxyz"));
            ConsoleWriteMessage(info_message("I am trying to overflow 1"));
            ConsoleWriteMessage(info_message("I am trying to overflow 2"));
            ConsoleWriteMessage(info_message("I am trying to overflow 3"));
            ConsoleWriteMessage(info_message("I am trying to overflow 4"));
            ConsoleWriteMessage(info_message("I am trying to overflow 5"));
            ConsoleWriteMessage(info_message("I am trying to overflow 6"));
            ConsoleWriteMessage(info_message("I am trying to overflow 7"));
            ConsoleWriteMessage(info_message("I am trying to overflow 8"));
            ConsoleWriteMessage(info_message("I am trying to overflow 9"));
            ConsoleWriteMessage(info_message("I am trying to overflow 10"));
            ConsoleWriteMessage(info_message("I am trying to overflow 11"));
            ConsoleWriteMessage(info_message("I am trying to overflow 12"));
        }

    }

    UIBeginPipeline(ConsoleUI->Pipeline);

    // Drain The Queue
    {
        ui_node BufferNode = UIFindNodeById(ui_id("Console_Buffer"), ConsoleUI->Pipeline->IdTable);
        if (BufferNode.CanUse)
        {
            console_queue_node *Node = 0;
            while ((Node = PopConsoleMessageQueue(&UIState.Console)))
            {
                ConsolePrintMessage(ByteString(Node->Value.Text, Node->Value.TextSize), Node->Value.Severity, ConsoleUI->Arena, BufferNode);
                FreeConsoleNode(Node);
            }
        }
    }

    UIExecuteAllSubtrees(ConsoleUI->Pipeline);
}
