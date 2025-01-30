#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#include <windows.h>
#include <tchar.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

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
// Convert a UTF-8 std::string to std::wstring for displaying in a Unicode GUI.
// ---------------------------------------------------------------------------
static std::wstring StringToWString(const std::string& str)
{
    if (str.empty()) {
        return std::wstring();
    }

    // Determine required buffer size (in wchar_t elements), including null terminator
    int lenWide = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (lenWide <= 0) {
        return std::wstring();
    }

    std::vector<wchar_t> buffer(lenWide);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer.data(), lenWide);
    // buffer now holds a null-terminated wide string
    return std::wstring(buffer.data());
}

// ---------------------------------------------------------------------------
// Measure approximate CPU frequency (in MHz) by comparing TSC to QPC.
// ---------------------------------------------------------------------------
static double MeasureCPUFrequencyMHz()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER startCount, endCount;
    QueryPerformanceCounter(&startCount);
    std::uint64_t startTSC = read_tsc();

    // Sleep ~50ms
    Sleep(50);

    std::uint64_t endTSC = read_tsc();
    QueryPerformanceCounter(&endCount);

    double elapsedSec = static_cast<double>(endCount.QuadPart - startCount.QuadPart)
        / static_cast<double>(freq.QuadPart);
    double tscDelta = static_cast<double>(endTSC - startTSC);

    double tscPerSec = tscDelta / elapsedSec;
    double cpuFreqMHz = tscPerSec / 1.0e6;
    return cpuFreqMHz;
}

// ---------------------------------------------------------------------------
// Collect CPU information into a single UTF-8 string, which we then display.
// ---------------------------------------------------------------------------
static std::string GetCPUInformation()
{
    std::ostringstream oss;

    // -----------------------------------------------------------------------
    // 1) CPU Vendor
    // -----------------------------------------------------------------------
    int cpuInfo[4] = { 0 };
    cpuid(cpuInfo, 0);

    char vendorBuf[13];
    std::memset(vendorBuf, 0, sizeof(vendorBuf));

    // Copy EBX, EDX, ECX into vendorBuf
    std::memcpy(&vendorBuf[0], &cpuInfo[1], sizeof(int)); // EBX
    std::memcpy(&vendorBuf[4], &cpuInfo[3], sizeof(int)); // EDX
    std::memcpy(&vendorBuf[8], &cpuInfo[2], sizeof(int)); // ECX

    oss << "CPU Vendor: " << vendorBuf << "\r\n";

    // -----------------------------------------------------------------------
    // 2) Family, Model, Stepping
    // -----------------------------------------------------------------------
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

    oss << "Family: " << family
        << ", Model: " << model
        << ", Stepping: " << stepping
        << ", Type: " << type << "\r\n";

    // -----------------------------------------------------------------------
    // 3) Brand String
    // -----------------------------------------------------------------------
    cpuid(cpuInfo, 0x80000000);
    unsigned maxExt = static_cast<unsigned>(cpuInfo[0]);

    if (maxExt >= 0x80000004) {
        char brandBuf[49];
        std::memset(brandBuf, 0, sizeof(brandBuf));

        int brandData[12];
        cpuid(&brandData[0], 0x80000002);
        cpuid(&brandData[4], 0x80000003);
        cpuid(&brandData[8], 0x80000004);

        std::memcpy(brandBuf, brandData, 48);

        // Trim leading spaces
        char* p = brandBuf;
        while (*p == ' ') {
            p++;
        }
        oss << "CPU Brand: " << p << "\r\n";
    }
    else {
        oss << "CPU Brand: <Not available>\r\n";
    }

    // -----------------------------------------------------------------------
    // 4) Feature Flags
    // -----------------------------------------------------------------------
    cpuid(cpuInfo, 1);
    int stdECX = cpuInfo[2];
    int stdEDX = cpuInfo[3];

    auto PrintFeat = [&](const char* featName, bool isSet)
    {
        oss << "    " << featName << ": " << (isSet ? "Yes" : "No") << "\r\n";
    };

    oss << "\r\nStandard Features (CPUID.1):\r\n";
    PrintFeat("SSE", (stdEDX & (1 << 25)) != 0);
    PrintFeat("SSE2", (stdEDX & (1 << 26)) != 0);
    PrintFeat("SSE3", (stdECX & (1 << 0)) != 0);
    PrintFeat("SSSE3", (stdECX & (1 << 9)) != 0);
    PrintFeat("SSE4.1", (stdECX & (1 << 19)) != 0);
    PrintFeat("SSE4.2", (stdECX & (1 << 20)) != 0);
    PrintFeat("AVX", (stdECX & (1 << 28)) != 0);
    PrintFeat("FMA3", (stdECX & (1 << 12)) != 0);
    PrintFeat("PCLMUL", (stdECX & (1 << 1)) != 0);
    PrintFeat("AES", (stdECX & (1 << 25)) != 0);

    cpuid(cpuInfo, 0x80000001);
    int extECX = cpuInfo[2];
    int extEDX = cpuInfo[3];

    oss << "\r\nExtended Features (CPUID.0x80000001):\r\n";
    PrintFeat("x86-64 (LM)", (extEDX & (1 << 29)) != 0);
    PrintFeat("RDTSCP", (extECX & (1 << 27)) != 0);
    PrintFeat("SSE4a(AMD)", (extECX & (1 << 6)) != 0);
    PrintFeat("MMXExt(AMD)", (extEDX & (1 << 22)) != 0);

    // -----------------------------------------------------------------------
    // 5) Cores / Threads
    // -----------------------------------------------------------------------
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD logicalCount = sysInfo.dwNumberOfProcessors;
    int physicalCores = static_cast<int>(logicalCount);

    // Re-check vendor
    cpuid(cpuInfo, 0);
    char vendBuf[13];
    std::memset(vendBuf, 0, sizeof(vendBuf));
    std::memcpy(&vendBuf[0], &cpuInfo[1], sizeof(int)); // EBX
    std::memcpy(&vendBuf[4], &cpuInfo[3], sizeof(int)); // EDX
    std::memcpy(&vendBuf[8], &cpuInfo[2], sizeof(int)); // ECX
    std::string vendorStr = vendBuf;

    int maxBasic = cpuInfo[0];
    if (vendorStr.find("Intel") != std::string::npos && maxBasic >= 4) {
        cpuidex(cpuInfo, 4, 0);
        int coresPerPkg = ((cpuInfo[0] >> 26) & 0x3F) + 1;
        physicalCores = coresPerPkg;
    }
    else if (vendorStr.find("AuthenticAMD") != std::string::npos && maxExt >= 0x80000008) {
        cpuid(cpuInfo, 0x80000008);
        int coresPerPkg = (cpuInfo[2] & 0xFF) + 1;
        physicalCores = coresPerPkg;
    }

    oss << "\r\nLogical Processors: " << logicalCount << "\r\n"
        << "Approx. Physical Cores: " << physicalCores << "\r\n";

    // -----------------------------------------------------------------------
    // 6) Cache Information
    // -----------------------------------------------------------------------
    oss << "\r\nCache Information:\r\n";
    unsigned leafCache = 0;
    if (maxBasic >= 4) {
        leafCache = 4; // Intel
    }
    else if (maxExt >= 0x8000001D) {
        leafCache = 0x8000001D; // AMD
    }

    if (leafCache == 0) {
        oss << "    No advanced cache enumeration.\r\n";
    }
    else {
        for (int subLeaf = 0; subLeaf < 32; ++subLeaf) {
            cpuidex(cpuInfo, leafCache, subLeaf);
            unsigned cacheType = cpuInfo[0] & 0x1F;
            if (cacheType == 0) {
                break; // no more caches
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

            oss << "    L" << cacheLevel << " " << typeName
                << " Cache: " << totalSize << " KB, "
                << ways << "-way, line size " << lineSize << " bytes\r\n";
        }
    }

    // -----------------------------------------------------------------------
    // 7) Approximate CPU Frequency
    // -----------------------------------------------------------------------
    double freqMHz = MeasureCPUFrequencyMHz();
    oss << "\r\nApprox. CPU Frequency: " << freqMHz << " MHz\r\n";

    return oss.str();
}

// ---------------------------------------------------------------------------
// Globals for our Win32 Window
// ---------------------------------------------------------------------------
static const TCHAR* g_szClassName = TEXT("CPUInfoWindowClass");
static HWND g_hEdit = nullptr;  // handle to multiline Edit control

// ---------------------------------------------------------------------------
// Window Procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // Create a read-only multiline edit control to display info
        RECT rc;
        GetClientRect(hwnd, &rc);
        g_hEdit = CreateWindowEx(
            0,
            TEXT("EDIT"),
            nullptr,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            WS_VSCROLL | ES_READONLY,
            0,
            0,
            rc.right - rc.left,
            rc.bottom - rc.top,
            hwnd,
            reinterpret_cast<HMENU>(1),
            reinterpret_cast<HINSTANCE>(GetModuleHandle(nullptr)),
            nullptr
        );

        // Get CPU info as a UTF-8 string
        std::string infoStr = GetCPUInformation();

        // Convert to wide and display
        std::wstring wInfo = StringToWString(infoStr);
        SetWindowTextW(g_hEdit, wInfo.c_str());
    }
    break;

    case WM_SIZE:
    {
        // Resize the edit control
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        MoveWindow(g_hEdit, 0, 0, width, height, TRUE);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// WinMain: the main entry point for a Win32 GUI application
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Register the window class
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wc))
    {
        MessageBox(nullptr, TEXT("Failed to register window class!"), TEXT("Error"), MB_ICONERROR);
        return 0;
    }

    // Create main window
    HWND hwnd = CreateWindowEx(
        0,
        g_szClassName,
        TEXT("CPU Info Utility (Win32)"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        640, 480,
        nullptr, nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
    {
        MessageBox(nullptr, TEXT("Failed to create main window!"), TEXT("Error"), MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Standard message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}