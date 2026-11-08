# Landscape (rotwrap4)

## The problem

The 2008 driver presents straight into the physical 240x320 portrait framebuffer. A landscape app makes a 320x240 window, and the result was a picture drawn into the top-left corner of the portrait screen, partly off-screen, with the desktop visible around it.

## What didn't work

**Offscreen render + rotated composite (rotwrap1-3).** The plan was to render into a framebuffer object and draw it rotated at swap time. The driver is OpenGL ES **1.0**: `GL_OES_framebuffer_object` isn't in its extension list, and `glCheckFramebufferStatusOES` returns `GL_FRAMEBUFFER_UNSUPPORTED` (0x8CDD) for every colour format. The fallback (render to a pbuffer, `glCopyTexSubImage2D` into a texture, composite) ran at 9 fps, and then crashed. ES 1.0 has none of the `glGet*` state queries a compositor needs to save and restore the app's state, so the "restored" vertex pointer was garbage.

## What works: pre-rotation

rotwrap4 never copies anything. When an app creates a landscape window surface:

1. The real EGL surface is created on a hidden 240x320 popup window at (0,0), so the driver is happy.
2. `eglQuerySurface` reports the app's logical size (320x240).
3. A 90° rotation is folded into the **bottom** of the projection matrix:
   - at first `eglMakeCurrent` for a context, projection becomes `R`
   - `glLoadIdentity` in `GL_PROJECTION` mode becomes `R`
   - `glLoadMatrixf/x(M)` in projection becomes `R * M`
   - everything the app multiplies on afterwards (`glFrustum`, `glOrtho`, `glRotate`, ...) multiplies on the right, so the final transform is `R * P * MV`
4. `glViewport` and `glScissor` rectangles are mapped from landscape to portrait:

```
+90:  (x, y, w, h)  ->  (240 - y - h,  x,            h, w)
-90:  (x, y, w, h)  ->  (y,            320 - x - w,  h, w)
```

The GPU then draws the picture already rotated, at full speed: cuberun runs at 70 fps in landscape against 69 in portrait.

`glMatrixMode` is tracked so the wrapper knows when projection is current, since ES 1.0 can't be asked.

## Settings

`HKLM\Software\KaiserGPU`:

| Value | Default | Meaning |
|---|---|---|
| `LandscapeRotation` | 90 | 90 or 270, if the picture comes out upside down |
| `HelperVisible` | 0 | 1 makes the hidden helper window visible (debugging) |
| `DisableRotation` | 0 | 1 = plain HTC-CA behaviour, no rotation |

Trace: `\rotwrap.log`.

## Not handled yet

- `glReadPixels`: rectangle and pixel order aren't remapped
- `glDrawTexOES` (`GL_OES_draw_texture`): screen-space, so it comes out unrotated
- Apps that rotate themselves (like `cuberun2`) get rotated twice
