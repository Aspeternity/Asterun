#include "app/App.hpp"
#include "ui/LauncherWindow.hpp"
#include <windows.h>
#include <psapi.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
unsigned loads{}, failLoad{};
bool failInfo{}, failDc{};
HANDLE WINAPI TestLoadImage(HINSTANCE instance, LPCWSTR name, UINT type, int x, int y, UINT flags) {
    if (type == IMAGE_BITMAP && ++loads == failLoad) return nullptr;
    return LoadImageW(instance, name, type, x, y, flags);
}
int WINAPI TestGetObject(HANDLE object, int bytes, LPVOID info) {
    return failInfo ? 0 : GetObjectW(object, bytes, info);
}
HDC WINAPI TestCreateDc(HDC dc) { return failDc ? nullptr : CreateCompatibleDC(dc); }
}
#define LoadImageW TestLoadImage
#define GetObjectW TestGetObject
#define CreateCompatibleDC TestCreateDc
#include "../src/ui/LauncherWindow.cpp"
#undef CreateCompatibleDC
#undef GetObjectW
#undef LoadImageW

namespace {
struct Resources {
    DWORD gdi, user, handles;
    SIZE_T privateBytes, workingSet;
};
Resources Sample(const char* label) {
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory);
    assert(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)));
    Resources r{GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS)};
    assert(GetProcessHandleCount(GetCurrentProcess(), &r.handles));
    r.privateBytes = memory.PrivateUsage; r.workingSet = memory.WorkingSetSize;
    std::cout << label << ",PrivateBytes=" << r.privateBytes << ",WorkingSet=" << r.workingSet
        << ",GDI=" << r.gdi << ",USER=" << r.user << ",Handles=" << r.handles << '\n';
    return r;
}
void DrainQuit() { MSG msg{}; while (PeekMessageW(&msg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {} }
}
namespace altrun {
struct LauncherResourceRuntimeFixture {
    static void Empty(const LauncherWindow& w) {
        assert(!w.classicBitmapDc_ && !w.classicBackgroundBitmap_);
        for (auto h : w.classicShortcutBitmaps_) assert(!h);
        for (auto h : w.classicCloseBitmaps_) assert(!h);
        assert(w.classicBackgroundSize_.cx == 0 && w.classicBackgroundSize_.cy == 0);
    }
    static void Destroy(LauncherWindow& w) { if (w.hwnd_) DestroyWindow(w.hwnd_); DrainQuit(); }
    static void Dpi(LauncherWindow& w, UINT dpi) {
        RECT rect{}; GetWindowRect(w.hwnd_, &rect);
        SendMessageW(w.hwnd_, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&rect));
        assert(w.dpi_ == dpi);
    }
    static std::vector<unsigned char> Pixels(LauncherWindow& w) {
        constexpr int width = 900, height = 300;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* bits{}; HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC dc = CreateCompatibleDC(nullptr); assert(bitmap && dc);
        auto old = SelectObject(dc, bitmap);
        std::memset(bits, 0, width * height * 4);
        RECT client{0, 0, width, height};
        w.PaintClassicBackground(dc, client);
        w.PaintClassicLogo(dc, 20, 20);
        RECT close{width - 70, 20, width - 20, 70};
        w.PaintClassicClose(dc, close);
        GdiFlush();
        auto* begin = static_cast<unsigned char*>(bits);
        std::vector<unsigned char> result(begin, begin + width * height * 4);
        SelectObject(dc, old); DeleteDC(dc); DeleteObject(bitmap);
        return result;
    }
    static void Run(HINSTANCE instance) {
        App app(instance);
        auto& settings = const_cast<Settings&>(app.settingsStore_.Data());
        settings.showTrayIcon = false; settings.soundEnabled = false;
        settings.autoCheckUpdates = false;
        const auto resourceCount = static_cast<unsigned>(kClassicShortcutResourceIds.size() + kClassicCloseResourceIds.size() + 1);
        // Every partial load must roll back. Also permit retry after DC/info failure.
        for (unsigned failure = 1; failure <= resourceCount + 2; ++failure) {
            LauncherWindow w(app, instance);
            const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            loads = 0; failLoad = failure <= resourceCount ? failure : 0;
            failInfo = failure == resourceCount + 1; failDc = failure == resourceCount + 2;
            assert(!w.EnsureClassicResources()); Empty(w);
            assert(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == before);
            failLoad = 0; failInfo = failDc = false;
            assert(w.EnsureClassicResources());
            const auto loaded = loads; assert(w.EnsureClassicResources()); assert(loads == loaded);
            w.ReleaseClassicResources(); Empty(w);
            assert(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == before);
        }
        // Compare lazy Classic pixels against Classic-first initialization at all asset DPIs.
        std::vector<std::vector<unsigned char>> reference;
        settings.uiStyle = UiStyle::Classic;
        {
            LauncherWindow eager(app, instance); assert(eager.Create());
            for (UINT dpi : {96U, 120U, 144U, 192U, 240U}) { Dpi(eager, dpi); reference.push_back(Pixels(eager)); }
            Destroy(eager);
        }
        Resources warm{};
        for (int cycle = 0; cycle < 21; ++cycle) {
            settings.uiStyle = UiStyle::ModernCompact; loads = 0;
            {
                LauncherWindow w(app, instance); assert(w.Create());
                Empty(w); assert(loads == 0);
                settings.uiStyle = UiStyle::Classic; w.ApplyAppearance();
                assert(loads == resourceCount && w.classicBitmapDc_);
                auto dc = w.classicBitmapDc_; auto bitmap = w.classicBackgroundBitmap_;
                settings.uiStyle = UiStyle::ModernCompact; w.ApplyAppearance();
                settings.uiStyle = UiStyle::Classic; w.ApplyAppearance();
                std::size_t i = 0;
                for (UINT dpi : {96U, 120U, 144U, 192U, 240U}) { Dpi(w, dpi); assert(Pixels(w) == reference[i++]); }
                assert(loads == resourceCount && dc == w.classicBitmapDc_ && bitmap == w.classicBackgroundBitmap_);
                Destroy(w);
            }
            if (cycle == 0) warm = Sample("warm");
            if (cycle == 10 || cycle == 20) {
                auto current = Sample(cycle == 10 ? "cycle10" : "cycle20");
                // Compare repeated-operation counts, not machine-specific memory ceilings.
                assert(current.gdi <= warm.gdi + 2 && current.user <= warm.user + 2);
                assert(current.handles <= warm.handles + 4);
            }
        }
    }
};
}
int main() {
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    altrun::LauncherResourceRuntimeFixture::Run(GetModuleHandleW(nullptr));
    if (SUCCEEDED(com)) CoUninitialize();
    std::cout << "Classic lazy resources: partial failures, reuse, DPI pixels and lifecycle passed\n";
}
