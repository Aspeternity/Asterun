#include "ui/UiListView.hpp"
#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>

// Observe the state allocation passed to the real window property. Replacement
// allocation functions are confined to this test executable.
namespace {
void* observedState{};
unsigned stateDeletes{};
bool failProperty{};
UINT_PTR failSubclass{};
}
void* operator new(std::size_t bytes) {
    if (void* value = std::malloc(bytes ? bytes : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept {
    if (value && value == observedState) {
        ++stateDeletes;
        observedState = nullptr;
    }
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept { ::operator delete(value); }

namespace {
BOOL WINAPI TestSetProp(HWND hwnd, LPCWSTR name, HANDLE value) {
    observedState = value;
    if (failProperty) return FALSE;
    return SetPropW(hwnd, name, value);
}
BOOL WINAPI TestSetSubclass(HWND hwnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR data) {
    if (id == failSubclass) return FALSE;
    return SetWindowSubclass(hwnd, proc, id, data);
}
}
#define SetPropW TestSetProp
#define SetWindowSubclass TestSetSubclass
#include "../src/ui/UiListView.cpp"
#undef SetWindowSubclass
#undef SetPropW

int main() {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    assert(InitCommonControlsEx(&controls));
    const auto instance = GetModuleHandleW(nullptr);
    const HWND owner = CreateWindowExW(0, L"STATIC", L"fixture", WS_POPUP,
        0, 0, 300, 200, nullptr, nullptr, instance, nullptr);
    assert(owner);
    for (int failure = 0; failure < 4; ++failure) {
        failProperty = failure == 0;
        failSubclass = failure == 1 ? 0x1A57 : failure == 2 ? 0x1A58 : 0;
        stateDeletes = 0;
        const HWND list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT,
            0, 0, 300, 200, owner, nullptr, instance, nullptr);
        assert(list);
        // As in the product, populate columns before styling the report view.
        // Common controls can defer creating the Header until the first column.
        LVCOLUMNW column{};
        column.mask = LVCF_WIDTH;
        column.cx = 150;
        assert(ListView_InsertColumn(list, 0, &column) == 0);
        const HWND header = ListView_GetHeader(list);
        assert(header);
        const LONG_PTR headerStyle = GetWindowLongPtrW(header, GWL_STYLE);
        altrun::ui::InitializeNextListView(list, 96, nullptr, nullptr);
        if (failure < 2) {
            assert(!GetPropW(list, L"Asterun.UiListView.State"));
            assert(stateDeletes == 1 && !observedState);
        } else {
            assert(GetPropW(list, L"Asterun.UiListView.State"));
            assert(stateDeletes == 0 && observedState);
            if (failure == 2) {
                assert(GetWindowLongPtrW(header, GWL_STYLE) == headerStyle);
                // A later successful initialization must recover gracefully.
                failSubclass = 0;
                altrun::ui::InitializeNextListView(list, 144, nullptr, nullptr);
            }
        }
        assert(DestroyWindow(list));
        assert(stateDeletes == 1 && !observedState);
    }
    DestroyWindow(owner);
}
