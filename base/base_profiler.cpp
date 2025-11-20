#define PROFILER 1
#ifndef PROFILER
#define PROFILER 0
#endif

#if PROFILER

internal u64
ReadCPUTimer(void)
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #if defined(_MSC_VER)
        return __rdtsc();
    #elif defined(__GNUC__) || defined(__clang__)
        u32 low, high;
        __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
        return ((u64)high << 32) | low;
    #else
        #error "Unsupported compiler for x86/x64 RDTSC"
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    u64 cycles;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cycles));
    return cycles;
#elif defined(__arm__) || defined(_M_ARM)
    u32 cycles;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycles));
    return cycles;
#elif defined(__riscv)
    u64 cycles;
    __asm__ __volatile__("rdcycle %0" : "=r"(cycles));
    return cycles;
#else
    #error "Unsupported platform for CPU timer"
#endif
}

struct profile_anchor
{
    u64 TSCElapsedExclusive;
    u64 TSCElapsedInclusive;
    u64 HitCount;
    u64 ProcessedByteCount;
    char const* Label;
};
internal profile_anchor GlobalProfilerAnchors[4096];
internal u32            GlobalProfilerParent;

struct profile_block
{
    char const* Label;
    u64         OldTSCElapsedInclusive;
    u64         StartTSC;
    u32         ParentIndex;
    u32         AnchorIndex;

    profile_block(char const* Label, u32 AnchorIndex, u64 ByteCount)
    {
        profile_anchor* Anchor = GlobalProfilerAnchors + AnchorIndex;
        Anchor->ProcessedByteCount += ByteCount;

        this->ParentIndex            = GlobalProfilerParent;
        this->AnchorIndex            = AnchorIndex;
        this->Label                  = Label;
        this->OldTSCElapsedInclusive = Anchor->TSCElapsedInclusive;
        this->StartTSC               = ReadCPUTimer();

        GlobalProfilerParent = AnchorIndex;
    }

    ~profile_block(void)
    {
        u64 Elapsed = ReadCPUTimer() - this->StartTSC;

        profile_anchor* Parent = GlobalProfilerAnchors + ParentIndex;
        Parent->TSCElapsedExclusive -= Elapsed;

        profile_anchor* Anchor = GlobalProfilerAnchors + this->AnchorIndex;
        Anchor->TSCElapsedExclusive += Elapsed;
        Anchor->TSCElapsedInclusive  = this->OldTSCElapsedInclusive + Elapsed;
        Anchor->HitCount            += 1;
        Anchor->Label                = this->Label;

        GlobalProfilerParent = ParentIndex;
    }
};

#define TimeBandwidth(Name, ByteCount) profile_block Concat(Block, __LINE__)(Name, __COUNTER__ + 1, ByteCount)

internal void
PrintTimeElapsed(u64 TotalTSCElapsed, u64 TimerFreq, profile_anchor* Anchor)
{
    f64 Percent = 100.0 * ((f64)Anchor->TSCElapsedExclusive / (f64)TotalTSCElapsed);
    printf("  %s[%llu]: %llu (%.2f%%", Anchor->Label, Anchor->HitCount, Anchor->TSCElapsedExclusive, Percent);
    if (Anchor->TSCElapsedInclusive != Anchor->TSCElapsedExclusive)
    {
        f64 PercentWithChildren = 100.0 * ((f64)Anchor->TSCElapsedInclusive / (f64)TotalTSCElapsed);
        printf(", %.2f%% w/children", PercentWithChildren);
    }
    printf(")");

    if (Anchor->ProcessedByteCount)
    {
        f64 Megabyte = 1024.0f  * 1024.0f;
        f64 Gigabyte = Megabyte * 1024.0f;

        f64 Seconds            = (f64)Anchor->TSCElapsedInclusive / (f64)TimerFreq;
        f64 BytesPerSecond     = (f64)Anchor->ProcessedByteCount / Seconds;
        f64 Megabytes          = (f64)Anchor->ProcessedByteCount / (f64)Megabyte;
        f64 GigabytesPerSecond = BytesPerSecond / Gigabyte;

        printf("  %.3fmb at %.2fgb/s", Megabytes, GigabytesPerSecond);
    }

    printf("\n");
}

internal void
PrintAnchorData(u64 TotalCPUElapsed, u64 TimerFreq)
{
    for (u32 AnchorIndex = 0; AnchorIndex < ArrayCount(GlobalProfilerAnchors); ++AnchorIndex)
    {
        profile_anchor* Anchor = GlobalProfilerAnchors + AnchorIndex;
        if (Anchor->TSCElapsedInclusive)
        {
            PrintTimeElapsed(TotalCPUElapsed, TimerFreq, Anchor);
        }
    }
}

#else

#define TimeBandwidth(...)
#define PrintAnchorData(...)

internal_assert(__COUNTER__ < ArrayCount(GlobalProfilerAnchors), "Number of profile points exceeds size of profiler::Anchors array");

#endif

struct profiler
{
    u64 StartTSC;
    u64 EndTSC;
};
internal profiler GlobalProfiler;

#define TimeBlock(Name) TimeBandwidth(Name, 0)
#define TimeFunction TimeBlock(__func__)

internal u64
EstimateBlockTimerFreq(void)
{
    u64 MillisecondsToWait = 100;
    u64 OSFreq             = OSGetTimerFrequency();

    u64 BlockStart = ReadCPUTimer();
    u64 OSStart    = OSReadTimer();
    u64 OSEnd      = 0;
    u64 OSElapsed  = 0;
    u64 OSWaitTime = OSFreq * MillisecondsToWait / 1000;
    while (OSElapsed < OSWaitTime)
    {
        OSEnd     = OSReadTimer();
        OSElapsed = OSEnd - OSStart;
    }

    u64 BlockEnd = ReadCPUTimer();
    u64 BlockElapsed = BlockEnd - BlockStart;

    u64 BlockFreq = 0;
    if (OSElapsed)
    {
        BlockFreq = OSFreq * BlockElapsed / OSElapsed;
    }

    return BlockFreq;
}

internal void
BeginProfile(void)
{
    GlobalProfiler.StartTSC = ReadCPUTimer();
}

internal void
EndAndPrintProfile()
{
    GlobalProfiler.EndTSC = ReadCPUTimer();

    u64 TimerFreq       = EstimateBlockTimerFreq();
    u64 TotalTSCElapsed = GlobalProfiler.EndTSC - GlobalProfiler.StartTSC;

    if (TimerFreq)
    {
        printf("\nTotal time: %0.4fms (timer freq %llu)\n", 1000.0 * (f64)TotalTSCElapsed / (f64)TimerFreq, TimerFreq);
    }

    PrintAnchorData(TotalTSCElapsed, TimerFreq);
}
