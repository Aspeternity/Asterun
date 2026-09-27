# Third-Party Notices

## Original ALTRun Classic visual assets

ALTRun Next includes the original Classic background and two bitmap glyphs extracted from the original ALTRun repository:

- `Form/frmALTRun.dfm` → `imgBackground.Picture.Data` (original `BG.jpg` JPEG payload; retained as `classic_bg.jpg` source/provenance asset and materialized to `classic_bg.bmp` for native runtime loading)
- `Form/frmALTRun.dfm` → `btnShortCut.Glyph.Data`
- `Form/frmALTRun.dfm` → `btnClose.Glyph.Data`

Source repository: `etworker/ALTRun`.

These three visual assets are included in ALTRun Next with permission from the original ALTRun author. The surrounding ALTRun Next implementation remains independently developed.

## Original ALTRun application icon and popup sound

ALTRun Next also includes two additional original ALTRun assets from `etworker/ALTRun`
with permission from the original author:

- `ALTRun.res` / `Res/Carracho.ico` → `src/resources/altrun_original.ico` (the original `MAINICON`; the extracted icon is byte-identical to `Res/Carracho.ico`)
- `Res/Popup.wav` → `src/resources/altrun_popup.wav`

The application icon is used for the ALTRun Next executable, top-level product windows and
notification-area icon. The original `Popup.wav` is embedded once and is used by ALTRun Next's
existing sound-feedback policy. These assets are redistributed under the author's permission;
the surrounding ALTRun Next implementation remains independently developed.
