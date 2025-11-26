#define PROFILER 1
#ifndef PROFILER
#define PROFILER 0
#endif

#if PROFILER

static uint64_t
ReadCPUTimer(void)
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #if defined(_MSC_VER)
        return __rdtsc();
    #elif defined(__GNUC__) || defined(__clang__)
        uint32_t low, high;
        __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
        return ((uint64_t)high << 32) | low;
    #else
        #error "Unsupported compiler for x86/x64 RDTSC"
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint64_t cycles;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cycles));
    return cycles;
#elif defined(__arm__) || defined(_M_ARM)
    uint32_t cycles;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycles));
    return cycles;
#elif defined(__riscv)
    uint64_t cycles;
    __asm__ __volatile__("rdcycle %0" : "=r"(cycles));
    return cycles;
#else
    #error "Unsupported platform for CPU timer"
#endif
}

struct profile_anchor
{
    uint64_t TSCElapsedExclusive;
    uint64_t TSCElapsedInclusive;
    uint64_t HitCount;
    uint64_t ProcessedByteCount;
    char const* Label;
};
static profile_anchor GlobalProfilerAnchors[4096];
static uint32_t            GlobalProfilerParent;

struct profile_block
{
    char const* Label;
    uint64_t         OldTSCElapsedInclusive;
    uint64_t         StartTSC;
    uint32_t         ParentIndex;
    uint32_t         AnchorIndex;

    profile_block(char const* Label, uint32_t AnchorIndex, uint64_t ByteCount)
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
        uint64_t Elapsed = ReadCPUTimer() - this->StartTSC;

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

#define TimeBandwidth(Name, ByteCount) profile_block VOID_NAMECONCAT(Block, __LINE__)(Name, __COUNTER__ + 1, ByteCount)

static void
PrintTimeElapsed(uint64_t TotalTSCElapsed, uint64_t TimerFreq, profile_anchor* Anchor)
{
    double Percent = 100.0 * ((double)Anchor->TSCElapsedExclusive / (double)TotalTSCElapsed);
    printf("  %s[%llu]: %llu (%.2f%%", Anchor->Label, Anchor->HitCount, Anchor->TSCElapsedExclusive, Percent);
    if (Anchor->TSCElapsedInclusive != Anchor->TSCElapsedExclusive)
    {
        double PercentWithChildren = 100.0 * ((double)Anchor->TSCElapsedInclusive / (double)TotalTSCElapsed);
        printf(", %.2f%% w/children", PercentWithChildren);
    }
    printf(")");

    if (Anchor->ProcessedByteCount)
    {
        double Megabyte = 1024.0f  * 1024.0f;
        double Gigabyte = Megabyte * 1024.0f;

        double Seconds            = (double)Anchor->TSCElapsedInclusive / (double)TimerFreq;
        double BytesPerSecond     = (double)Anchor->ProcessedByteCount / Seconds;
        double Megabytes          = (double)Anchor->ProcessedByteCount / (double)Megabyte;
        double GigabytesPerSecond = BytesPerSecond / Gigabyte;

        printf("  %.3fmb at %.2fgb/s", Megabytes, GigabytesPerSecond);
    }

    printf("\n");
}

static void
PrintAnchorData(uint64_t TotalCPUElapsed, uint64_t TimerFreq)
{
    for (uint32_t AnchorIndex = 0; AnchorIndex < VOID_ARRAYCOUNT(GlobalProfilerAnchors); ++AnchorIndex)
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

static_assert(__COUNTER__ < VOID_ARRAYCOUNT(GlobalProfilerAnchors), "Number of profile points exceeds size of profiler::Anchors array");

#endif

struct profiler
{
    uint64_t StartTSC;
    uint64_t EndTSC;
};
static profiler GlobalProfiler;

#define TimeBlock(Name) TimeBandwidth(Name, 0)
#define TimeFunction TimeBlock(__func__)

static uint64_t
EstimateBlockTimerFreq(void)
{
    uint64_t MillisecondsToWait = 100;
    uint64_t OSFreq             = OSGetTimerFrequency();

    uint64_t BlockStart = ReadCPUTimer();
    uint64_t OSStart    = OSReadTimer();
    uint64_t OSEnd      = 0;
    uint64_t OSElapsed  = 0;
    uint64_t OSWaitTime = OSFreq * MillisecondsToWait / 1000;
    while (OSElapsed < OSWaitTime)
    {
        OSEnd     = OSReadTimer();
        OSElapsed = OSEnd - OSStart;
    }

    uint64_t BlockEnd = ReadCPUTimer();
    uint64_t BlockElapsed = BlockEnd - BlockStart;

    uint64_t BlockFreq = 0;
    if (OSElapsed)
    {
        BlockFreq = OSFreq * BlockElapsed / OSElapsed;
    }

    return BlockFreq;
}

static void
BeginProfile(void)
{
    GlobalProfiler.StartTSC = ReadCPUTimer();
}

static void
EndAndPrintProfile()
{
    GlobalProfiler.EndTSC = ReadCPUTimer();

    uint64_t TimerFreq       = EstimateBlockTimerFreq();
    uint64_t TotalTSCElapsed = GlobalProfiler.EndTSC - GlobalProfiler.StartTSC;

    if (TimerFreq)
    {
        printf("\nTotal time: %0.4fms (timer freq %llu)\n", 1000.0 * (double)TotalTSCElapsed / (double)TimerFreq, TimerFreq);
    }

    PrintAnchorData(TotalTSCElapsed, TimerFreq);
}
