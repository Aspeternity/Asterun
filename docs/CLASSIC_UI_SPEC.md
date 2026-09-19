# Classic UI specification

Classic mode is intentionally a **behavioral and visual homage** to the old ALTRun launcher while remaining a clean-room implementation.

## Frozen visual structure

At 100% scale, v0.1.2 targets:

- 500 px launcher width;
- 6 px outer margin;
- 23 px top input/hint row;
- 184 px classic input width;
- light-gray operation hint on the right;
- 10 visible results;
- 22 px result row height;
- 202 px shortcut column;
- white two-column result surface;
- 23 px recessed command preview box;
- square Win11 corners;
- silver/gray vertical gradient outer shell.

## Classic result row

The left column renders:

```text
1  chrome
2  code
3  calc
```

The right column renders the human-readable command title/description.

A subtle vertical divider separates both columns. Selection uses a full-width classic Windows blue highlight with white text.

## Classic typography

- Simplified Chinese: SimSun, 9 pt
- English: Tahoma, 9 pt

The goal is to preserve the compact desktop-tool character instead of making Classic look like a modern web application.

## Classic top hint

The upper-right hint cycles through only features that currently exist:

- keyboard selection / run / hide;
- double-click launch and Alt+Space show/hide;
- tray appearance/language/reload controls.

## Modern Compact separation

Classic-specific behavior must not leak into Modern Compact.

Modern Compact keeps:

- 620 px width;
- 32 px rows;
- Win11 rounded corners;
- themed controls;
- larger spacing;
- no shortcut numbering;
- no classic vertical column divider.

## Interaction shared by both themes

| Input | Behavior |
| --- | --- |
| Alt+Space | Show/hide launcher |
| typing | Filter and rank immediately |
| Up/Down | Move selection without leaving the input box |
| Enter | Launch selected result |
| Escape | Hide launcher |
| double click | Launch selected result |

## Non-goals for Classic

Do not add:

- card layouts;
- oversized icons;
- navigation sidebars;
- toolbar ribbons;
- animation-heavy transitions;
- settings pages inside the launcher surface.
