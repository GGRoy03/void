internal void 
GameEntryPoint()
{
    game_state State = {0};

    memory_arena_params Params = {0};
    Params.ReserveSize       = ArenaDefaultReserveSize;
    Params.CommitSize        = ArenaDefaultCommitSize;
    Params.AllocatedFromFile = __FILE__;
    Params.AllocatedFromLine = __LINE__;

    State.Arena = AllocateArena(Params);

    while(1)
    {
        OSWindow_Status WindowStatus = OSUpdateWindow();

        if(WindowStatus == OSWindow_Exit)
        {
            break;
        }
        else if(WindowStatus == OSWindow_Resize)
        {
            // TODO: Resizing of the renderer backend?
        }

        OSSleep(5);
    }
}
