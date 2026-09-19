# Classic UI specification

The v0.1.3 Classic skin is calibrated against a real screenshot of the original ALTRun running on the same Windows desktop where ALTRun Next was tested.

## Reference-derived geometry

The ALTRun Next v0.1.2 screenshot measured about 750 px wide while the code requested 500 logical px, confirming approximately 150% Windows scaling.

The original ALTRun reference measures about 628 px wide and 375 px high. Therefore the Classic target is approximately:

- width: 420 logical px
- height: 250 logical px
- title bar: 30 logical px
- top input/hint strip: 22 logical px
- result row: 16 logical px
- visible result rows: 10
- bottom command strip: 18 logical px

## Reference-derived colors

Approximate sampled colors:

- outer/frame gray: RGB(103,109,115)
- top/bottom pale green: RGB(186,214,190)
- result background: RGB(244,246,248)
- selected row: RGB(4,119,210)
- classic blue text/dividers: around RGB(38,41,145)

## Result columns

Classic is a three-column table:

1. hotkey number
2. shortcut keyword / aliases
3. human-readable description

The first ten visible hotkeys render exactly as:

```text
1 2 3 4 5 6 7 8 9 0
```

## Title bar

The title bar is custom drawn and contains:

- a small launcher emblem on the left;
- dynamic centered text in brackets, e.g. `[calc]`;
- a large red X on the right;
- a dark-to-light horizontal gray gradient with subtle scanline texture.

## Top and bottom strips

The top row and bottom command row use the pale-green skin color rather than native white edit/static backgrounds.

The bottom row prefixes the selected target with:

- Chinese: `命令：`
- English: `Command: `

## Separation from Modern Compact

Modern Compact remains a separate UI path and is not forced to inherit Classic geometry, colors, numbering, or title-bar behavior.
