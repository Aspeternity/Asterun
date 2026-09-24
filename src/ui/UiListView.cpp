#include "UiListView.hpp"

#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace altrun::ui {
namespace {

constexpr wchar_t
    kNextListStateProperty[] =
        L"ALTRunNext.UiListView.State";

constexpr UINT_PTR
    kNextListSubclassId =
        0x1A57;

constexpr UINT_PTR
    kNextHeaderSubclassId =
        0x1A58;

constexpr COLORREF
    kTableHairline =
        RGB(237, 240, 244);

constexpr COLORREF
    kResizePreviewColor =
        RGB(155, 190, 222);

struct NextColumnResizeDrag {
    bool active{false};
    bool ending{false};
    int column{-1};
    int startMouseX{0};
    int startWidth{0};
    int previewWidth{0};
};

struct NextListState {
    UINT dpi{96};
    HFONT bodyFont{};
    HFONT headerFont{};
    HIMAGELIST rowHeightImageList{};
    HWND header{};
    int hotItem{-1};
    int hotDivider{-1};
    int resizePreviewX{-1};
    NextListColumnResizePolicy
        resizePolicy{};
    bool resizePolicyConfigured{false};
    bool userAdjustedColumns{false};
    NextColumnResizeDrag resizeDrag{};
};

[[nodiscard]] NextListState*
ListState(
    HWND list) noexcept {

    return reinterpret_cast<
        NextListState*>(
            GetPropW(
                list,
                kNextListStateProperty));
}

void InvalidateListItem(
    HWND list,
    int itemIndex) {

    if (!list ||
        itemIndex < 0) {
        return;
    }

    RECT rect{};

    if (ListView_GetItemRect(
            list,
            itemIndex,
            &rect,
            LVIR_BOUNDS)) {
        InvalidateRect(
            list,
            &rect,
            FALSE);
    }
}

void ReplaceRowHeightImageList(
    HWND list,
    NextListState& state) {

    if (state.rowHeightImageList) {
        ListView_SetImageList(
            list,
            nullptr,
            LVSIL_SMALL);
        ImageList_Destroy(
            state.rowHeightImageList);
        state.rowHeightImageList =
            nullptr;
    }

    const int rowHeight =
        Scale(30, state.dpi);

    state.rowHeightImageList =
        ImageList_Create(
            1,
            rowHeight,
            ILC_COLOR32,
            1,
            1);

    if (!state.rowHeightImageList) {
        return;
    }

    HBITMAP spacer =
        CreateBitmap(
            1,
            rowHeight,
            1,
            32,
            nullptr);

    if (spacer) {
        ImageList_Add(
            state.rowHeightImageList,
            spacer,
            nullptr);
        DeleteObject(
            spacer);
    }

    ListView_SetImageList(
        list,
        state.rowHeightImageList,
        LVSIL_SMALL);
}

void DrawListFrame(
    HWND list) {

    HDC dc =
        GetDC(list);

    if (!dc) {
        return;
    }

    RECT rect{};
    GetClientRect(
        list,
        &rect);

    if (rect.right >
            rect.left &&
        rect.bottom >
            rect.top) {
        rect.right -= 1;
        rect.bottom -= 1;

        HPEN pen =
            CreatePen(
                PS_SOLID,
                1,
                kApplicationPalette
                    .frame);

        HGDIOBJ oldPen =
            SelectObject(
                dc,
                pen);
        HGDIOBJ oldBrush =
            SelectObject(
                dc,
                GetStockObject(
                    NULL_BRUSH));

        const auto* state =
            ListState(list);
        const UINT dpi =
            state
                ? state->dpi
                : 96;

        RoundRect(
            dc,
            rect.left,
            rect.top,
            rect.right + 1,
            rect.bottom + 1,
            Scale(6, dpi),
            Scale(6, dpi));

        SelectObject(
            dc,
            oldBrush);
        SelectObject(
            dc,
            oldPen);
        DeleteObject(
            pen);
    }

    ReleaseDC(
        list,
        dc);
}

[[nodiscard]] int
HeaderDividerX(
    HWND header,
    int divider) {

    if (!header ||
        divider < 0) {
        return -1;
    }

    RECT itemRect{};

    if (!Header_GetItemRect(
            header,
            divider,
            &itemRect)) {
        return -1;
    }

    return itemRect.right;
}

[[nodiscard]] int
DividerNearPoint(
    HWND header,
    POINT point,
    const NextListState& state) {

    if (!header ||
        !state.resizePolicyConfigured) {
        return -1;
    }

    const int count =
        Header_GetItemCount(
            header);
    const int dividerCount =
        std::min(
            std::max(
                0,
                count - 1),
            std::clamp(
                state.resizePolicy
                    .resizableColumnCount,
                0,
                4));
    const int hitRadius =
        Scale(
            4,
            state.dpi);

    int bestDivider = -1;
    int bestDistance =
        hitRadius + 1;

    for (int divider = 0;
         divider < dividerCount;
         ++divider) {
        const int x =
            HeaderDividerX(
                header,
                divider);

        if (x < 0) {
            continue;
        }

        const int distance =
            std::abs(
                point.x - x);

        if (distance <=
                hitRadius &&
            distance <
                bestDistance) {
            bestDivider =
                divider;
            bestDistance =
                distance;
        }
    }

    return bestDivider;
}

[[nodiscard]] int
MinimumColumnWidth(
    const NextListState& state,
    int column) {

    if (column < 0 ||
        column >= 4) {
        return 1;
    }

    return std::max(
        1,
        Scale(
            state.resizePolicy
                .minimumLogicalWidths[
                    static_cast<
                        std::size_t>(
                            column)],
            state.dpi));
}

[[nodiscard]] int
HeaderContentWidth(
    HWND header,
    HWND list) {

    RECT client{};

    if (header &&
        GetClientRect(
            header,
            &client)) {
        const int width =
            static_cast<int>(
                client.right -
                    client.left);

        if (width > 0) {
            return width;
        }
    }

    if (list &&
        GetClientRect(
            list,
            &client)) {
        return std::max(
            1,
            static_cast<int>(
                client.right -
                    client.left));
    }

    return 1;
}

[[nodiscard]] int
ClampNextListColumnResizeWidth(
    HWND list,
    const NextListState& state,
    int column,
    int proposedWidth) {

    if (!list ||
        !state.resizePolicyConfigured ||
        column < 0 ||
        column >=
            state.resizePolicy
                .resizableColumnCount) {
        return proposedWidth;
    }

    const int minimum =
        MinimumColumnWidth(
            state,
            column);
    const int elastic =
        state.resizePolicy
            .elasticColumn;
    const int minimumElastic =
        MinimumColumnWidth(
            state,
            elastic);
    const int contentWidth =
        HeaderContentWidth(
            state.header,
            list);

    int otherWidth = 0;

    for (int index = 0;
         index <
            state.resizePolicy
                .resizableColumnCount;
         ++index) {
        if (index == column) {
            continue;
        }

        otherWidth +=
            std::max(
                MinimumColumnWidth(
                    state,
                    index),
                ListView_GetColumnWidth(
                    list,
                    index));
    }

    const int maximum =
        std::max(
            minimum,
            contentWidth -
                minimumElastic -
                otherWidth);

    return std::clamp(
        proposedWidth,
        minimum,
        maximum);
}

void UpdateHeaderResizePreview(
    HWND header,
    NextListState& state,
    int column,
    int proposedWidth) {

    if (!header ||
        column < 0 ||
        proposedWidth < 0) {
        return;
    }

    RECT columnRect{};

    if (!Header_GetItemRect(
            header,
            column,
            &columnRect)) {
        return;
    }

    const int previewX =
        columnRect.left +
        proposedWidth;

    if (previewX ==
        state.resizePreviewX) {
        return;
    }

    state.resizePreviewX =
        previewX;

    // The preview is part of the Header's own paint transaction. No child,
    // popup or layered HWND exists, so drag feedback can never overlap or
    // invalidate ListView rows, group text or empty-body pixels.
    InvalidateRect(
        header,
        nullptr,
        FALSE);
}

void ClearHeaderResizePreview(
    HWND header,
    NextListState& state) {

    if (state.resizePreviewX < 0) {
        return;
    }

    state.resizePreviewX = -1;

    if (header &&
        IsWindow(header)) {
        InvalidateRect(
            header,
            nullptr,
            FALSE);
    }
}

void CommitNextListColumnResize(
    HWND header,
    NextListState& state,
    int column,
    int proposedWidth) {

    HWND list =
        GetParent(header);

    if (!list ||
        !state.resizePolicyConfigured ||
        column < 0 ||
        column >=
            state.resizePolicy
                .resizableColumnCount) {
        return;
    }

    const int width =
        ClampNextListColumnResizeWidth(
            list,
            state,
            column,
            proposedWidth);

    int resizableTotal = 0;

    for (int index = 0;
         index <
            state.resizePolicy
                .resizableColumnCount;
         ++index) {
        resizableTotal +=
            index == column
                ? width
                : ListView_GetColumnWidth(
                      list,
                      index);
    }

    const int elastic =
        state.resizePolicy
            .elasticColumn;
    const int elasticWidth =
        std::max(
            MinimumColumnWidth(
                state,
                elastic),
            HeaderContentWidth(
                header,
                list) -
                resizableTotal);
    const int currentElastic =
        ListView_GetColumnWidth(
            list,
            elastic);

    if (elasticWidth <
        currentElastic) {
        ListView_SetColumnWidth(
            list,
            elastic,
            elasticWidth);
    }

    if (ListView_GetColumnWidth(
            list,
            column) != width) {
        ListView_SetColumnWidth(
            list,
            column,
            width);
    }

    if (elasticWidth >=
            currentElastic &&
        currentElastic !=
            elasticWidth) {
        ListView_SetColumnWidth(
            list,
            elastic,
            elasticWidth);
    }

    state.userAdjustedColumns =
        true;
}

void BeginNextListColumnResize(
    HWND header,
    NextListState& state,
    int column,
    int mouseX) {

    HWND list =
        GetParent(header);

    if (!list ||
        column < 0 ||
        column >=
            state.resizePolicy
                .resizableColumnCount) {
        return;
    }

    state.resizeDrag.active =
        true;
    state.resizeDrag.ending =
        false;
    state.resizeDrag.column =
        column;
    state.resizeDrag.startMouseX =
        mouseX;
    state.resizeDrag.startWidth =
        ListView_GetColumnWidth(
            list,
            column);
    state.resizeDrag.previewWidth =
        state.resizeDrag.startWidth;
    state.hotDivider =
        column;

    SetCapture(
        header);

    UpdateHeaderResizePreview(
        header,
        state,
        column,
        state.resizeDrag.previewWidth);

    InvalidateRect(
        header,
        nullptr,
        FALSE);
}

void UpdateNextListColumnResize(
    HWND header,
    NextListState& state,
    int mouseX) {

    if (!state.resizeDrag.active ||
        state.resizeDrag.ending) {
        return;
    }

    HWND list =
        GetParent(header);

    if (!list) {
        return;
    }

    const int proposed =
        state.resizeDrag.startWidth +
        mouseX -
        state.resizeDrag.startMouseX;
    const int preview =
        ClampNextListColumnResizeWidth(
            list,
            state,
            state.resizeDrag.column,
            proposed);

    if (preview ==
        state.resizeDrag
            .previewWidth) {
        return;
    }

    state.resizeDrag.previewWidth =
        preview;

    UpdateHeaderResizePreview(
        header,
        state,
        state.resizeDrag.column,
        preview);
}

void CancelNextListColumnResize(
    HWND header,
    NextListState& state,
    bool releaseCapture) {

    const bool wasActive =
        state.resizeDrag.active;

    ClearHeaderResizePreview(
        header,
        state);
    state.resizeDrag =
        NextColumnResizeDrag{};

    if (releaseCapture &&
        GetCapture() == header) {
        ReleaseCapture();
    }

    if (wasActive) {
        InvalidateRect(
            header,
            nullptr,
            FALSE);
    }
}

void EndNextListColumnResize(
    HWND header,
    NextListState& state) {

    if (!state.resizeDrag.active ||
        state.resizeDrag.ending) {
        return;
    }

    const int column =
        state.resizeDrag.column;
    const int finalWidth =
        state.resizeDrag.previewWidth;

    state.resizeDrag.ending =
        true;

    ClearHeaderResizePreview(
        header,
        state);

    CommitNextListColumnResize(
        header,
        state,
        column,
        finalWidth);

    if (GetCapture() == header) {
        ReleaseCapture();
    }

    state.resizeDrag =
        NextColumnResizeDrag{};
    state.hotDivider =
        column;

    InvalidateRect(
        header,
        nullptr,
        FALSE);
}

void DrawHeaderSurface(
    HWND header,
    HDC dc,
    const NextListState& state) {

    RECT client{};
    GetClientRect(
        header,
        &client);

    HBRUSH background =
        CreateSolidBrush(
            kApplicationPalette
                .cardBackground);
    FillRect(
        dc,
        &client,
        background);
    DeleteObject(
        background);

    SetBkMode(
        dc,
        TRANSPARENT);
    SetTextColor(
        dc,
        kApplicationPalette.text);

    HGDIOBJ oldFont =
        nullptr;

    if (state.headerFont) {
        oldFont =
            SelectObject(
                dc,
                state.headerFont);
    }

    const int count =
        Header_GetItemCount(
            header);
    const int padding =
        Scale(
            12,
            state.dpi);

    for (int index = 0;
         index < count;
         ++index) {
        RECT itemRect{};

        if (!Header_GetItemRect(
                header,
                index,
                &itemRect)) {
            continue;
        }

        std::array<wchar_t, 256>
            text{};

        HDITEMW item{};
        item.mask = HDI_TEXT;
        item.pszText =
            text.data();
        item.cchTextMax =
            static_cast<int>(
                text.size());

        if (!Header_GetItem(
                header,
                index,
                &item)) {
            continue;
        }

        RECT textRect =
            itemRect;
        textRect.left += padding;
        textRect.right -=
            padding;

        DrawTextW(
            dc,
            text.data(),
            -1,
            &textRect,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);

        if (state.hotDivider ==
                index &&
            !state.resizeDrag.active &&
            index <
                count - 1) {
            HPEN guide =
                CreatePen(
                    PS_SOLID,
                    std::max(
                        1,
                        Scale(2, state.dpi)),
                    RGB(171, 199, 224));
            HGDIOBJ oldPen =
                SelectObject(
                    dc,
                    guide);

            const int x =
                itemRect.right;

            MoveToEx(
                dc,
                x,
                itemRect.top +
                    Scale(
                        6,
                        state.dpi),
                nullptr);
            LineTo(
                dc,
                x,
                itemRect.bottom -
                    Scale(
                        6,
                        state.dpi));

            SelectObject(
                dc,
                oldPen);
            DeleteObject(
                guide);
        }
    }

    if (oldFont) {
        SelectObject(
            dc,
            oldFont);
    }

    if (state.resizeDrag.active &&
        state.resizePreviewX >= 0) {
        const int clientLeft =
            static_cast<int>(
                client.left);
        const int clientRight =
            static_cast<int>(
                client.right);
        const int x =
            std::clamp(
                state.resizePreviewX,
                clientLeft,
                std::max(
                    clientLeft,
                    clientRight - 1));

        HPEN preview =
            CreatePen(
                PS_SOLID,
                std::max(
                    1,
                    Scale(
                        2,
                        state.dpi)),
                kResizePreviewColor);
        HGDIOBJ oldPreviewPen =
            SelectObject(
                dc,
                preview);

        MoveToEx(
            dc,
            x,
            client.top +
                Scale(
                    4,
                    state.dpi),
            nullptr);
        LineTo(
            dc,
            x,
            client.bottom -
                Scale(
                    4,
                    state.dpi));

        SelectObject(
            dc,
            oldPreviewPen);
        DeleteObject(
            preview);
    }

    HPEN bottom =
        CreatePen(
            PS_SOLID,
            1,
            kTableHairline);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            bottom);

    MoveToEx(
        dc,
        client.left,
        client.bottom - 1,
        nullptr);
    LineTo(
        dc,
        client.right,
        client.bottom - 1);

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        bottom);
}

LRESULT CALLBACK
NextHeaderSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR refData) {

    auto* state =
        reinterpret_cast<
            NextListState*>(
                refData);

    switch (message) {
    case HDM_LAYOUT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (state &&
            lParam) {
            auto* layout =
                reinterpret_cast<
                    HDLAYOUT*>(
                        lParam);

            if (layout->pwpos &&
                layout->prc) {
                const int height =
                    Scale(
                        34,
                        state->dpi);

                layout->pwpos->cy =
                    height;
                layout->prc->top =
                    layout->pwpos->y +
                    height;
            }
        }

        return result;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc =
            BeginPaint(
                hwnd,
                &paint);

        if (state) {
            DrawHeaderSurface(
                hwnd,
                dc,
                *state);
        }

        EndPaint(
            hwnd,
            &paint);
        return 0;
    }

    case WM_PRINTCLIENT:
        if (state) {
            DrawHeaderSurface(
                hwnd,
                reinterpret_cast<HDC>(
                    wParam),
                *state);
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (state) {
            if (state->resizeDrag.active) {
                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_SIZEWE));
                return TRUE;
            }

            POINT point{};

            if (GetCursorPos(
                    &point) &&
                ScreenToClient(
                    hwnd,
                    &point) &&
                DividerNearPoint(
                    hwnd,
                    point,
                    *state) >= 0) {
                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            if (state->resizeDrag.active) {
                UpdateNextListColumnResize(
                    hwnd,
                    *state,
                    point.x);
                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_SIZEWE));
                return 0;
            }

            const int divider =
                DividerNearPoint(
                    hwnd,
                    point,
                    *state);

            if (divider !=
                state->hotDivider) {
                state->hotDivider =
                    divider;
                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);
            }

            TRACKMOUSEEVENT track{
                sizeof(track),
                TME_LEAVE,
                hwnd,
                0,
            };
            TrackMouseEvent(
                &track);
        }
        break;

    case WM_LBUTTONDOWN:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };
            const int divider =
                DividerNearPoint(
                    hwnd,
                    point,
                    *state);

            if (divider >= 0) {
                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_SIZEWE));
                BeginNextListColumnResize(
                    hwnd,
                    *state,
                    divider,
                    point.x);
                return 0;
            }
        }
        break;

    case WM_LBUTTONDBLCLK:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            if (DividerNearPoint(
                    hwnd,
                    point,
                    *state) >= 0) {
                return 0;
            }
        }
        break;

    case WM_LBUTTONUP:
        if (state &&
            state->resizeDrag.active) {
            EndNextListColumnResize(
                hwnd,
                *state);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (state &&
            state->resizeDrag.active) {
            if (state->resizeDrag.ending) {
                return 0;
            }

            CancelNextListColumnResize(
                hwnd,
                *state,
                false);
            return 0;
        }
        break;

    case WM_CANCELMODE:
        if (state &&
            state->resizeDrag.active) {
            CancelNextListColumnResize(
                hwnd,
                *state,
                true);
            return 0;
        }
        break;

    case WM_MOUSELEAVE:
        if (state &&
            state->hotDivider != -1 &&
            !state->resizeDrag.active) {
            state->hotDivider = -1;
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }
        break;

    case WM_SETFONT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (state) {
            state->headerFont =
                reinterpret_cast<HFONT>(
                    wParam);
        }

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return result;
    }

    case WM_NCDESTROY:
        if (state) {
            if (state->resizeDrag.active) {
                CancelNextListColumnResize(
                    hwnd,
                    *state,
                    true);
            }

            if (state->header == hwnd) {
                state->header = nullptr;
            }
        }

        RemoveWindowSubclass(
            hwnd,
            NextHeaderSubclassProc,
            subclassId);
        break;

    default:
        break;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

LRESULT CALLBACK
NextListSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR refData) {

    auto* state =
        reinterpret_cast<
            NextListState*>(
                refData);

    switch (message) {
    case WM_MOUSEMOVE:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            LVHITTESTINFO hit{};
            hit.pt = point;

            const int hotItem =
                ListView_HitTest(
                    hwnd,
                    &hit);

            if (hotItem !=
                state->hotItem) {
                const int oldHot =
                    state->hotItem;
                state->hotItem =
                    hotItem;

                InvalidateListItem(
                    hwnd,
                    oldHot);
                InvalidateListItem(
                    hwnd,
                    hotItem);
            }

            TRACKMOUSEEVENT track{
                sizeof(track),
                TME_LEAVE,
                hwnd,
                0,
            };
            TrackMouseEvent(
                &track);
        }
        break;

    case WM_MOUSELEAVE:
        if (state &&
            state->hotItem >= 0) {
            const int oldHot =
                state->hotItem;
            state->hotItem = -1;

            InvalidateListItem(
                hwnd,
                oldHot);
        }
        break;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        break;


    case WM_PAINT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        DrawListFrame(
            hwnd);
        return result;
    }

    case WM_PRINTCLIENT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        DrawListFrame(
            hwnd);
        return result;
    }

    case WM_NCDESTROY:
        if (state) {
            if (state->header &&
                IsWindow(
                    state->header)) {
                RemoveWindowSubclass(
                    state->header,
                    NextHeaderSubclassProc,
                    kNextHeaderSubclassId);
                state->header =
                    nullptr;
            }


            if (state->
                    rowHeightImageList) {
                ListView_SetImageList(
                    hwnd,
                    nullptr,
                    LVSIL_SMALL);
                ImageList_Destroy(
                    state->
                        rowHeightImageList);
                state->
                    rowHeightImageList =
                        nullptr;
            }

            RemovePropW(
                hwnd,
                kNextListStateProperty);
            RemoveWindowSubclass(
                hwnd,
                NextListSubclassProc,
                subclassId);
            delete state;
        }
        break;

    default:
        break;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

void InitializeNextListView(
    HWND list,
    UINT dpi,
    HFONT bodyFont,
    HFONT headerFont) {

    if (!list) {
        return;
    }

    auto* state =
        ListState(list);

    if (!state) {
        state =
            new NextListState();

        if (!SetPropW(
                list,
                kNextListStateProperty,
                reinterpret_cast<HANDLE>(
                    state))) {
            delete state;
            return;
        }

        SetWindowSubclass(
            list,
            NextListSubclassProc,
            kNextListSubclassId,
            reinterpret_cast<DWORD_PTR>(
                state));
    }

    state->dpi =
        dpi != 0
            ? dpi
            : 96;
    state->bodyFont =
        bodyFont;
    state->headerFont =
        headerFont
            ? headerFont
            : bodyFont;

    const DWORD extended =
        static_cast<DWORD>(
            ListView_GetExtendedListViewStyle(
                list));

    ListView_SetExtendedListViewStyle(
        list,
        extended |
            LVS_EX_FULLROWSELECT |
            LVS_EX_DOUBLEBUFFER);

    SetWindowTheme(
        list,
        L"Explorer",
        nullptr);

    HWND header =
        ListView_GetHeader(
            list);

    if (header) {
        if (state->header &&
            state->header != header &&
            IsWindow(
                state->header)) {
            RemoveWindowSubclass(
                state->header,
                NextHeaderSubclassProc,
                kNextHeaderSubclassId);
        }

        state->header =
            header;

        LONG_PTR headerStyle =
            GetWindowLongPtrW(
                header,
                GWL_STYLE);
        headerStyle &=
            ~static_cast<LONG_PTR>(
                HDS_FULLDRAG);
        headerStyle |=
            static_cast<LONG_PTR>(
                HDS_NOSIZING);
        SetWindowLongPtrW(
            header,
            GWL_STYLE,
            headerStyle);

        // The native Header remains the column/layout/accessibility model.
        // UiListView exclusively owns divider hit testing, mouse capture,
        // preview and commit; native Header resize gestures are disabled.
        SetWindowTheme(
            header,
            L"",
            L"");

        SetWindowSubclass(
            header,
            NextHeaderSubclassProc,
            kNextHeaderSubclassId,
            reinterpret_cast<DWORD_PTR>(
                state));
    }

    ListView_SetBkColor(
        list,
        kApplicationPalette
            .controlBackground);
    ListView_SetTextBkColor(
        list,
        kApplicationPalette
            .controlBackground);
    ListView_SetTextColor(
        list,
        kApplicationPalette.text);

    if (bodyFont) {
        SendMessageW(
            list,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                bodyFont),
            TRUE);
    }

    if (header &&
        state->headerFont) {
        SendMessageW(
            header,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                state->headerFont),
            TRUE);
    }

    ReplaceRowHeightImageList(
        list,
        *state);

    if (header) {
        InvalidateRect(
            header,
            nullptr,
            FALSE);
    }

    RedrawWindow(
        list,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN);
}

void ConfigureNextListColumnResize(
    HWND list,
    const NextListColumnResizePolicy&
        policy) {

    auto* state =
        ListState(list);

    if (!state) {
        return;
    }

    state->resizePolicy =
        policy;
    state->resizePolicy
        .resizableColumnCount =
        std::clamp(
            policy.resizableColumnCount,
            0,
            4);

    for (int& minimum :
         state->resizePolicy
             .minimumLogicalWidths) {
        minimum =
            std::max(
                1,
                minimum);
    }

    const int elastic =
        state->resizePolicy
            .elasticColumn;

    state->resizePolicyConfigured =
        state->resizePolicy
                .resizableColumnCount >
            0 &&
        elastic >=
            state->resizePolicy
                .resizableColumnCount &&
        elastic >= 0 &&
        elastic < 4;

    if (!state->resizePolicyConfigured &&
        state->header &&
        state->resizeDrag.active) {
        CancelNextListColumnResize(
            state->header,
            *state,
            true);
    }

    if (state->header) {
        InvalidateRect(
            state->header,
            nullptr,
            FALSE);
    }
}

bool NextListHasUserAdjustedColumns(
    HWND list) noexcept {

    const auto* state =
        ListState(list);

    return state &&
        state->userAdjustedColumns;
}

COLORREF NextListRowBackground(
    HWND list,
    int itemIndex,
    bool selected) {

    const auto& palette =
        kApplicationPalette;

    if (selected) {
        return palette.selectionBackground;
    }

    const auto* state =
        ListState(list);

    if (state &&
        state->hotItem ==
            itemIndex) {
        return palette.accentBackground;
    }

    return palette.controlBackground;
}

COLORREF NextListRowText(
    bool selected) noexcept {

    return selected
        ? kApplicationPalette
              .selectionText
        : kApplicationPalette.text;
}

int NextListCellPadding(
    UINT dpi) noexcept {

    return Scale(
        12,
        dpi);
}

void DrawNextListRowSeparator(
    HDC dc,
    const RECT& row) {

    if (!dc) {
        return;
    }

    HPEN separator =
        CreatePen(
            PS_SOLID,
            1,
            kTableHairline);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            separator);

    MoveToEx(
        dc,
        row.left,
        row.bottom - 1,
        nullptr);
    LineTo(
        dc,
        row.right,
        row.bottom - 1);

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        separator);
}

} // namespace altrun::ui
