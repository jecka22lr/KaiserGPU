# Third-party binaries

Nothing in this folder was written by this project. These files are copyright their owners (HTC, ATI/AMD, Qualcomm) and are included unmodified, apart from the one patch noted below, so the stack can be installed on a real phone. They come from the community "HTC-CA" Kaiser stack as found in the Kaiser Nino WM6.5 ROM (`Kaiser_Nino_WWE_v2_2`).

If you're a rights holder and want them removed, open an issue.

## htc-ca/

| File | Role |
|---|---|
| `libgles_cm_ks.dll` | OpenGL ES 1.0 + EGL ("KS20") |
| `d3dm_ati.dll` | Direct3D Mobile driver |
| `ahi2dati.dll`, `ahi2dati_dm.dll` | ATI AHI wrapper and core |
| `clkregim.dll`, `msm_clk.dll` | GPU clock control |
| `TrsTai_extensioF.dll`, `wce_reF.dll`, `wce_pmeF.dll`, `wce_rex_dm.dll` | MSM support libraries |

## patched/

`libgles_cm_ks.dll`, with two size-check branches NOPed (see docs/ARCHITECTURE.md). This is the one installed by the CAB.
