# Gotchas

The traps that cost the most time, in roughly the order they bit.

**A second AHI instance freezes the phone.** The Kaiser display driver already owns AHI. Initialising a private copy with a different shared-memory layout took over the GPU and froze GWES. Use the HTC-CA `ahi2dati_dm` patched to the DDI's 0x178F4 layout.

**`AhiInit` returns 0 when the shared memory already exists.** It looks like failure but means "attached". Probe AHI before loading `libgles_cm_ks`, not after.

**Modified signed DLLs are rejected.** Error 0x80090006. Strip the Authenticode directory. CABs containing them get rejected too.

**Unstripped CeGCC DLLs give error 193.** Always link DLLs with `-s`.

**`%CE1%` isn't always `\Program Files`.** Italian ROM: `\Programmi`.

**OpenGL ES 1.0 means no FBOs and no state queries.** Don't plan anything that renders offscreen or saves and restores GL state. Pre-rotate in the projection matrix instead.

**A GPU wedged by a crashed GL app makes the next `eglInitialize` fail with 0x3001.** Soft reset clears it.

**`MessageBox` from a fullscreen GL app may never show.** Use `MB_SETFOREGROUND | MB_TOPMOST`.

**CAB uninstall order.** Uninstalling a CAB removes its files even after another CAB overwrote them. Remove the old one first.

**The landscape touchscreen.** Not a driver issue; on the test phone only landscape touch worked (probably the flex cable).
