# KaiserGPU

Working hardware 3D on the **HTC TyTN II / Kaiser** (MSM7200, ATI Imageon / Qualcomm Q3Dimension GPU) under **Windows Mobile 6.1**, using HTC's own leaked 2008 driver stack, glued together and made landscape-aware.

HTC shipped the Kaiser with a GPU it never enabled. The drivers existed: HTC's "class action" (HTC-CA) stack for the Kaiser surfaced in later community ROMs. This project gets that stack running on a stock-ish official ROM, adds a small wrapper so OpenGL ES apps work in landscape, and ships test tools to prove it.

| | Result |
|---|---|
| OpenGL ES 1.0 (Q3Dimension) | ~300 fps on the gltest triangle, portrait and landscape |
| Direct3D Mobile (d3dm_ati) | Hardware T&L + hardware raster, d3dmrun at ~35 fps |
| cuberun (fog, lighting, depth) | 69-70 fps, landscape at zero extra cost |
| 2D GUI acceleration | Not done. See [docs/2D-FINDINGS.md](docs/2D-FINDINGS.md) |

Tested on a Kaiser 130, ROM 3.29.408.0 ITA, CE OS 5.2.19212, radio 1.65.17.56.

## Install

1. Copy `cab/KaiserGPU_1.0.cab` to the phone and install it to **Device** (not the storage card).
2. Soft reset.
3. Run **gltest** from the Start menu. It should end with `RESULT: PASSED` and a spinning triangle. A full log goes to `\gltest.log`.
4. Run **cuberun** for a quick real-world test (open the keyboard for landscape).

To undo, remove "KaiserGPU 1.0" in Settings > System > Remove Programs and soft reset. If anything ever leaves the phone unbootable, a hard reset (hold both soft keys, poke reset, press Send) returns it to the ROM state.

## What's in here

```
cab/        KaiserGPU_1.0.cab, ready to install, plus the provisioning XML and build script
bin/        our own builds: libgles_cm.dll (rotwrap4), gltest, cuberun, d3dmrun, bench2d
src/        source for everything in bin/
vendor/     HTC / ATI / Qualcomm binaries (not ours, see vendor/NOTICE.md)
tools/      Python scripts used to take ROMs apart (NBH, imgfs, XIP, LZX, WinCE e32)
docs/       how it works and what was learned
```

## Docs

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): the driver stack, what each DLL does, and how it was made to load
- [docs/LANDSCAPE.md](docs/LANDSCAPE.md): how rotwrap4 makes every GL app work in landscape
- [docs/BUILDING.md](docs/BUILDING.md): building the tools and the CAB with CeGCC
- [docs/2D-FINDINGS.md](docs/2D-FINDINGS.md): benchmarks of the 2D path and why the GUI is slow
- [docs/GOTCHAS.md](docs/GOTCHAS.md): the traps that cost the most time

## Known limitations

- No 2D GUI acceleration, and screen tearing remains (it's in HTC's 2D display driver, not the 3D stack).
- In landscape, `glReadPixels` and `glDrawTexOES` aren't remapped yet; HUDs drawn with draw_texture would appear sideways.
- The 2008 driver is OpenGL ES **1.0** only: no framebuffer objects, no state queries (`glGet*` beyond constants).

## License

Our code and docs are MIT, see [LICENSE](LICENSE). The files in `vendor/` (and the copies inside the CAB) belong to HTC, ATI/AMD and Qualcomm and aren't covered by it.
