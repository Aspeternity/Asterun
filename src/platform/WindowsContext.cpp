#include "WindowsContext.hpp"

#include "../core/ExplorerContextSelection.hpp"

#include <ole2.h>
#include <oaidl.h>
#include <ocidl.h>
#include <oleauto.h>
#include <exdisp.h>
#include <servprov.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace altrun::win {
namespace {

using Microsoft::WRL::ComPtr;

struct ExplorerShellCandidate {
    ComPtr<IWebBrowser2> browser;
    HWND browserWindow{};
    HWND viewWindow{};
    std::wstring folderPath;
    ExplorerContextCandidateState state;
};

[[nodiscard]] HWND RootWindow(
    HWND window) {
    if (!window) {
        return nullptr;
    }

    if (HWND root =
            GetAncestor(
                window,
                GA_ROOT)) {
        return root;
    }

    return window;
}

[[nodiscard]] bool SameOrChild(
    HWND parent,
    HWND candidate) {
    return parent != nullptr &&
        candidate != nullptr &&
        (parent == candidate ||
         IsChild(parent, candidate));
}

[[nodiscard]] HWND FocusWindowFor(
    HWND foregroundWindow) {
    if (!foregroundWindow) {
        return nullptr;
    }

    const DWORD threadId =
        GetWindowThreadProcessId(
            foregroundWindow,
            nullptr);

    if (threadId == 0) {
        return nullptr;
    }

    GUITHREADINFO info{};
    info.cbSize = sizeof(info);

    if (!GetGUIThreadInfo(
            threadId,
            &info)) {
        return nullptr;
    }

    return info.hwndFocus;
}

[[nodiscard]] bool
GetBrowserWindow(
    IWebBrowser2* browser,
    HWND* window) {
    if (!browser || !window) {
        return false;
    }

    SHANDLE_PTR rawWindow{};

    if (FAILED(
            browser->get_HWND(
                &rawWindow))) {
        return false;
    }

    const auto handleValue =
        static_cast<std::intptr_t>(
            rawWindow);

    HWND candidate =
        reinterpret_cast<HWND>(
            handleValue);

    if (!candidate ||
        !IsWindow(candidate)) {
        return false;
    }

    *window = candidate;
    return true;
}

[[nodiscard]] bool
GetActiveViewContext(
    IWebBrowser2* browser,
    HWND* viewWindow,
    std::wstring* folderPath) {
    if (!browser ||
        !viewWindow ||
        !folderPath) {
        return false;
    }

    *viewWindow = nullptr;
    folderPath->clear();

    ComPtr<IServiceProvider>
        serviceProvider;

    if (FAILED(
            browser->QueryInterface(
                IID_PPV_ARGS(
                    serviceProvider
                        .GetAddressOf())))) {
        return false;
    }

    ComPtr<IShellBrowser>
        shellBrowser;

    if (FAILED(
            serviceProvider->QueryService(
                SID_STopLevelBrowser,
                IID_PPV_ARGS(
                    shellBrowser
                        .GetAddressOf())))) {
        return false;
    }

    ComPtr<IShellView>
        shellView;

    if (FAILED(
            shellBrowser->
                QueryActiveShellView(
                    shellView
                        .GetAddressOf()))) {
        return false;
    }

    HWND shellViewWindow{};
    if (FAILED(
            shellView->GetWindow(
                &shellViewWindow)) ||
        !shellViewWindow ||
        !IsWindow(
            shellViewWindow)) {
        return false;
    }

    // The Shell view itself is enough to identify a safe Explorer source
    // context. The current location can be virtual (Home, This PC, Quick
    // access, Network...) and therefore may not have a filesystem path.
    *viewWindow =
        shellViewWindow;

    ComPtr<IFolderView>
        folderView;

    if (FAILED(
            shellView.As(
                &folderView))) {
        return true;
    }

    ComPtr<IShellFolder>
        folder;

    if (FAILED(
            folderView->GetFolder(
                IID_PPV_ARGS(
                    folder
                        .GetAddressOf())))) {
        return true;
    }

    ComPtr<IPersistFolder2>
        persistFolder;

    if (FAILED(
            folder.As(
                &persistFolder))) {
        return true;
    }

    PIDLIST_ABSOLUTE folderId{};
    if (FAILED(
            persistFolder->
                GetCurFolder(
                    &folderId)) ||
        !folderId) {
        return true;
    }

    std::wstring path(
        32768,
        L'\0');

    const BOOL converted =
        SHGetPathFromIDListEx(
            folderId,
            path.data(),
            static_cast<DWORD>(
                path.size()),
            GPFIDL_DEFAULT);

    CoTaskMemFree(folderId);

    if (!converted) {
        return true;
    }

    const auto terminator =
        path.find(L'\0');

    if (terminator !=
        std::wstring::npos) {
        path.resize(terminator);
    }

    if (!path.empty()) {
        *folderPath =
            std::move(path);
    }

    return true;
}

[[nodiscard]]
std::vector<
    ExplorerShellCandidate>
EnumerateExplorerCandidates(
    HWND foregroundWindow) {
    std::vector<
        ExplorerShellCandidate>
        candidates;

    if (!foregroundWindow ||
        !IsWindow(
            foregroundWindow)) {
        return candidates;
    }

    const HWND foregroundRoot =
        RootWindow(
            foregroundWindow);

    if (!foregroundRoot) {
        return candidates;
    }

    const HWND focusWindow =
        FocusWindowFor(
            foregroundWindow);

    ComPtr<IShellWindows>
        shellWindows;

    if (FAILED(
            CoCreateInstance(
                CLSID_ShellWindows,
                nullptr,
                CLSCTX_ALL,
                IID_PPV_ARGS(
                    shellWindows
                        .GetAddressOf())))) {
        return candidates;
    }

    long count{};
    if (FAILED(
            shellWindows->get_Count(
                &count)) ||
        count <= 0) {
        return candidates;
    }

    for (long i = 0;
         i < count;
         ++i) {
        VARIANT index{};
        VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;

        ComPtr<IDispatch> dispatch;

        const HRESULT itemResult =
            shellWindows->Item(
                index,
                dispatch.GetAddressOf());

        VariantClear(&index);

        if (FAILED(itemResult) ||
            !dispatch) {
            continue;
        }

        ComPtr<IWebBrowser2>
            browser;

        if (FAILED(
                dispatch.As(
                    &browser))) {
            continue;
        }

        ExplorerShellCandidate
            candidate;

        candidate.browser =
            browser;

        if (!GetBrowserWindow(
                browser.Get(),
                &candidate
                     .browserWindow)) {
            continue;
        }

        if (RootWindow(
                candidate
                    .browserWindow) !=
            foregroundRoot) {
            continue;
        }

        candidate.state
            .hasShellView =
            GetActiveViewContext(
                browser.Get(),
                &candidate
                     .viewWindow,
                &candidate
                     .folderPath);

        if (!candidate.state
                 .hasShellView) {
            continue;
        }

        candidate.state
            .focusMatched =
            SameOrChild(
                candidate
                    .viewWindow,
                focusWindow);

        candidate.state.visible =
            IsWindowVisible(
                candidate
                    .viewWindow) !=
            FALSE;

        // Some Windows 11 Explorer builds can surface the same active shell
        // view through more than one ShellWindows automation object. Treat
        // byte-for-byte identical browser/view/path observations as one
        // candidate so duplicate automation wrappers do not create a false
        // multi-tab ambiguity.
        const bool duplicate =
            std::any_of(
                candidates.begin(),
                candidates.end(),
                [&](const ExplorerShellCandidate&
                        existing) {
                    return existing
                               .browserWindow ==
                            candidate
                                .browserWindow &&
                        existing
                               .viewWindow ==
                            candidate
                                .viewWindow &&
                        existing
                               .folderPath ==
                            candidate
                                .folderPath;
                });

        if (duplicate) {
            continue;
        }

        candidates.push_back(
            std::move(candidate));
    }

    return candidates;
}

[[nodiscard]]
ComPtr<IWebBrowser2>
FindCapturedExplorer(
    const WindowsContextSnapshot&
        context) {
    if (!context.HasExplorer() ||
        !IsWindow(
            context
                .foregroundWindow)) {
        return {};
    }

    auto candidates =
        EnumerateExplorerCandidates(
            context
                .foregroundWindow);

    for (auto& candidate :
         candidates) {
        if (candidate.viewWindow ==
                context
                    .explorerViewWindow &&
            candidate.browserWindow ==
                context
                    .explorerBrowserWindow) {
            return candidate.browser;
        }
    }

    // A view HWND can be recreated by Explorer. A unique browser HWND match
    // is still safe; multiple matches remain intentionally ambiguous.
    ComPtr<IWebBrowser2>
        uniqueBrowser;

    std::size_t browserMatches = 0;

    for (auto& candidate :
         candidates) {
        if (candidate.browserWindow !=
            context
                .explorerBrowserWindow) {
            continue;
        }

        ++browserMatches;
        uniqueBrowser =
            candidate.browser;
    }

    return browserMatches == 1
        ? uniqueBrowser
        : ComPtr<IWebBrowser2>{};
}

} // namespace

WindowsContextSnapshot
CaptureWindowsContext(
    HWND foregroundWindow) {
    WindowsContextSnapshot
        snapshot;

    if (!foregroundWindow ||
        !IsWindow(
            foregroundWindow)) {
        return snapshot;
    }

    auto candidates =
        EnumerateExplorerCandidates(
            foregroundWindow);

    std::vector<
        ExplorerContextCandidateState>
        states;

    states.reserve(
        candidates.size());

    for (const auto& candidate :
         candidates) {
        states.push_back(
            candidate.state);
    }

    const auto selected =
        SelectExplorerContextCandidate(
            states);

    if (!selected ||
        *selected >=
            candidates.size()) {
        return snapshot;
    }

    const auto& candidate =
        candidates[*selected];

    snapshot.kind =
        WindowsContextKind::
            Explorer;
    snapshot.foregroundWindow =
        foregroundWindow;
    snapshot.explorerBrowserWindow =
        candidate.browserWindow;
    snapshot.explorerViewWindow =
        candidate.viewWindow;
    snapshot.explorerFolder =
        candidate.folderPath;

    return snapshot;
}

bool NavigateExplorerToFolder(
    const WindowsContextSnapshot&
        context,
    std::wstring_view folderPath) {
    if (!context.HasExplorer() ||
        folderPath.empty()) {
        return false;
    }

    auto browser =
        FindCapturedExplorer(
            context);

    if (!browser) {
        return false;
    }

    const std::wstring target(
        folderPath);

    VARIANT url{};
    VARIANT empty{};

    VariantInit(&url);
    VariantInit(&empty);

    url.vt = VT_BSTR;
    url.bstrVal =
        SysAllocStringLen(
            target.data(),
            static_cast<UINT>(
                target.size()));

    if (!url.bstrVal) {
        return false;
    }

    const HRESULT navigateResult =
        browser->Navigate2(
            &url,
            &empty,
            &empty,
            &empty,
            &empty);

    VariantClear(&url);

    if (FAILED(
            navigateResult)) {
        return false;
    }

    if (IsWindow(
            context
                .foregroundWindow)) {
        SetForegroundWindow(
            context
                .foregroundWindow);
    }

    return true;
}

} // namespace altrun::win
