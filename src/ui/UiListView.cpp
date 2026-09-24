#include "UiListView.hpp"

#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <array>

namespace altrun::ui {
namespace {

constexpr wchar_t
    kNextListStateProperty[] =
        L"ALTRunNext.UiListView.State";

constexpr UINT_PTR
    kNextListSubclassId =
        0x1A57;

struct NextListState {
    UINT dpi{96};
    HFONT bodyFont{};
    HFONT headerFont{};
    HIMAGELIST rowHeightImageList{};
    int hotItem{-1};
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
        SetWindowTheme(
            header,
            L"Explorer",
            nullptr);
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

    RedrawWindow(
        list,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN);
}

LRESULT DrawNextListHeader(
    NMCUSTOMDRAW* draw,
    UINT dpi,
    HFONT headerFont) {

    if (!draw) {
        return CDRF_DODEFAULT;
    }

    if (draw->dwDrawStage ==
        CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }

    if (draw->dwDrawStage !=
        CDDS_ITEMPREPAINT) {
        return CDRF_DODEFAULT;
    }

    const auto& palette =
        kApplicationPalette;

    RECT rect =
        draw->rc;

    const bool hot =
        (draw->uItemState &
         (CDIS_HOT |
          CDIS_SELECTED)) != 0;

    HBRUSH background =
        CreateSolidBrush(
            hot
                ? palette.cardBackground
                : palette.accentBackground);

    FillRect(
        draw->hdc,
        &rect,
        background);
    DeleteObject(
        background);

    std::array<wchar_t, 256>
        text{};

    HDITEMW item{};
    item.mask = HDI_TEXT;
    item.pszText =
        text.data();
    item.cchTextMax =
        static_cast<int>(
            text.size());

    Header_GetItem(
        draw->hdr.hwndFrom,
        static_cast<int>(
            draw->dwItemSpec),
        &item);

    SetBkMode(
        draw->hdc,
        TRANSPARENT);
    SetTextColor(
        draw->hdc,
        palette.text);

    HGDIOBJ oldFont =
        nullptr;

    if (headerFont) {
        oldFont =
            SelectObject(
                draw->hdc,
                headerFont);
    }

    RECT textRect =
        rect;
    textRect.left +=
        Scale(10, dpi);
    textRect.right -=
        Scale(10, dpi);

    DrawTextW(
        draw->hdc,
        text.data(),
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    if (oldFont) {
        SelectObject(
            draw->hdc,
            oldFont);
    }

    const COLORREF dividerColor =
        RGB(236, 239, 243);

    HPEN separator =
        CreatePen(
            PS_SOLID,
            1,
            dividerColor);
    HGDIOBJ oldPen =
        SelectObject(
            draw->hdc,
            separator);

    MoveToEx(
        draw->hdc,
        rect.left,
        rect.bottom - 1,
        nullptr);
    LineTo(
        draw->hdc,
        rect.right,
        rect.bottom - 1);

    MoveToEx(
        draw->hdc,
        rect.right - 1,
        rect.top +
            Scale(8, dpi),
        nullptr);
    LineTo(
        draw->hdc,
        rect.right - 1,
        rect.bottom -
            Scale(8, dpi));

    SelectObject(
        draw->hdc,
        oldPen);
    DeleteObject(
        separator);

    return CDRF_SKIPDEFAULT;
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
        10,
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
            kApplicationPalette
                .separator);
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
