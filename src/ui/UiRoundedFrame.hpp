#pragma once

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <vector>

namespace altrun::ui {

// Corner radius shared by every single-line input surface (native EDIT
// chrome and Next ComboBox), so neighbouring fields read as one family.
inline constexpr int kControlCornerRadiusLogical = 4;

// Paint `area` with a 1px rounded outline along `frame`: pixels outside the
// outline get `outside`, pixels inside get `fill`. GDI RoundRect has no
// anti-aliasing, so its corners step visibly; this rasterizes them with 4x4
// coverage while straight edges on integer bounds stay pixel-crisp. Pixels
// inside `hole` (if any) are left untouched.
inline void PaintSmoothRoundedFrame(
    HDC dc,
    const RECT& area,
    const RECT& frame,
    int radius,
    COLORREF outside,
    COLORREF frameColor,
    COLORREF fill,
    const RECT* hole = nullptr) noexcept {

    const int width = area.right - area.left;
    const int height = area.bottom - area.top;
    if (!dc || width <= 0 || height <= 0) return;

    // Geometry in area-local coordinates.
    const float l = static_cast<float>(frame.left - area.left);
    const float t = static_cast<float>(frame.top - area.top);
    const float r = static_cast<float>(frame.right - area.left);
    const float b = static_cast<float>(frame.bottom - area.top);
    const float maxRadius = std::max(0.0f, std::min(r - l, b - t) / 2.0f);
    const float outerRadius =
        std::clamp(static_cast<float>(radius), 0.0f, maxRadius);
    const float innerRadius = std::max(0.0f, outerRadius - 1.0f);

    const auto inside = [](float x, float y, float il, float it, float ir,
                           float ib, float rad) {
        if (x < il || y < it || x >= ir || y >= ib) return false;
        const float cx = std::clamp(x, il + rad, ir - rad);
        const float cy = std::clamp(y, it + rad, ib - rad);
        const float dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy <= rad * rad;
    };
    const auto colorAt = [&](float x, float y) {
        if (!inside(x, y, l, t, r, b, outerRadius)) return outside;
        return inside(x, y, l + 1.0f, t + 1.0f, r - 1.0f, b - 1.0f,
                   innerRadius)
            ? fill : frameColor;
    };
    const auto pack = [](COLORREF color) {
        return (static_cast<DWORD>(GetRValue(color)) << 16) |
            (static_cast<DWORD>(GetGValue(color)) << 8) |
            static_cast<DWORD>(GetBValue(color));
    };

    const int saved = SaveDC(dc);
    if (hole) {
        ExcludeClipRect(dc, hole->left, hole->top, hole->right, hole->bottom);
    }

    // Straight runs are exact pixel rectangles: fill them as disjoint bands
    // (outside strips, 1px ring, interior) so nothing is painted twice and
    // large surfaces cost a handful of FillRect calls.
    const auto fillBand = [&](LONG left, LONG top, LONG right, LONG bottom,
                              HBRUSH brush) {
        if (brush && right > left && bottom > top) {
            const RECT band{left, top, right, bottom};
            FillRect(dc, &band, brush);
        }
    };
    HBRUSH outsideBrush = CreateSolidBrush(outside);
    HBRUSH frameBrush = CreateSolidBrush(frameColor);
    HBRUSH fillBrush = CreateSolidBrush(fill);
    fillBand(area.left, area.top, area.right, frame.top, outsideBrush);
    fillBand(area.left, frame.bottom, area.right, area.bottom, outsideBrush);
    fillBand(area.left, frame.top, frame.left, frame.bottom, outsideBrush);
    fillBand(frame.right, frame.top, area.right, frame.bottom, outsideBrush);
    fillBand(frame.left, frame.top, frame.right, frame.top + 1, frameBrush);
    fillBand(frame.left, frame.bottom - 1, frame.right, frame.bottom,
        frameBrush);
    fillBand(frame.left, frame.top + 1, frame.left + 1, frame.bottom - 1,
        frameBrush);
    fillBand(frame.right - 1, frame.top + 1, frame.right, frame.bottom - 1,
        frameBrush);
    fillBand(frame.left + 1, frame.top + 1, frame.right - 1,
        frame.bottom - 1, fillBrush);
    for (HBRUSH brush : {outsideBrush, frameBrush, fillBrush}) {
        if (brush) DeleteObject(brush);
    }

    // Only the four corner squares need anti-aliasing (4x4 coverage).
    const int frameWidth = static_cast<int>(frame.right - frame.left);
    const int frameHeight = static_cast<int>(frame.bottom - frame.top);
    const int box = std::min({
        static_cast<int>(outerRadius) + 2,
        frameWidth / 2,
        frameHeight / 2});
    if (outerRadius > 0.0f && box > 0) {
        std::vector<DWORD> pixels(
            static_cast<std::size_t>(box) * static_cast<std::size_t>(box));
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = box;
        info.bmiHeader.biHeight = -box; // top-down
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        constexpr int samples = 4;
        const POINT corners[]{
            {frame.left, frame.top},
            {frame.right - box, frame.top},
            {frame.left, frame.bottom - box},
            {frame.right - box, frame.bottom - box},
        };
        for (const POINT& corner : corners) {
            for (int y = 0; y < box; ++y) {
                for (int x = 0; x < box; ++x) {
                    const float px = static_cast<float>(
                        corner.x - area.left + x);
                    const float py = static_cast<float>(
                        corner.y - area.top + y);
                    int red = 0, green = 0, blue = 0;
                    for (int sy = 0; sy < samples; ++sy) {
                        for (int sx = 0; sx < samples; ++sx) {
                            const COLORREF color = colorAt(
                                px + (sx + 0.5f) / samples,
                                py + (sy + 0.5f) / samples);
                            red += GetRValue(color);
                            green += GetGValue(color);
                            blue += GetBValue(color);
                        }
                    }
                    constexpr int total = samples * samples;
                    pixels[static_cast<std::size_t>(y) * box + x] =
                        pack(RGB(red / total, green / total, blue / total));
                }
            }
            SetDIBitsToDevice(dc, corner.x, corner.y, box, box, 0, 0, 0,
                box, pixels.data(), &info, DIB_RGB_COLORS);
        }
    }

    if (saved) RestoreDC(dc, saved);
}
// Anti-aliased checkbox glyph shared by every custom checkbox (editor admin
// option, path-conversion rows) so they read as one control family with the
// text fields: 1px outline, `radius` corners, accent fill + white check when
// checked. `background` is what surrounds the glyph (its corner pixels blend
// into it). Drawn with 4x4 coverage in one SetDIBitsToDevice.
inline void PaintSmoothCheckbox(
    HDC dc,
    int left,
    int top,
    int size,
    int radius,
    bool checked,
    bool enabled,
    bool pressed,
    COLORREF background,
    COLORREF accent,
    COLORREF fieldFill) noexcept {

    if (!dc || size <= 0) return;

    const float side = static_cast<float>(size);
    const float unit = side / 18.0f;
    const float outerRadius =
        std::min(side / 2.0f, static_cast<float>(std::max(0, radius)));
    constexpr float borderWidth = 1.0f;
    const COLORREF pressedAccent = RGB(0, 96, 180);
    const COLORREF border = !enabled ? RGB(214, 219, 225)
        : checked ? (pressed ? pressedAccent : accent)
        : RGB(182, 189, 198);
    const COLORREF fill = checked
        ? (pressed ? pressedAccent : accent)
        : fieldFill;

    const auto insideRound = [side](float x, float y, float inset,
                                    float rad) {
        if (x < inset || y < inset ||
            x >= side - inset || y >= side - inset) return false;
        const float nearX = std::clamp(x, inset + rad, side - inset - rad);
        const float nearY = std::clamp(y, inset + rad, side - inset - rad);
        const float dx = x - nearX, dy = y - nearY;
        return dx * dx + dy * dy <= rad * rad;
    };
    const auto nearStroke = [](float x, float y, float ax, float ay,
                               float bx, float by, float rad) {
        const float vx = bx - ax, vy = by - ay;
        const float t = std::clamp(
            ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy),
            0.0f, 1.0f);
        const float dx = x - (ax + t * vx);
        const float dy = y - (ay + t * vy);
        return dx * dx + dy * dy <= rad * rad;
    };

    constexpr int samples = 4;
    std::vector<DWORD> pixels(
        static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int red = 0, green = 0, blue = 0;
            for (int sy = 0; sy < samples; ++sy) {
                for (int sx = 0; sx < samples; ++sx) {
                    const float px = x + (sx + 0.5f) / samples;
                    const float py = y + (sy + 0.5f) / samples;
                    COLORREF color = background;
                    if (insideRound(px, py, 0.0f, outerRadius)) {
                        color = border;
                        if (insideRound(px, py, borderWidth,
                                std::max(0.0f, outerRadius - borderWidth)))
                            color = fill;
                        // Checkmark on an 18-unit grid, 1.6 units wide.
                        if (checked &&
                            (nearStroke(px, py, 4.6f * unit, 9.2f * unit,
                                        7.6f * unit, 12.2f * unit,
                                        0.8f * unit) ||
                             nearStroke(px, py, 7.6f * unit, 12.2f * unit,
                                        13.4f * unit, 6.0f * unit,
                                        0.8f * unit)))
                            color = RGB(255, 255, 255);
                    }
                    red += GetRValue(color);
                    green += GetGValue(color);
                    blue += GetBValue(color);
                }
            }
            constexpr int total = samples * samples;
            pixels[static_cast<std::size_t>(y) * size + x] =
                (static_cast<DWORD>(red / total) << 16) |
                (static_cast<DWORD>(green / total) << 8) |
                static_cast<DWORD>(blue / total);
        }
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size; // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, left, top, size, size, 0, 0, 0, size,
        pixels.data(), &info, DIB_RGB_COLORS);
}

} // namespace altrun::ui
