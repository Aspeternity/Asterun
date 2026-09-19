# Classic UI specification

This document freezes the design target for the daily launcher so later feature work does not gradually turn it into a large modern dashboard.

## Design target

The launcher should preserve the classic ALTRun interaction model:

- the cursor is placed in a small input box immediately after the hotkey;
- results appear in a white compact list below the input;
- the left side emphasizes the shortcut/keyword;
- the right side shows the human-readable description;
- the bottom line can show the currently selected command/path;
- the default visual language is a restrained silver/gray desktop-tool skin;
- the launcher disappears immediately after execution or Escape;
- configuration UI is separate and must never be permanently attached to the launcher.

## v0.1 geometry

At 100% scale:

- launcher width: 520 px;
- outer margin: 7 px;
- input height: 27 px;
- row height: 24 px;
- visible results: 11;
- command preview: 22 px;
- keyword column: approximately 135 px.

All geometry is scaled with per-monitor DPI v2.

## Interaction

| Input | Behavior |
| --- | --- |
| Alt+Space | Show/hide launcher |
| typing | Filter and rank immediately |
| Up/Down | Move selection without leaving the input box |
| Enter | Launch selected result |
| Escape | Hide launcher |
| double click | Launch selected result |

## Non-goals for the launcher surface

Do not add the following to the compact launcher by default:

- large application icons;
- card layouts;
- navigation sidebars;
- toolbar ribbons;
- animation-heavy transitions;
- settings pages inside the popup;
- web-style spacing and oversized controls.

Those can exist in a separate settings center if needed.

## Next visual pass

The next UI pass should add, in order:

1. silver gradient/background treatment closer to classic ALTRun;
2. right-side contextual hint text in the input row;
3. more accurate classic border/selection spacing;
4. optional hidden command preview, matching old behavior;
5. Classic Dark as a separate skin without changing geometry.
