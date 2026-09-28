# Third-Party Notices

## miniz

ZIP extraction uses miniz 3.1.2 under the MIT license. Its full license is
distributed as `third_party/miniz-LICENSE.txt` with the application.

## Original ALTRun Classic visual assets

Asterun's **Classic** mode includes the original Classic background and two bitmap glyphs extracted from the original ALTRun repository:

- `Form/frmALTRun.dfm` → `imgBackground.Picture.Data` (original `BG.jpg` JPEG payload; retained as `classic_bg.jpg` source/provenance asset and materialized to `classic_bg.bmp` for native runtime loading)
- `Form/frmALTRun.dfm` → `btnShortCut.Glyph.Data`
- `Form/frmALTRun.dfm` → `btnClose.Glyph.Data`

Source repository: `etworker/ALTRun`.

These visual assets are included in Asterun with permission from the original ALTRun author. The surrounding Asterun implementation is independently developed.

## Original ALTRun popup sound

Asterun also includes the original `Res/Popup.wav` from `etworker/ALTRun`, redistributed with permission from the original author:

- `Res/Popup.wav` → `src/resources/altrun_popup.wav`

The sound is embedded once and used by Asterun's existing optional sound-feedback policy.

## Asterun brand assets

The **Asterun** application icon, notification-area icon, wordmark, orbital-star visual language, and README hero are Asterun brand assets. They are independent of the original ALTRun application icon.

Other third-party dependencies remain subject to their respective licenses and notices.
