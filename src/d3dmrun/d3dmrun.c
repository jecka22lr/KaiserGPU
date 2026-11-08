/*
 * d3dmrun.exe - the cuberun game rewritten on Direct3D Mobile.
 *
 * Tests the D3DM path (d3dm.dll -> LocalHook driver -> d3dm_ati.dll -> libgles).
 * Vtable slots come from vt.h, generated from the WM6 SDK's d3dm.h.
 * Portrait only. D-pad left/right to dodge, centre/back to quit.
 * Every setup step and the final result go to \d3dmrun.log.
 */
#include <windows.h>
#include "vt.h"

typedef long HRES;
#define CALL(obj, slot, type) ((type)((*(void ***)(obj))[slot]))

/* --- the few D3DM types we need (layouts from d3dmtypes.h / d3dmcaps.h) --- */
typedef struct { UINT BackBufferWidth, BackBufferHeight, BackBufferFormat, BackBufferCount,
                 MultiSampleType, SwapEffect; BOOL Windowed, EnableAutoDepthStencil;
                 UINT AutoDepthStencilFormat; ULONG Flags; UINT FullScreen_PresentationInterval; } PRESENTP;
typedef struct { float r, g, b, a; } COLV;               /* D3DMVALUE as float bits */
typedef struct { COLV Diffuse, Ambient, Specular; float Power; } MATERIAL;
typedef struct { UINT Type; COLV Diffuse, Specular, Ambient; float Position[3], Direction[3];
                 float Range, Att0, Att1, Att2; } LIGHT;
typedef struct { ULONG X, Y, Width, Height; float MinZ, MaxZ; } VIEWPORT;
typedef struct { UINT DeviceType, AdapterOrdinal; ULONG PresentationIntervals, DevCaps, SurfaceCaps,
                 PrimitiveMiscCaps, RasterCaps, ZCmpCaps; ULONG rest[40]; } CAPS;

#define FMT_UNKNOWN 0
#define FMT_D16 24
#define FMT_FLOAT 30                     /* D3DMFMT_D3DMVALUE_FLOAT */
#define SWAP_DISCARD 1
#define FVF_XYZ_FLOAT 0x0000
#define FVF_NORMAL_FLOAT 0x0008
#define POOL_VIDEOMEM 0
#define POOL_SYSTEMMEM 1
#define PT_TRIANGLELIST 4
#define CLEAR_TARGET 1
#define CLEAR_ZBUFFER 2
#define TS_WORLD 0
#define TS_VIEW 1
#define TS_PROJECTION 2
#define RS_CULLMODE 7
#define RS_FOGENABLE 13
#define RS_FOGCOLOR 15
#define RS_FOGSTART 17
#define RS_FOGEND 18
#define RS_LIGHTING 37
#define RS_AMBIENT 38
#define RS_FOGVERTEXMODE 39
#define RS_ZENABLE 48
#define FOG_LINEAR 3
#define CULL_NONE 1
#define LIGHT_DIRECTIONAL 3
#define DEVCAPS_HWTNL 0x00010000
#define DEVCAPS_HWRAST 0x00080000

typedef void *(*PFCreate)(UINT);
typedef ULONG (*PFRelease)(void *);
typedef HRES (*PFGetAdapterIdentifier)(void *, UINT, ULONG, void *);
typedef HRES (*PFGetDeviceCaps)(void *, UINT, UINT, CAPS *);
typedef HRES (*PFCreateDevice)(void *, UINT, UINT, HWND, ULONG, PRESENTP *, void **);
typedef HRES (*PFCreateVB)(void *, UINT, DWORD, DWORD, UINT, void **);
typedef HRES (*PFLock)(void *, UINT, UINT, void **, DWORD);
typedef HRES (*PFUnlock)(void *);
typedef HRES (*PFNoArg)(void *);
typedef HRES (*PFClear)(void *, DWORD, const RECT *, DWORD, ULONG, float, DWORD);
typedef HRES (*PFSetTransform)(void *, UINT, const float *, UINT);
typedef HRES (*PFSetViewport)(void *, const VIEWPORT *);
typedef HRES (*PFSetMaterial)(void *, const MATERIAL *, UINT);
typedef HRES (*PFSetLight)(void *, DWORD, const LIGHT *, UINT);
typedef HRES (*PFLightEnable)(void *, DWORD, BOOL);
typedef HRES (*PFSetRenderState)(void *, UINT, DWORD);
typedef HRES (*PFDrawPrimitive)(void *, UINT, UINT, UINT);
typedef HRES (*PFSetStreamSource)(void *, UINT, void *, UINT);
typedef HRES (*PFPresent)(void *, const RECT *, const RECT *, HWND, const void *);

static HANDLE g_log = INVALID_HANDLE_VALUE;
static void logw(const WCHAR *fmt, ...)
{
    WCHAR w[300]; char a[600]; DWORD n, wr; va_list ap;
    va_start(ap, fmt); wvsprintfW(w, fmt, ap); va_end(ap);
    lstrcatW(w, L"\r\n");
    n = WideCharToMultiByte(CP_ACP, 0, w, -1, a, sizeof(a), NULL, NULL);
    if (g_log != INVALID_HANDLE_VALUE && n) { WriteFile(g_log, a, n - 1, &wr, NULL); FlushFileBuffers(g_log); }
}
static DWORD fbits(float f) { union { float f; DWORD d; } u; u.f = f; return u.d; }

static int keys_left, keys_right, quit;
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN || m == WM_KEYUP) {
        int down = (m == WM_KEYDOWN);
        if (w == VK_LEFT) keys_left = down;
        if (w == VK_RIGHT) keys_right = down;
        if (down && (w == VK_RETURN || w == VK_ESCAPE || w == VK_BACK)) quit = 1;
        return 0;
    }
    if (m == WM_CLOSE) { quit = 1; return 0; }
    return DefWindowProcW(h, m, w, l);
}

/* 4x4 row-major matrices, D3D convention (row vectors, left-handed) */
static void mat_ident(float *m) { int i; for (i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }
static void mat_world(float *m, float sx, float sy, float sz, float tx, float ty, float tz)
{
    mat_ident(m); m[0] = sx; m[5] = sy; m[10] = sz; m[12] = tx; m[13] = ty; m[14] = tz;
}
static void mat_persp(float *m, float w, float h, float zn, float zf)
{
    int i; for (i = 0; i < 16; i++) m[i] = 0;
    m[0] = 2 * zn / w; m[5] = 2 * zn / h; m[10] = zf / (zf - zn); m[11] = 1; m[14] = -zn * zf / (zf - zn);
}
static void mat_rollz(float *m, float deg)
{
    /* small-angle rotation about Z without libm: use a short Taylor series */
    float r = deg * 0.0174533f, r2 = r * r;
    float s = r * (1 - r2 / 6 + r2 * r2 / 120), c = 1 - r2 / 2 + r2 * r2 / 24;
    mat_ident(m); m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
}

#define NOBS 24
static float ox[NOBS], oz[NOBS], oc[NOBS];
static unsigned rng = 4242;
static float frand(void) { rng = rng * 1103515245u + 12345u; return ((rng >> 8) & 0xffff) / 65535.0f; }
static void respawn(int i, float zmin) { ox[i] = frand() * 6.0f - 3.0f; oz[i] = zmin + frand() * 30.0f; oc[i] = frand(); }

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    HMODULE d3dmdll; PFCreate create;
    void *d3d = NULL, *dev = NULL, *vb = NULL;
    WNDCLASSW wc; HWND wnd; RECT rc; MSG msg;
    PRESENTP pp; CAPS caps; VIEWPORT vp; MATERIAL mat; LIGHT lt;
    static WCHAR ident[1100];
    float proj[16], view[16], world[16];
    float *v; HRES hr;
    float px = 0, speed = 8.0f, dist = 0, roll = 0;
    DWORD t0, tprev, tnow; int frames = 0, i, crashed = 0;
    WCHAR res[300];
    /* one unit cube: 12 triangles, position + normal */
    static const int f[6][4][3] = {
        {{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1}}, {{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1},{-1,-1, 1}},
        {{-1,-1, 1},{-1, 1, 1},{-1, 1,-1},{-1,-1,-1}}, {{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1},{ 1,-1, 1}},
        {{-1, 1,-1},{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1}}, {{-1,-1, 1},{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1}} };
    static const int nrm[6][3] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    static const int tri[6] = {0, 1, 2, 0, 2, 3};

    g_log = CreateFileW(L"\\d3dmrun.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    logw(L"d3dmrun - Direct3D Mobile test");
    if (GetSystemMetrics(SM_CXSCREEN) > GetSystemMetrics(SM_CYSCREEN)) {
        logw(L"landscape: refusing");
        MessageBoxW(NULL, L"Close the keyboard first: the 3D driver only works in portrait.", L"d3dmrun",
                    MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
        return 0;
    }

    d3dmdll = LoadLibraryW(L"d3dm.dll");
    create = d3dmdll ? (PFCreate)GetProcAddressW(d3dmdll, L"Direct3DMobileCreate") : NULL;
    if (!create || !(d3d = create(120))) { logw(L"[FAIL] Direct3DMobileCreate"); goto fail; }
    logw(L"[ OK ] Direct3DMobileCreate");

    if (CALL(d3d, VT_Mobile_GetAdapterIdentifier, PFGetAdapterIdentifier)(d3d, 0, 0, ident) == 0)
        logw(L"adapter driver '%s' description '%s'", ident, ident + 512);
    memset(&caps, 0, sizeof(caps));
    hr = CALL(d3d, VT_Mobile_GetDeviceCaps, PFGetDeviceCaps)(d3d, 0, 0, &caps);
    logw(L"GetDeviceCaps hr=%08x DevCaps=%08x  HW T&L: %s  HW raster: %s  RasterCaps=%08x",
         hr, caps.DevCaps, caps.DevCaps & DEVCAPS_HWTNL ? L"yes" : L"no",
         caps.DevCaps & DEVCAPS_HWRAST ? L"yes" : L"no", caps.RasterCaps);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc; wc.hInstance = inst; wc.lpszClassName = L"d3dmrun";
    RegisterClassW(&wc);
    wnd = CreateWindowW(L"d3dmrun", L"d3dmrun", WS_VISIBLE, 0, 0,
                        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, inst, NULL);
    SetForegroundWindow(wnd);
    GetClientRect(wnd, &rc);

    memset(&pp, 0, sizeof(pp));
    pp.Windowed = TRUE; pp.SwapEffect = SWAP_DISCARD; pp.BackBufferFormat = FMT_UNKNOWN;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = FMT_D16;
    hr = CALL(d3d, VT_Mobile_CreateDevice, PFCreateDevice)(d3d, 0, 0, wnd, 0, &pp, &dev);
    logw(L"CreateDevice hr=%08x dev=%08x", hr, (unsigned)dev);
    if (hr || !dev) goto fail;

    hr = CALL(dev, VT_MobileDevice_CreateVertexBuffer, PFCreateVB)(dev, 36 * 24, 0, FVF_XYZ_FLOAT | FVF_NORMAL_FLOAT,
                                                                   POOL_SYSTEMMEM, &vb);
    logw(L"CreateVertexBuffer hr=%08x", hr);
    if (hr || !vb) goto fail;
    hr = CALL(vb, VT_MobileVertexBuffer_Lock, PFLock)(vb, 0, 0, (void **)&v, 0);
    logw(L"Lock hr=%08x", hr);
    if (hr) goto fail;
    for (i = 0; i < 36; i++) {
        int face = i / 6, k;
        for (k = 0; k < 3; k++) { v[i * 6 + k] = 0.5f * f[face][tri[i % 6]][k]; v[i * 6 + 3 + k] = (float)nrm[face][k]; }
    }
    CALL(vb, VT_MobileVertexBuffer_Unlock, PFUnlock)(vb);
    CALL(dev, VT_MobileDevice_SetStreamSource, PFSetStreamSource)(dev, 0, vb, 24);

    vp.X = 0; vp.Y = 0; vp.Width = rc.right; vp.Height = rc.bottom; vp.MinZ = 0; vp.MaxZ = 1;
    CALL(dev, VT_MobileDevice_SetViewport, PFSetViewport)(dev, &vp);
    {
        float aspect = (float)rc.right / (float)rc.bottom, zn = 0.5f, h = zn * 1.2f;
        mat_persp(proj, h * aspect, h, zn, 60.0f);
    }
    CALL(dev, VT_MobileDevice_SetTransform, PFSetTransform)(dev, TS_PROJECTION, proj, FMT_FLOAT);

    memset(&lt, 0, sizeof(lt));
    lt.Type = LIGHT_DIRECTIONAL;
    lt.Diffuse.r = 0.9f; lt.Diffuse.g = 0.85f; lt.Diffuse.b = 0.8f; lt.Diffuse.a = 1;
    lt.Direction[0] = 0.4f; lt.Direction[1] = -1.0f; lt.Direction[2] = 0.6f;
    lt.Range = 1000.0f;
    hr = CALL(dev, VT_MobileDevice_SetLight, PFSetLight)(dev, 0, &lt, FMT_FLOAT);
    logw(L"SetLight hr=%08x", hr);
    CALL(dev, VT_MobileDevice_LightEnable, PFLightEnable)(dev, 0, TRUE);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_LIGHTING, TRUE);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_AMBIENT, 0xff59595f);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_ZENABLE, 1);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_CULLMODE, CULL_NONE);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_FOGENABLE, TRUE);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_FOGCOLOR, 0xff8cb3e6);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_FOGVERTEXMODE, FOG_LINEAR);
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_FOGSTART, fbits(15.0f));
    CALL(dev, VT_MobileDevice_SetRenderState, PFSetRenderState)(dev, RS_FOGEND, fbits(45.0f));
    logw(L"setup done, entering game loop");

    memset(&mat, 0, sizeof(mat));
    for (i = 0; i < NOBS; i++) respawn(i, 10.0f + i * 2.0f);
    t0 = tprev = GetTickCount();
    while (!quit) {
        float dt;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        tnow = GetTickCount(); dt = (tnow - tprev) / 1000.0f; if (dt > 0.1f) dt = 0.1f; tprev = tnow;
        if (!crashed) {
            float steer = (keys_right - keys_left) * 6.0f;
            px += steer * dt; if (px < -3) px = -3; if (px > 3) px = 3;
            roll += (-steer * 4.0f - roll) * 0.2f;
            speed += 0.35f * dt; dist += speed * dt;
            for (i = 0; i < NOBS; i++) {
                oz[i] -= speed * dt;
                if (oz[i] < -2.0f) respawn(i, 40.0f);
                if (oz[i] > -0.6f && oz[i] < 0.6f && ox[i] - px < 0.85f && px - ox[i] < 0.85f) crashed = 1;
            }
            if (crashed) quit = 1;
        }
        mat_rollz(view, roll);
        CALL(dev, VT_MobileDevice_SetTransform, PFSetTransform)(dev, TS_VIEW, view, FMT_FLOAT);
        CALL(dev, VT_MobileDevice_Clear, PFClear)(dev, 0, NULL, CLEAR_TARGET | CLEAR_ZBUFFER, 0xff8cb3e6, 1.0f, 0);
        CALL(dev, VT_MobileDevice_BeginScene, PFNoArg)(dev);

#define CUBE(sx, sy, sz, x, y, z, R, G, B) do { \
            mat.Diffuse.r = mat.Ambient.r = (R); mat.Diffuse.g = mat.Ambient.g = (G); \
            mat.Diffuse.b = mat.Ambient.b = (B); mat.Diffuse.a = mat.Ambient.a = 1.0f; \
            CALL(dev, VT_MobileDevice_SetMaterial, PFSetMaterial)(dev, &mat, FMT_FLOAT); \
            mat_world(world, sx, sy, sz, (x) - px, (y) - 1.2f, (z) + 3.0f); \
            CALL(dev, VT_MobileDevice_SetTransform, PFSetTransform)(dev, TS_WORLD, world, FMT_FLOAT); \
            hr = CALL(dev, VT_MobileDevice_DrawPrimitive, PFDrawPrimitive)(dev, PT_TRIANGLELIST, 0, 12); \
        } while (0)

        for (i = 0; i < 12; i++) {
            float z = -(dist - 4.0f * (float)(int)(dist / 4.0f)) + i * 4.0f;
            CUBE(8.0f, 0.1f, 2.0f, px, -0.55f, z, 0.25f, 0.55f, 0.25f);
            CUBE(8.0f, 0.1f, 2.0f, px, -0.55f, z + 2.0f, 0.3f, 0.65f, 0.3f);
        }
        for (i = 0; i < NOBS; i++)
            CUBE(1.0f, 1.0f, 1.0f, ox[i], 0.0f, oz[i], 0.9f, 0.3f + 0.6f * oc[i], 0.2f + 0.3f * oc[i]);
        CUBE(0.8f, 0.25f, 1.0f, px, -0.2f, 0.0f, 0.2f, 0.4f, 1.0f);
        if (frames == 0) logw(L"first frame DrawPrimitive hr=%08x", hr);

        CALL(dev, VT_MobileDevice_EndScene, PFNoArg)(dev);
        hr = CALL(dev, VT_MobileDevice_Present, PFPresent)(dev, NULL, NULL, NULL, NULL);
        if (frames == 0) logw(L"first Present hr=%08x", hr);
        frames++;
    }
    tnow = GetTickCount();
    wsprintfW(res, L"%s\nScore: %d\nFrames: %d in %u ms\nAverage: %d fps\nDriver: %s",
              crashed ? L"Crashed!" : L"Quit.", (int)dist, frames, tnow - t0,
              frames * 1000 / (int)(tnow - t0 + 1), ident[0] ? ident : L"(no name)");
    logw(L"%s", res);
    CALL(vb, VT_MobileVertexBuffer_Release, PFRelease)(vb);
    CALL(dev, VT_MobileDevice_Release, PFRelease)(dev);
    CALL(d3d, VT_Mobile_Release, PFRelease)(d3d);
    DestroyWindow(wnd);
    MessageBoxW(NULL, res, L"d3dmrun", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
    return 0;

fail:
    logw(L"=== FAILED, see lines above");
    MessageBoxW(NULL, L"D3DM setup failed. See \\d3dmrun.log", L"d3dmrun", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
    return 0;
}
