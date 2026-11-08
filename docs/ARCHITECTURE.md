# Architecture

## Hardware

- **SoC:** Qualcomm MSM7200, ARM1136 apps processor
- **GPU:** ATI Imageon (Qualcomm Q3Dimension), reports `Q3Dimension MSM7500 01.02.01 0 4.0.0`
- **Screen:** 240x320 portrait panel. Landscape is GWES rotating the GDI picture; the physical framebuffer stays portrait.

The stock Kaiser ROM ships no 3D driver at all. Its display driver (`DDI.dll`) embeds ATI's AHI (ATI Handheld Interface) library for its own 2D use, which matters below.

## The stack

All of these go in `\Windows`. The CAB installs them bottom-up in this dependency order:

| DLL | Origin | Role |
|---|---|---|
| `wce_reF.dll`, `wce_pmeF.dll` | HTC-CA | REX (Qualcomm RTOS) shim and physical-memory allocator, renamed so they don't clash with the ROM's own copies |
| `TrsTai_extensioF.dll` | HTC-CA | Clock and power helpers, renamed copy |
| `msm_clk.dll`, `clkregim.dll` | HTC-CA | GPU clock control |
| `ahi2dati_dm.dll` | HTC-CA | ATI AHI 2D/3D core. Diamond build, patched by HTC-CA to the Kaiser DDI's AHI shared-memory layout (0x178F4 bytes) |
| `ahi2dati.dll` | HTC-CA | Wrapper that forwards to `ahi2dati_dm` and copies the DDI's AHI shared state |
| `libgles_cm_ks.dll` | HTC-CA ("KS20"), **patched** | The real OpenGL ES 1.0 + EGL implementation |
| `libgles_cm.dll` | **ours (rotwrap4)** | Front-end every GL app loads. Forwards everything to `libgles_cm_ks` and adds landscape support |
| `d3dm_ati.dll` | HTC-CA | Direct3D Mobile driver, built on top of `libgles_cm` |
| `wce_rex_dm.dll` | HTC-CA | REX support used by the Diamond-derived pieces |

Registry: `HKLM\System\D3DM\Drivers\LocalHook = d3dm_ati.dll` points D3DM at the hardware driver instead of HTC's stub `htc_d3dm.dll`.

### Why the shared memory matters

The Kaiser's DDI already initialises AHI and creates the named shared memory `AhiSrmSharedMem` (0x178F4 bytes). A second AHI instance with a different layout either fails (`AhiInit` sees the existing mapping and bails) or, worse, takes over the GPU and freezes GWES. The HTC-CA `ahi2dati_dm` build is patched to the Kaiser DDI's layout, so the 3D stack attaches to the AHI state the display driver already owns instead of fighting it.

## The libgles_cm_ks patch

`libgles_cm_ks.dll` checks the window surface size and refuses anything wider than the panel's portrait width. Two branches are NOPed:

| Address | Original | Patched |
|---|---|---|
| `0x100138d4` | `bgt 0x1001392c` | `mov r0, r0` (`0000a0e1`) |
| `0x100138e4` | `bgt 0x1001392c` | `mov r0, r0` (`0000a0e1`) |

With rotwrap4 the real surface is always 240x320, so the check no longer bites; the patch is kept so raw landscape surfaces don't hard-fail.

## What HTC's original libgles_cm.dll did

The 2008 HTC-CA `libgles_cm.dll` is 236 export forwarders plus one real function, `eglSwapBuffers`, which:

1. calls the real swap in `libgles_cm_ks`
2. calls `ExtEscape(hdc, 100011, ...)` on the screen DC, asking HTC's DDI to refresh the panel
3. every 1500 frames, calls `SystemIdleTimerReset()` so the backlight doesn't time out during a game

rotwrap4 reproduces both behaviours exactly (ordinals checked against the Diamond and Kaiser `coredll` export tables).

## D3DM

`d3dm_ati.dll` reports DevCaps `0x1009080c`: hardware transform & lighting and hardware rasterisation. `d3dmrun` exercises vertex buffers, lighting, materials and fog; all HRESULTs come back 0.
