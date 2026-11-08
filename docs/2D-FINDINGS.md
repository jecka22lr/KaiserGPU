# 2D findings

2D GUI acceleration was the last goal and isn't done. This is what was measured and learned, as a starting point for whoever picks it up.

## The Diamond DDI can't be dropped in

The HTC Touch Diamond's `DDI.dll` (MSM7201A) uses AHI for 2D and was the original "hybrid driver" candidate. It doesn't port as a CAB:

- In the Diamond ROM it's an imgfs module with **relocations stripped**, pre-placed at fixed addresses (code 0x03DFC000, data 0x01FD5000). It only runs at those addresses, which means cooking it into a ROM, not installing it.
- It's written for the Diamond's 480x640 MDDI panel.
- Its only missing `disptools` imports on the Kaiser are `CreateMDDIUpdateThread` (called from one DrvEscape case only) and two global handles that are safe as zero. Those are easy; the fixed addresses aren't.

## The Kaiser DDI already uses hardware 2D

The stock Kaiser `DDI.dll` (QComGPE) opens AHI's "Windows2D Renderer" and routes large blits through the Qualcomm MDP. Its own log string says it falls back to software blits only when the display is off.

## Benchmarks (bench2d, landscape)

| Operation | Speed |
|---|---|
| Full-screen fill | 460/s |
| Screen-to-screen scroll | 490/s |
| Compatible bitmap to screen blt | 163-206/s |
| 16-bit DIB to screen blt | 52/s |
| 2x StretchBlt | 23/s |
| 32x32 FillRect | ~1300/s |
| One 48-character text line | ~85/s |

Per-call timing (bench2d v2):

| | On screen | Off-screen DDI bitmap | Off-screen DIB (plain RAM) |
|---|---|---|---|
| Text line | 11.6 ms | 6.3 ms | **1.0 ms** |
| 32x32 fill | 0.51 ms | 0.28 ms | 0.16 ms |
| Diagonal line | 0.48 ms | 0.30 ms | 0.32 ms |

## Conclusions

1. **CPU-drawn graphics on DDI-owned memory are slow.** Text into a plain DIB is about 12x faster than the same text on screen. The screen and the DDI's bitmaps live in memory the CPU reaches uncached. HTC's `MMMAP.dll` maps it with `PAGE_NOCACHE` by writing page-table entries directly, and the kernel's `VirtualSetAttributes` refuses that range (error 87).
2. **About half of each on-screen text line is waiting for the panel.** Almost every on-screen text call lands at 10-15 ms, one refresh. The DDI pushes dirty rectangles to the panel through `ddi_disp2d_update` (MDP DMA) without syncing to the refresh, which is also the tearing.
3. Blits and fills done by the hardware are fine.

## Where a fix would live

A modified DDI that maps the framebuffer cacheable, writes the cache back before each `ddi_disp2d_update` and before/after hardware blits, and syncs updates to vblank. Because ROM files can't be overwritten, it would load through the GWES display-driver registry entry under a new name, so the first boot test carries hard-reset risk.

## For app writers

Draw into a DIB section and blit the finished frame to the screen in one call. That uses the fast path on both ends.
