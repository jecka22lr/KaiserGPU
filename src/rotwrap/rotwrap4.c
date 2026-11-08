/*
 * libgles_cm.dll - KaiserGPU rotating wrapper, v4 ("pre-rotation")
 *
 * The 2008 Q3Dimension driver is OpenGL ES 1.0: no framebuffer objects, and no
 * state queries, so we cannot render offscreen and composite. Instead, when an
 * app creates a landscape window surface, the wrapper:
 *   - creates the real EGL surface on a 240x320 helper window at (0,0)
 *   - reports the surface as 320x240 to the app (eglQuerySurface)
 *   - folds a 90 degree rotation into the bottom of the projection matrix
 *     (glLoadIdentity / glLoadMatrix in PROJECTION mode, and at first bind)
 *   - maps glViewport / glScissor rectangles from landscape to portrait
 * The GPU then draws the picture already rotated, at full speed.
 *
 * Everything else forwards to libgles_cm_ks.dll (see rotwrap4.def).
 * Settings, HKLM\Software\KaiserGPU: LandscapeRotation 90/270, HelperVisible 0/1,
 * DisableRotation 1 = plain HTC-CA behaviour. Trace: \rotwrap.log
 */
#include <windows.h>

typedef int EGLint; typedef unsigned EGLBoolean;
typedef void *EGLDisplay, *EGLConfig, *EGLSurface, *EGLContext;
typedef unsigned GLenum; typedef float GLfloat; typedef int GLint, GLsizei, GLfixed;

#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_TRUE 1
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701

#define FUNCS \
 F(EGLSurface, eglCreateWindowSurface, (EGLDisplay, EGLConfig, HWND, const EGLint*)) \
 F(EGLBoolean, eglDestroySurface, (EGLDisplay, EGLSurface)) \
 F(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext)) \
 F(EGLBoolean, eglSwapBuffers, (EGLDisplay, EGLSurface)) \
 F(EGLBoolean, eglQuerySurface, (EGLDisplay, EGLSurface, EGLint, EGLint*)) \
 F(EGLint, eglGetError, (void)) \
 F(void, glMatrixMode, (GLenum)) \
 F(void, glLoadIdentity, (void)) \
 F(void, glLoadMatrixf, (const GLfloat*)) \
 F(void, glLoadMatrixx, (const GLfixed*)) \
 F(void, glMultMatrixf, (const GLfloat*)) \
 F(void, glMultMatrixx, (const GLfixed*)) \
 F(void, glRotatef, (GLfloat, GLfloat, GLfloat, GLfloat)) \
 F(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
 F(void, glScissor, (GLint, GLint, GLsizei, GLsizei))

#define F(ret, name, args) typedef ret (*PF_##name) args; static PF_##name r_##name;
FUNCS
#undef F

static HMODULE g_ks;
static int g_ready, g_rot = 90, g_helper_visible = 0, g_disabled = 0;

static void tracef(const char *s)
{
    HANDLE h = CreateFileW(L"\\rotwrap.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    DWORD w; int n = 0;
    if (h == INVALID_HANDLE_VALUE) return;
    while (s[n]) n++;
    SetFilePointer(h, 0, NULL, FILE_END);
    WriteFile(h, s, n, &w, NULL); WriteFile(h, "\r\n", 2, &w, NULL);
    CloseHandle(h);
}
static void tracehex(const char *s, unsigned v)
{
    char b[96]; int i = 0, k;
    static const char x[] = "0123456789abcdef";
    while (s[i] && i < 80) { b[i] = s[i]; i++; }
    for (k = 7; k >= 0; k--) b[i++] = x[(v >> (k * 4)) & 15];
    b[i] = 0; tracef(b);
}

static DWORD reg_dword(const WCHAR *name, DWORD def)
{
    HKEY k; DWORD v = def, sz = sizeof(v), t;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\KaiserGPU", 0, 0, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, name, NULL, &t, (BYTE *)&v, &sz) != ERROR_SUCCESS) v = def;
        RegCloseKey(k);
    }
    return v;
}

static int init(void)
{
    if (g_ready) return g_ready > 0;
    g_ks = LoadLibraryW(L"libgles_cm_ks.dll");
    if (!g_ks) { tracef("rotwrap4: cannot load libgles_cm_ks.dll"); g_ready = -1; return 0; }
#define F(ret, name, args) r_##name = (PF_##name)GetProcAddressW(g_ks, L## #name); \
    if (!r_##name) { tracef("rotwrap4: missing " #name); g_ready = -1; return 0; }
    FUNCS
#undef F
    g_rot = reg_dword(L"LandscapeRotation", 90) == 270 ? -90 : 90;
    g_helper_visible = reg_dword(L"HelperVisible", 0) != 0;
    g_disabled = reg_dword(L"DisableRotation", 0) != 0;
    tracehex("rotwrap4: ready, rotation/disabled/visible = ", (unsigned)((g_rot > 0 ? 0x900 : 0x2700) | (g_disabled << 4) | g_helper_visible));
    g_ready = 1;
    return 1;
}

/* HTC-CA behaviour after every swap: ask HTC's DDI to refresh the panel,
   and keep the backlight timer alive every 1500 frames. */
static void screen_refresh(void)
{
    static int frames;
    HDC dc = GetDC(NULL);
    ExtEscape(dc, 100011, 0, NULL, 0, NULL);
    ReleaseDC(NULL, dc);
    if (++frames >= 1500) { frames = 0; SystemIdleTimerReset(); }
}

/* ------------------------------------------------------------------ state */
static struct {
    EGLSurface real;     /* the landscape app's surface (really 240x320 on the helper) */
    HWND helper;
    int lw, lh;          /* logical size reported to the app */
    EGLContext ctx;      /* context we already prepared */
} L;
static int g_active;              /* a landscape surface is current */
static GLenum g_mode = GL_MODELVIEW;

#define PW 240
#define PH 320

static LRESULT CALLBACK helper_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

static HWND make_helper(void)
{
    static int registered;
    if (!registered) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = helper_proc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"KaiserGPURotHelper";
        RegisterClassW(&wc); registered = 1;
    }
    return CreateWindowW(L"KaiserGPURotHelper", L"", WS_POPUP | WS_DISABLED | (g_helper_visible ? WS_VISIBLE : 0),
                         0, 0, PW, PH, NULL, NULL, GetModuleHandleW(NULL), NULL);
}

/* landscape rectangle (bottom-left origin, 320x240) -> portrait rectangle (240x320) */
static void map_rect(GLint *x, GLint *y, GLsizei *w, GLsizei *h)
{
    GLint nx, ny; GLsizei nw = *h, nh = *w;
    if (g_rot > 0) { nx = PW - *y - *h; ny = *x; }        /* +90: U = 240 - Y, V = X */
    else           { nx = *y;           ny = PH - *x - *w; } /* -90: U = Y, V = 320 - X */
    *x = nx; *y = ny; *w = nw; *h = nh;
}

static void base_rotation(void) { r_glRotatef((GLfloat)g_rot, 0, 0, 1); }

/* -------------------------------------------------------------- exports -- */
EGLSurface W_eglCreateWindowSurface(EGLDisplay dpy, EGLConfig cfg, HWND win, const EGLint *attr)
{
    RECT rc;
    if (!init()) return 0;
    GetClientRect(win, &rc);
    if (g_disabled || rc.right <= rc.bottom || L.real)       /* portrait, or a second landscape surface */
        return r_eglCreateWindowSurface(dpy, cfg, win, attr);
    L.helper = make_helper();
    if (!L.helper) { tracef("rotwrap4: helper window failed, passing through"); return r_eglCreateWindowSurface(dpy, cfg, win, attr); }
    L.real = r_eglCreateWindowSurface(dpy, cfg, L.helper, attr);
    if (!L.real) { tracehex("rotwrap4: real surface failed, egl err ", r_eglGetError()); DestroyWindow(L.helper); L.helper = 0; return 0; }
    L.lw = rc.right; L.lh = rc.bottom; L.ctx = 0;
    tracehex("rotwrap4: landscape surface, logical size ", (unsigned)((L.lw << 16) | L.lh));
    return L.real;
}

EGLBoolean W_eglDestroySurface(EGLDisplay dpy, EGLSurface s)
{
    EGLBoolean r;
    if (!init()) return 0;
    r = r_eglDestroySurface(dpy, s);
    if (s && s == L.real) {
        if (L.helper) DestroyWindow(L.helper);
        memset(&L, 0, sizeof(L));
        g_active = 0;
        tracef("rotwrap4: landscape surface destroyed");
    }
    return r;
}

EGLBoolean W_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    EGLBoolean r;
    if (!init()) return 0;
    r = r_eglMakeCurrent(dpy, draw, read, ctx);
    if (!r) return r;
    g_active = (ctx && draw && draw == L.real);
    if (g_active && L.ctx != ctx) {
        /* fresh context: projection is identity, viewport is the full surface,
           matrix mode is MODELVIEW. Put the rotation under the projection. */
        L.ctx = ctx;
        g_mode = GL_MODELVIEW;
        r_glMatrixMode(GL_PROJECTION);
        r_glLoadIdentity();
        base_rotation();
        r_glMatrixMode(GL_MODELVIEW);
        r_glViewport(0, 0, PW, PH);
        tracef("rotwrap4: context prepared for landscape");
    }
    return r;
}

EGLBoolean W_eglSwapBuffers(EGLDisplay dpy, EGLSurface s)
{
    EGLBoolean r;
    if (!init()) return 0;
    r = r_eglSwapBuffers(dpy, s);
    screen_refresh();
    return r;
}

EGLBoolean W_eglQuerySurface(EGLDisplay dpy, EGLSurface s, EGLint attr, EGLint *val)
{
    if (!init()) return 0;
    if (s && s == L.real && val && (attr == EGL_WIDTH || attr == EGL_HEIGHT)) {
        *val = attr == EGL_WIDTH ? L.lw : L.lh;
        return EGL_TRUE;
    }
    return r_eglQuerySurface(dpy, s, attr, val);
}

void W_glMatrixMode(GLenum mode)
{
    if (!init()) return;
    g_mode = mode;
    r_glMatrixMode(mode);
}

void W_glLoadIdentity(void)
{
    if (!init()) return;
    r_glLoadIdentity();
    if (g_active && g_mode == GL_PROJECTION) base_rotation();
}

void W_glLoadMatrixf(const GLfloat *m)
{
    if (!init()) return;
    if (g_active && g_mode == GL_PROJECTION) { r_glLoadIdentity(); base_rotation(); r_glMultMatrixf(m); }
    else r_glLoadMatrixf(m);
}

void W_glLoadMatrixx(const GLfixed *m)
{
    if (!init()) return;
    if (g_active && g_mode == GL_PROJECTION) { r_glLoadIdentity(); base_rotation(); r_glMultMatrixx(m); }
    else r_glLoadMatrixx(m);
}

void W_glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!init()) return;
    if (g_active) map_rect(&x, &y, &w, &h);
    r_glViewport(x, y, w, h);
}

void W_glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!init()) return;
    if (g_active) map_rect(&x, &y, &w, &h);
    r_glScissor(x, y, w, h);
}

BOOL WINAPI DllMain(HANDLE h, DWORD reason, LPVOID r) { return TRUE; }
