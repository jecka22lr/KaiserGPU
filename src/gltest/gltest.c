/*
 * gltest.exe - KaiserGPU step-by-step diagnostic.
 *
 * Loads every DLL of the GPU stack one at a time, then brings up EGL/GLES and
 * draws a spinning triangle for 5 seconds. Every step is written to
 * \gltest.log and flushed immediately, so if the phone hangs, the last line
 * in the log is the step that hung.
 *
 * Everything is resolved with GetProcAddress so no import library is needed.
 */
#include <windows.h>

typedef int   EGLint;
typedef unsigned int EGLBoolean;
typedef void *EGLDisplay, *EGLConfig, *EGLSurface, *EGLContext;
typedef unsigned int GLenum, GLbitfield;
typedef float GLfloat;
typedef int GLint, GLsizei;
typedef unsigned char GLubyte;

#define EGL_DEFAULT_DISPLAY ((void *)0)
#define EGL_NONE            0x3038
#define EGL_RED_SIZE        0x3024
#define EGL_GREEN_SIZE      0x3023
#define EGL_BLUE_SIZE       0x3022
#define EGL_DEPTH_SIZE      0x3025
#define EGL_SURFACE_TYPE    0x3033
#define EGL_WINDOW_BIT      0x0004
#define EGL_VENDOR          0x3053
#define EGL_VERSION         0x3054

#define GL_VENDOR      0x1F00
#define GL_RENDERER    0x1F01
#define GL_VERSION     0x1F02
#define GL_EXTENSIONS  0x1F03
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_TRIANGLES   0x0004
#define GL_FLOAT       0x1406
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY  0x8076
#define GL_PROJECTION  0x1701
#define GL_MODELVIEW   0x1700
#define EGL_BUFFER_SIZE 0x3020
#define EGL_ALPHA_SIZE  0x3021
#define EGL_CONFIG_ID   0x3028
#define EGL_WIDTH       0x3057
#define EGL_HEIGHT      0x3056
#define EGL_PBUFFER_BIT 0x0001
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401

typedef EGLDisplay (*PFeglGetDisplay)(void *);
typedef EGLBoolean (*PFeglInitialize)(EGLDisplay, EGLint *, EGLint *);
typedef EGLBoolean (*PFeglChooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
typedef EGLSurface (*PFeglCreateWindowSurface)(EGLDisplay, EGLConfig, HWND, const EGLint *);
typedef EGLContext (*PFeglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
typedef EGLBoolean (*PFeglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef EGLBoolean (*PFeglSwapBuffers)(EGLDisplay, EGLSurface);
typedef EGLint     (*PFeglGetError)(void);
typedef const char*(*PFeglQueryString)(EGLDisplay, EGLint);
typedef EGLBoolean (*PFeglTerminate)(EGLDisplay);
typedef EGLBoolean (*PFeglGetConfigs)(EGLDisplay, EGLConfig *, EGLint, EGLint *);
typedef EGLBoolean (*PFeglGetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint *);
typedef EGLSurface (*PFeglCreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint *);
typedef void (*PFglReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
typedef EGLBoolean (*PFeglDestroy)(EGLDisplay, void *);
typedef const GLubyte *(*PFglGetString)(GLenum);
typedef void (*PFglClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*PFglClear)(GLbitfield);
typedef void (*PFglViewport)(GLint, GLint, GLsizei, GLsizei);
typedef void (*PFglEnum)(GLenum);
typedef void (*PFglVoid)(void);
typedef void (*PFglRotatef)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*PFglPointer)(GLint, GLenum, GLsizei, const void *);
typedef void (*PFglDrawArrays)(GLenum, GLint, GLsizei);
typedef void (*PFglOrthof)(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat);
typedef GLenum (*PFglGetError)(void);

static HANDLE g_log = INVALID_HANDLE_VALUE;
static void logw(const WCHAR *fmt, ...);
static WCHAR g_sum[2048];
static void sum(const WCHAR *fmt, ...)
{
    WCHAR b[256]; va_list ap; va_start(ap, fmt); wvsprintfW(b, fmt, ap); va_end(ap);
    if (lstrlenW(g_sum) + lstrlenW(b) + 3 < 2048) { lstrcatW(g_sum, b); lstrcatW(g_sum, L"\n"); }
}
static int has(const char *s, const char *needle)
{
    int i, j;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        for (j = 0; needle[j]; j++) {
            char a = s[i + j], b = needle[j];
            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;
            if (a != b) break;
        }
        if (!needle[j]) return 1;
    }
    return 0;
}
typedef void *(*PFD3DMCreate)(UINT);
typedef HRESULT (*PFGetAdapterIdentifier)(void *, UINT, ULONG, void *);
typedef ULONG (*PFRelease)(void *);

/* Direct3D Mobile: which driver does the runtime actually pick up? */
static void test_d3dm(void)
{
    HKEY k; WCHAR hook[128]; DWORD sz = sizeof(hook), type;
    HMODULE d3dm; PFD3DMCreate create; void *d3d; void **vt;
    static WCHAR ident[1100];
    logw(L"--- D3DM check");
    hook[0] = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\D3DM\\Drivers", 0, 0, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"LocalHook", NULL, &type, (BYTE *)hook, &sz);
        RegCloseKey(k);
    }
    logw(L"registry LocalHook = %s", hook[0] ? hook : L"(not set)");
    sum(L"D3DM driver (registry): %s", hook[0] ? hook : L"(not set)");
    d3dm = LoadLibraryW(L"d3dm.dll");
    if (!d3dm) { logw(L"[FAIL] LoadLibrary d3dm.dll (%u)", GetLastError()); sum(L"D3DM runtime: missing"); return; }
    create = (PFD3DMCreate)GetProcAddressW(d3dm, L"Direct3DMobileCreate");
    if (!create) { logw(L"[FAIL] Direct3DMobileCreate not exported"); sum(L"D3DM runtime: broken"); return; }
    d3d = create(120);
    if (!d3d) { logw(L"[FAIL] Direct3DMobileCreate(120) returned NULL"); sum(L"D3DM create: FAILED"); return; }
    vt = *(void ***)d3d;
    memset(ident, 0, sizeof(ident));
    if (((PFGetAdapterIdentifier)vt[5])(d3d, 0, 0, ident) == 0) {
        logw(L"[ OK ] D3DM adapter driver: %s", ident);
        logw(L"       description: %s", ident + 512);
        sum(L"D3DM loaded: %s", ident);
    } else {
        logw(L"[FAIL] GetAdapterIdentifier");
        sum(L"D3DM adapter: FAILED");
    }
    ((PFRelease)vt[2])(d3d);
}
static int g_fail = 0;

static void logw(const WCHAR *fmt, ...)
{
    WCHAR wbuf[512];
    char  abuf[1024];
    DWORD n, w;
    va_list ap;
    va_start(ap, fmt);
    wvsprintfW(wbuf, fmt, ap);
    va_end(ap);
    lstrcatW(wbuf, L"\r\n");
    n = WideCharToMultiByte(CP_ACP, 0, wbuf, -1, abuf, sizeof(abuf), NULL, NULL);
    if (n) n--;
    if (g_log != INVALID_HANDLE_VALUE) {
        WriteFile(g_log, abuf, n, &w, NULL);
        FlushFileBuffers(g_log);
    }
}

static void loga(const WCHAR *label, const char *s)
{
    WCHAR w[400];
    int i;
    if (!s) { logw(L"%s: (null)", label); return; }
    for (i = 0; s[i] && i < 399; i++) w[i] = (WCHAR)(unsigned char)s[i];
    w[i] = 0;
    logw(L"%s: %s", label, w);
}

static HMODULE try_load(const WCHAR *name)
{
    HMODULE h;
    logw(L"       loading %s ...", name);
    h = LoadLibraryW(name);
    if (h) logw(L"[ OK ] LoadLibrary %s", name);
    else { logw(L"[FAIL] LoadLibrary %s  (error %u)", name, GetLastError()); g_fail = 1; }
    return h;
}

#define GETPROC(var, type, name) \
    var = (type)GetProcAddressW(gl, L##name); \
    if (!var) { logw(L"[FAIL] missing export " L##name); g_fail = 1; goto done; }


typedef int (*PFAhiInit)(int);
typedef int (*PFAhiDevEnum)(void *, void *, int);
/* Ask ATI's hardware layer directly whether it can see a GPU. */
static void probe_ahi(void)
{
    HMODULE a = GetModuleHandleW(L"ahi2dati.dll");
    PFAhiInit init; PFAhiDevEnum en;
    static unsigned char info[0x200];
    unsigned dev = 0;
    int rc, i, j;
    WCHAR line[80];
    logw(L"--- step 2b: AHI probe");
    if (!a) a = LoadLibraryW(L"ahi2dati.dll");
    if (!a) { logw(L"[FAIL] ahi2dati.dll not loaded"); return; }
    init = (PFAhiInit)GetProcAddressW(a, L"AhiInit");
    en = (PFAhiDevEnum)GetProcAddressW(a, L"AhiDevEnum");
    if (!init || !en) { logw(L"[FAIL] AhiInit/AhiDevEnum missing"); return; }
    logw(L"calling AhiInit(0)...");
    rc = init(0);
    logw(L"AhiInit -> %d (0x%x)  [0 ok, 6 shared-mem, 0x6000 HAL, 0x6003 no device]", rc, rc);
    sum(L"AhiInit: %d", rc);
    memset(info, 0xcd, sizeof(info));
    logw(L"calling AhiDevEnum...");
    rc = en(&dev, info, 0);
    logw(L"AhiDevEnum -> %d, handle %08x", rc, dev);
    sum(L"AhiDevEnum: %d", rc);
    for (i = 0; i < 0x150; i += 16) {
        WCHAR *p = line;
        p += wsprintfW(p, L"  %03x:", i);
        for (j = 0; j < 16; j++) p += wsprintfW(p, L" %02x", info[i + j]);
        logw(L"%s", line);
    }
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return DefWindowProcW(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    HMODULE gl = NULL;
    HWND wnd = NULL;
    WNDCLASSW wc;
    RECT rc;
    EGLDisplay dpy = 0; EGLConfig cfg = 0; EGLSurface surf = 0; EGLContext ctx = 0;
    EGLint major = 0, minor = 0, ncfg = 0;
    DWORD t0, t1;
    int frames = 0;
    float angle = 0;
    int hwgl = 0, fps = 0;
    static const EGLint attrs[] = { EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5,
                                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    static const GLfloat verts[] = { 0.0f, 0.8f,  -0.7f, -0.6f,  0.7f, -0.6f };
    static const GLfloat cols[]  = { 1,0,0,1,  0,1,0,1,  0,0,1,1 };

    PFeglGetDisplay eglGetDisplay; PFeglInitialize eglInitialize; PFeglChooseConfig eglChooseConfig;
    PFeglCreateWindowSurface eglCreateWindowSurface; PFeglCreateContext eglCreateContext;
    PFeglMakeCurrent eglMakeCurrent; PFeglSwapBuffers eglSwapBuffers; PFeglGetError eglGetError;
    PFeglQueryString eglQueryString; PFeglTerminate eglTerminate;
    PFeglDestroy eglDestroyContext, eglDestroySurface;
    PFglGetString glGetString; PFglClearColor glClearColor; PFglClear glClear; PFglViewport glViewport;
    PFglEnum glMatrixMode, glEnableClientState; PFglVoid glLoadIdentity, glFinish;
    PFglRotatef glRotatef; PFglPointer glVertexPointer, glColorPointer;
    PFglDrawArrays glDrawArrays; PFglOrthof glOrthof; PFglGetError glGetError;
    PFeglGetConfigs eglGetConfigs; PFeglGetConfigAttrib eglGetConfigAttrib;
    PFeglCreatePbufferSurface eglCreatePbufferSurface; PFglReadPixels glReadPixels;
    EGLConfig cfgs[64]; EGLint ncfgs = 0; int ci; int pbuffer_mode = 0;

    g_log = CreateFileW(L"\\gltest.log", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    logw(L"gltest v7 (HTC-CA stack) - KaiserGPU diagnostic");
    g_sum[0] = 0;

    /* 1. load the stack bottom-up so a missing dependency names itself */
    logw(L"--- step 1: load DLLs bottom-up");
    try_load(L"wce_reF.dll");
    try_load(L"wce_pmeF.dll");
    try_load(L"TrsTai_extensioF.dll");
    try_load(L"msm_clk.dll");
    try_load(L"clkregim.dll");
    try_load(L"ahi2dati_dm.dll");
    try_load(L"ahi2dati.dll");
    probe_ahi();   /* first AhiInit in this process: before libgles can call it */
    try_load(L"libgles_cm_ks.dll");
    gl = try_load(L"libgles_cm.dll");
    try_load(L"d3dm_ati.dll");
    if (!gl) { sum(L"OpenGL ES: driver not installed"); goto done; }

    GETPROC(eglGetDisplay, PFeglGetDisplay, "eglGetDisplay");
    GETPROC(eglInitialize, PFeglInitialize, "eglInitialize");
    GETPROC(eglChooseConfig, PFeglChooseConfig, "eglChooseConfig");
    GETPROC(eglCreateWindowSurface, PFeglCreateWindowSurface, "eglCreateWindowSurface");
    GETPROC(eglCreateContext, PFeglCreateContext, "eglCreateContext");
    GETPROC(eglMakeCurrent, PFeglMakeCurrent, "eglMakeCurrent");
    GETPROC(eglSwapBuffers, PFeglSwapBuffers, "eglSwapBuffers");
    GETPROC(eglGetError, PFeglGetError, "eglGetError");
    GETPROC(eglQueryString, PFeglQueryString, "eglQueryString");
    GETPROC(eglTerminate, PFeglTerminate, "eglTerminate");
    GETPROC(eglDestroyContext, PFeglDestroy, "eglDestroyContext");
    GETPROC(eglDestroySurface, PFeglDestroy, "eglDestroySurface");
    GETPROC(glGetString, PFglGetString, "glGetString");
    GETPROC(glClearColor, PFglClearColor, "glClearColor");
    GETPROC(glClear, PFglClear, "glClear");
    GETPROC(glViewport, PFglViewport, "glViewport");
    GETPROC(glMatrixMode, PFglEnum, "glMatrixMode");
    GETPROC(glEnableClientState, PFglEnum, "glEnableClientState");
    GETPROC(glLoadIdentity, PFglVoid, "glLoadIdentity");
    GETPROC(glFinish, PFglVoid, "glFinish");
    GETPROC(glRotatef, PFglRotatef, "glRotatef");
    GETPROC(glVertexPointer, PFglPointer, "glVertexPointer");
    GETPROC(glColorPointer, PFglPointer, "glColorPointer");
    GETPROC(glDrawArrays, PFglDrawArrays, "glDrawArrays");
    GETPROC(glOrthof, PFglOrthof, "glOrthof");
    GETPROC(glGetError, PFglGetError, "glGetError");
    GETPROC(eglGetConfigs, PFeglGetConfigs, "eglGetConfigs");
    GETPROC(eglGetConfigAttrib, PFeglGetConfigAttrib, "eglGetConfigAttrib");
    GETPROC(eglCreatePbufferSurface, PFeglCreatePbufferSurface, "eglCreatePbufferSurface");
    GETPROC(glReadPixels, PFglReadPixels, "glReadPixels");
    logw(L"[ OK ] all GL/EGL entry points resolved");

    /* 2. window */
    logw(L"--- step 2: window");
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = L"gltest";
    RegisterClassW(&wc);
    wnd = CreateWindowW(L"gltest", L"gltest", WS_VISIBLE, 0, 0,
                        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                        NULL, NULL, inst, NULL);
    if (!wnd) { logw(L"[FAIL] CreateWindow (error %u)", GetLastError()); g_fail = 1; goto done; }
    GetClientRect(wnd, &rc);
    logw(L"[ OK ] window %dx%d", rc.right, rc.bottom);

    /* 3. EGL bring-up: this is where the GPU gets powered and initialised */
    logw(L"--- step 3: EGL (GPU power-up happens here)");
    logw(L"calling eglGetDisplay...");
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    logw(L"eglGetDisplay -> %08x", (unsigned)dpy);
    logw(L"calling eglInitialize... (if the log stops here, the GPU did not come up)");
    if (!eglInitialize(dpy, &major, &minor)) {
        logw(L"[FAIL] eglInitialize, eglGetError=%04x", eglGetError()); g_fail = 1;
        sum(L">> GPU did NOT initialise (eglInitialize failed)"); goto done;
    }
    logw(L"[ OK ] eglInitialize, EGL %d.%d", major, minor);
    loga(L"EGL_VENDOR", eglQueryString(dpy, EGL_VENDOR));
    loga(L"EGL_VERSION", eglQueryString(dpy, EGL_VERSION));

    if (!eglChooseConfig(dpy, attrs, &cfg, 1, &ncfg) || ncfg < 1) {
        logw(L"[FAIL] eglChooseConfig n=%d err=%04x", ncfg, eglGetError()); g_fail = 1; goto done;
    }
    logw(L"[ OK ] eglChooseConfig (%d)", ncfg);
    logw(L"screen %dx%d (%s)", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
         GetSystemMetrics(SM_CXSCREEN) > GetSystemMetrics(SM_CYSCREEN) ? L"LANDSCAPE" : L"portrait");
    sum(L"Screen: %s", GetSystemMetrics(SM_CXSCREEN) > GetSystemMetrics(SM_CYSCREEN) ? L"landscape" : L"portrait");
    eglGetConfigs(dpy, cfgs, 64, &ncfgs);
    logw(L"%d EGL configs:", ncfgs);
    for (ci = 0; ci < ncfgs; ci++) {
        EGLint id = 0, bs = 0, r = 0, g = 0, b = 0, a = 0, d = 0, st = 0;
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_CONFIG_ID, &id);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_BUFFER_SIZE, &bs);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_RED_SIZE, &r);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_BLUE_SIZE, &b);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_ALPHA_SIZE, &a);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_DEPTH_SIZE, &d);
        eglGetConfigAttrib(dpy, cfgs[ci], EGL_SURFACE_TYPE, &st);
        logw(L"  cfg id=%d bits=%d rgba=%d%d%d%d depth=%d surf=%x", id, bs, r, g, b, a, d, st);
    }
    surf = eglCreateWindowSurface(dpy, cfg, wnd, NULL);
    if (!surf) {
        logw(L"[FAIL] eglCreateWindowSurface (chosen cfg) err=%04x", eglGetError());
        for (ci = 0; ci < ncfgs && !surf; ci++) {
            surf = eglCreateWindowSurface(dpy, cfgs[ci], wnd, NULL);
            logw(L"  window surface with config #%d -> %s (err %04x)", ci, surf ? L"OK" : L"fail", surf ? 0 : eglGetError());
            if (surf) cfg = cfgs[ci];
        }
    }
    if (!surf) {
        static const EGLint pb[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
        sum(L"Window surface: FAILED (0x3009 = config/window mismatch)");
        logw(L"--- fallback: off-screen pbuffer (tests the GPU without the display)");
        for (ci = 0; ci < ncfgs && !surf; ci++) {
            surf = eglCreatePbufferSurface(dpy, cfgs[ci], pb);
            logw(L"  pbuffer with config #%d -> %s (err %04x)", ci, surf ? L"OK" : L"fail", surf ? 0 : eglGetError());
            if (surf) cfg = cfgs[ci];
        }
        if (!surf) { sum(L"Pbuffer: FAILED"); g_fail = 1; goto done; }
        pbuffer_mode = 1;
        rc.right = 64; rc.bottom = 64;
        sum(L"Off-screen pbuffer: OK");
    } else {
        logw(L"[ OK ] eglCreateWindowSurface");
        sum(L"Window surface: OK");
    }
    ctx = eglCreateContext(dpy, cfg, 0, NULL);
    if (!ctx) { logw(L"[FAIL] eglCreateContext err=%04x", eglGetError()); g_fail = 1; goto done; }
    logw(L"[ OK ] eglCreateContext");
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
        logw(L"[FAIL] eglMakeCurrent err=%04x", eglGetError()); g_fail = 1; goto done;
    }
    logw(L"[ OK ] eglMakeCurrent");

    {
        const char *v = (const char *)glGetString(GL_VENDOR), *r = (const char *)glGetString(GL_RENDERER);
        WCHAR w[128]; int i;
        loga(L"GL_VENDOR", v);
        loga(L"GL_RENDERER", r);
        for (i = 0; r && r[i] && i < 127; i++) w[i] = (WCHAR)(unsigned char)r[i];
        w[i] = 0;
        sum(L"GL renderer: %s", w);
        hwgl = !(has(r, "soft") || has(r, "generic") || has(v, "microsoft"));
    }
    loga(L"GL_VERSION", (const char *)glGetString(GL_VERSION));
    loga(L"GL_EXTENSIONS", (const char *)glGetString(GL_EXTENSIONS));

    /* 4. draw for 5 seconds */
    logw(L"--- step 4: render 5 seconds");
    glViewport(0, 0, rc.right, rc.bottom);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glColorPointer(4, GL_FLOAT, 0, cols);

    t0 = GetTickCount();
    do {
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glLoadIdentity();
        glRotatef(angle, 0, 0, 1);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        eglSwapBuffers(dpy, surf);
        angle += 2.0f;
        frames++;
        if (frames == 1) logw(L"[ OK ] first frame swapped, glGetError=%04x", glGetError());
        t1 = GetTickCount();
    } while (t1 - t0 < 5000);
    glFinish();
    if (pbuffer_mode) {
        unsigned char px[4] = {0, 0, 0, 0};
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        logw(L"pbuffer readback after red clear: %d %d %d %d", px[0], px[1], px[2], px[3]);
        sum(px[0] > 200 && px[1] < 50 ? L"GPU readback: correct (GPU renders)" : L"GPU readback: WRONG");
    }
    fps = frames * 1000 / (int)(t1 - t0);
    logw(L"[ OK ] %d frames in %u ms = %d fps", frames, t1 - t0, fps);
    sum(L"Triangle test: %d fps", fps);
    sum(hwgl && fps >= 30 ? L">> 3D HARDWARE ACCELERATION: WORKING"
        : hwgl ? L">> GPU driver runs but is slow (<30 fps): suspicious"
               : L">> OpenGL ES is running in SOFTWARE");

done:
    if (dpy) {
        eglMakeCurrent(dpy, 0, 0, 0);
        if (ctx) eglDestroyContext(dpy, ctx);
        if (surf) eglDestroySurface(dpy, surf);
        eglTerminate(dpy);
    }
    test_d3dm();
    logw(g_fail ? L"=== RESULT: FAILED (see [FAIL] lines)" : L"=== RESULT: PASSED");
    if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
    if (wnd) DestroyWindow(wnd);
    sum(L"(details in \\gltest.log)");
    MessageBoxW(NULL, g_sum, g_fail ? L"gltest: problems found" : L"gltest results", MB_OK);
    return 0;
}
