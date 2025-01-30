#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// CPUID Wrappers
// ---------------------------------------------------------------------------
static inline void cpuid(int cpuInfo[4], int function_id)
{
    __cpuid(cpuInfo, function_id);
}

static inline void cpuidex(int cpuInfo[4], int function_id, int subfunction_id)
{
    __cpuidex(cpuInfo, function_id, subfunction_id);
}

// ---------------------------------------------------------------------------
// Read the Time Stamp Counter (TSC).
// ---------------------------------------------------------------------------
static inline std::uint64_t read_tsc()
{
    return __rdtsc();
}

// ---------------------------------------------------------------------------
// Measure approximate CPU frequency (MHz) using TSC + QueryPerformanceCounter.
// ---------------------------------------------------------------------------
static double MeasureCPUFrequencyMHz()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER startTime, endTime;
    QueryPerformanceCounter(&startTime);
    std::uint64_t startTSC = read_tsc();

    // Sleep ~50ms
    Sleep(50);

    std::uint64_t endTSC = read_tsc();
    QueryPerformanceCounter(&endTime);

    double elapsedSec = static_cast<double>(endTime.QuadPart - startTime.QuadPart)
        / static_cast<double>(freq.QuadPart);
    double tscDelta = static_cast<double>(endTSC - startTSC);

    double tscPerSec = tscDelta / elapsedSec;
    double cpuFreqMHz = tscPerSec / 1.0e6;
    return cpuFreqMHz;
}

// ---------------------------------------------------------------------------
// Print a feature's presence in a standard format
// ---------------------------------------------------------------------------
static void PrintFeatureFlag(const char* name, bool isSet)
{
    printf("  %-8s: %s\n", name, isSet ? "Yes" : "No");
}

// ---------------------------------------------------------------------------
// Show CPU Vendor, Family/Model/Stepping, and Brand
// ---------------------------------------------------------------------------
static void ShowBasicCPUInfo()
{
    int cpuInfo[4] = { 0 };

    // 1) Vendor string
    cpuid(cpuInfo, 0);

    char vendor[13];
    std::memset(vendor, 0, sizeof(vendor));
    // Copy EBX, EDX, ECX into vendor[]
    std::memcpy(&vendor[0], &cpuInfo[1], sizeof(int));  // EBX
    std::memcpy(&vendor[4], &cpuInfo[3], sizeof(int));  // EDX
    std::memcpy(&vendor[8], &cpuInfo[2], sizeof(int));  // ECX

    printf("CPU Vendor: %s\n", vendor);

    // 2) Family, Model, Stepping (from CPUID.1)
    cpuid(cpuInfo, 1);
    int stepping = (cpuInfo[0] >> 0) & 0x0F;
    int model = (cpuInfo[0] >> 4) & 0x0F;
    int family = (cpuInfo[0] >> 8) & 0x0F;
    int type = (cpuInfo[0] >> 12) & 0x03;

    int extModel = (cpuInfo[0] >> 16) & 0x0F;
    int extFamily = (cpuInfo[0] >> 20) & 0xFF;
    if (family == 0xF) {
        family += extFamily;
    }
    if (family == 0x6 || family == 0xF) {
        model += (extModel << 4);
    }

    printf("Family: %d, Model: %d, Stepping: %d, Type: %d\n",
        family, model, stepping, type);

    // 3) Brand String via CPUID.0x80000002..0x80000004
    cpuid(cpuInfo, 0x80000000);
    unsigned maxExt = static_cast<unsigned>(cpuInfo[0]);

    char brandString[49];
    std::memset(brandString, 0, sizeof(brandString));

    if (maxExt >= 0x80000004) {
        int brandData[12];
        cpuid((int*)&brandData[0], 0x80000002);
        cpuid((int*)&brandData[4], 0x80000003);
        cpuid((int*)&brandData[8], 0x80000004);

        std::memcpy(brandString, brandData, 48);

        // Trim leading spaces if any
        char* p = brandString;
        while (*p == ' ') ++p;
        printf("CPU Brand: %s\n", p);
    }
    else {
        printf("CPU Brand: <Not available>\n");
    }
}

// ---------------------------------------------------------------------------
// Show standard & extended feature flags
// ---------------------------------------------------------------------------
static void ShowFeatureFlags()
{
    int cpuInfo[4] = { 0 };

    // CPUID(1) -> standard feature bits
    cpuid(cpuInfo, 1);
    int stdECX = cpuInfo[2];
    int stdEDX = cpuInfo[3];

    printf("\nStandard Feature Flags (CPUID.1):\n");
    PrintFeatureFlag("SSE", (stdEDX & (1 << 25)) != 0);
    PrintFeatureFlag("SSE2", (stdEDX & (1 << 26)) != 0);
    PrintFeatureFlag("SSE3", (stdECX & (1 << 0)) != 0);
    PrintFeatureFlag("SSSE3", (stdECX & (1 << 9)) != 0);
    PrintFeatureFlag("SSE4.1", (stdECX & (1 << 19)) != 0);
    PrintFeatureFlag("SSE4.2", (stdECX & (1 << 20)) != 0);
    PrintFeatureFlag("AVX", (stdECX & (1 << 28)) != 0);
    PrintFeatureFlag("FMA3", (stdECX & (1 << 12)) != 0);
    PrintFeatureFlag("PCLMUL", (stdECX & (1 << 1)) != 0);
    PrintFeatureFlag("AES", (stdECX & (1 << 25)) != 0);

    // CPUID(0x80000001) -> extended feature bits
    cpuid(cpuInfo, 0x80000001);
    int extECX = cpuInfo[2];
    int extEDX = cpuInfo[3];

    printf("\nExtended Feature Flags (CPUID.0x80000001):\n");
    PrintFeatureFlag("x86-64 (LM)", (extEDX & (1 << 29)) != 0);
    PrintFeatureFlag("RDTSCP", (extECX & (1 << 27)) != 0);
    PrintFeatureFlag("SSE4a(AMD)", (extECX & (1 << 6)) != 0);
    PrintFeatureFlag("MMXExt(AMD)", (extEDX & (1 << 22)) != 0);
}

// ---------------------------------------------------------------------------
// Determine approximate logical vs physical cores
// ---------------------------------------------------------------------------
static void ShowCoreAndThreadCount()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD logicalCount = sysInfo.dwNumberOfProcessors;

    // Basic approach for physical cores using CPUID
    int cpuInfo[4] = { 0 };
    cpuid(cpuInfo, 0);
    int maxBasic = cpuInfo[0];

    cpuid(cpuInfo, 0x80000000);
    unsigned maxExt = static_cast<unsigned>(cpuInfo[0]);

    // Re-check vendor
    char vendor[13];
    std::memset(vendor, 0, sizeof(vendor));
    cpuid(cpuInfo, 0);
    std::memcpy(&vendor[0], &cpuInfo[1], sizeof(int));  // EBX
    std::memcpy(&vendor[4], &cpuInfo[3], sizeof(int));  // EDX
    std::memcpy(&vendor[8], &cpuInfo[2], sizeof(int));  // ECX

    int physicalCores = static_cast<int>(logicalCount);

    // If Intel and CPUID leaf 4 is available
    if (std::strstr(vendor, "Intel") && maxBasic >= 4) {
        cpuidex(cpuInfo, 4, 0);
        int coresPerPkg = ((cpuInfo[0] >> 26) & 0x3F) + 1;
        physicalCores = coresPerPkg;
    }
    // If AMD and CPUID.0x80000008 is available
    else if (std::strstr(vendor, "AuthenticAMD") && maxExt >= 0x80000008) {
        cpuid(cpuInfo, 0x80000008);
        int coresPerPkg = (cpuInfo[2] & 0xFF) + 1;
        physicalCores = coresPerPkg;
    }

    printf("\nLogical Processors: %u\n", logicalCount);
    printf("Approx. Physical Cores: %d\n", physicalCores);
}

// ---------------------------------------------------------------------------
// Enumerate cache details using CPUID leaf 4 (Intel) or 0x8000001D (AMD).
// ---------------------------------------------------------------------------
static void ShowCacheInfo()
{
    int cpuInfo[4] = { 0 };

    cpuid(cpuInfo, 0);
    int maxBasic = cpuInfo[0];

    cpuid(cpuInfo, 0x80000000);
    unsigned maxExt = static_cast<unsigned>(cpuInfo[0]);

    unsigned leafCache = 0;
    if (maxBasic >= 4) {
        leafCache = 4;  // Intel
    }
    else if (maxExt >= 0x8000001D) {
        leafCache = 0x8000001D; // AMD
    }

    if (leafCache == 0) {
        printf("\nCache Information:\n  No advanced cache enumeration.\n");
        return;
    }

    printf("\nCache Information (CPUID leaf 0x%x):\n", leafCache);
    for (int subLeaf = 0; subLeaf < 32; subLeaf++) {
        cpuidex(cpuInfo, leafCache, subLeaf);
        unsigned cacheType = cpuInfo[0] & 0x1F;
        if (cacheType == 0) {
            // no more caches
            break;
        }
        unsigned cacheLevel = (cpuInfo[0] >> 5) & 0x7;
        unsigned ways = ((cpuInfo[1] >> 22) & 0x3FF) + 1;
        unsigned partitions = ((cpuInfo[1] >> 12) & 0x3FF) + 1;
        unsigned lineSize = (cpuInfo[1] & 0xFFF) + 1;
        unsigned sets = cpuInfo[2] + 1;
        unsigned totalSize = (ways * partitions * lineSize * sets) / 1024; // in KB

        const char* typeName = "Unknown";
        if (cacheType == 1) typeName = "Data";
        else if (cacheType == 2) typeName = "Instruction";
        else if (cacheType == 3) typeName = "Unified";

        printf("  L%u %s Cache: %u KB, %u-way, line size %u bytes\n",
            cacheLevel, typeName, totalSize, ways, lineSize);
    }
}

// ---------------------------------------------------------------------------
// main() - Command-line entry point
// ---------------------------------------------------------------------------
int main()
{
    printf("===== CPU Information Utility =====\n\n");

    // 1. Basic CPU info
    ShowBasicCPUInfo();

    // 2. Feature flags
    ShowFeatureFlags();

    // 3. Cores/Threads
    ShowCoreAndThreadCount();

    // 4. Cache info
    ShowCacheInfo();

    // 5. Frequency measurement
    double freqMHz = MeasureCPUFrequencyMHz();
    printf("\nApprox. CPU Frequency: %.2f MHz\n", freqMHz);

    printf("\n===================================\n");
    return 0;
}