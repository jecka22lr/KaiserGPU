/*
 * cuberun.exe - tiny OpenGL ES 1.x game for the KaiserGPU project.
 *
 * Portrait only (the 2008 driver can't do landscape). D-pad left/right to
 * dodge, D-pad centre / Enter or the back key to quit. The world speeds up
 * over time. At the end a message box shows score and average FPS, and the
 * same goes to \cuberun.log.
 *
 * Exercises: depth buffer, per-vertex lighting, fog, many draw calls per frame.
 */
#include <windows.h>

typedef int EGLint; typedef unsigned EGLBoolean;
typedef void *EGLDisplay, *EGLConfig, *EGLSurface, *EGLContext;
typedef unsigned GLenum, GLbitfield; typedef float GLfloat; typedef int GLint, GLsizei;

#define EGL_NONE 0x3038
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_DEPTH_SIZE 0x3025
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 4

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_TRIANGLES 4
#define GL_FLOAT 0x1406
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_LIGHTING 0x0B50
#define GL_LIGHT0 0x4000
#define GL_POSITION 0x1203
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_COLOR_MATERIAL 0x0B57
#define GL_FOG 0x0B60
#define GL_FOG_MODE 0x0B65
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_COLOR 0x0B66
#define GL_LINEAR 0x2601

/* every GL/EGL function we use, resolved at runtime from libgles_cm.dll */
#define FUNCS \
 F(void*, eglGetDisplay, (void*)) \
 F(EGLBoolean, eglInitialize, (EGLDisplay, EGLint*, EGLint*)) \
 F(EGLBoolean, eglChooseConfig, (EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*)) \
 F(EGLSurface, eglCreateWindowSurface, (EGLDisplay, EGLConfig, HWND, const EGLint*)) \
 F(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint*)) \
 F(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext)) \
 F(EGLBoolean, eglSwapBuffers, (EGLDisplay, EGLSurface)) \
 F(EGLint, eglGetError, (void)) \
 F(EGLBoolean, eglTerminate, (EGLDisplay)) \
 F(EGLBoolean, eglDestroyContext, (EGLDisplay, EGLContext)) \
 F(EGLBoolean, eglDestroySurface, (EGLDisplay, EGLSurface)) \
 F(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
 F(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
 F(void, glClear, (GLbitfield)) \
 F(void, glMatrixMode, (GLenum)) \
 F(void, glLoadIdentity, (void)) \
 F(void, glFrustumf, (GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat)) \
 F(void, glTranslatef, (GLfloat, GLfloat, GLfloat)) \
 F(void, glRotatef, (GLfloat, GLfloat, GLfloat, GLfloat)) \
 F(void, glScalef, (GLfloat, GLfloat, GLfloat)) \
 F(void, glPushMatrix, (void)) \
 F(void, glPopMatrix, (void)) \
 F(void, glEnable, (GLenum)) \
 F(void, glDisable, (GLenum)) \
 F(void, glEnableClientState, (GLenum)) \
 F(void, glVertexPointer, (GLint, GLenum, GLsizei, const void*)) \
 F(void, glNormalPointer, (GLenum, GLsizei, const void*)) \
 F(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
 F(void, glColor4f, (GLfloat, GLfloat, GLfloat, GLfloat)) \
 F(void, glLightfv, (GLenum, GLenum, const GLfloat*)) \
 F(void, glFogf, (GLenum, GLfloat)) \
 F(void, glFogfv, (GLenum, const GLfloat*))

#define F(ret, name, args) typedef ret (*PF_##name) args; static PF_##name name;
FUNCS
#undef F

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

/* unit cube centred on the origin: 36 vertices + normals */
static GLfloat cube_v[36 * 3], cube_n[36 * 3];
static void build_cube(void)
{
    static const int f[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},
        {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}},
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}},
        {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}},
        {{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1}},
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}} };
    static const int n[6][3] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    static const int tri[6] = {0, 1, 2, 0, 2, 3};
    int i, j, k, p = 0;
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++, p++)
            for (k = 0; k < 3; k++) {
                cube_v[p * 3 + k] = 0.5f * f[i][tri[j]][k];
                cube_n[p * 3 + k] = (GLfloat)n[i][k];
            }
}

#define NOBS 24
static float ox[NOBS], oz[NOBS], oc[NOBS];
static unsigned rng = 12345;
static float frand(void) { rng = rng * 1103515245u + 12345u; return ((rng >> 8) & 0xffff) / 65535.0f; }
static void respawn(int i, float zmin) { ox[i] = frand() * 6.0f - 3.0f; oz[i] = zmin - frand() * 30.0f; oc[i] = frand(); }

static void draw_cube(float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    glColor4f(r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPopMatrix();
}

static void writelog(const WCHAR *s)
{
    HANDLE h = CreateFileW(L"\\cuberun.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    char a[512]; DWORD w; int n;
    if (h == INVALID_HANDLE_VALUE) return;
    n = WideCharToMultiByte(CP_ACP, 0, s, -1, a, sizeof(a), NULL, NULL);
    WriteFile(h, a, n > 0 ? n - 1 : 0, &w, NULL);
    CloseHandle(h);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    HMODULE gl;
    WNDCLASSW wc;
    HWND wnd;
    RECT rc;
    MSG msg;
    EGLDisplay dpy; EGLConfig cfg; EGLSurface surf; EGLContext ctx;
    EGLint n = 0;
    static const EGLint attrs[] = { EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5,
                                    EGL_DEPTH_SIZE, 16, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    static const GLfloat lpos[] = { -0.4f, 1.0f, 0.6f, 0.0f };
    static const GLfloat lamb[] = { 0.35f, 0.35f, 0.4f, 1.0f };
    static const GLfloat ldif[] = { 0.9f, 0.85f, 0.8f, 1.0f };
    static const GLfloat fogc[] = { 0.55f, 0.7f, 0.9f, 1.0f };
    float px = 0, speed = 8.0f, dist = 0, roll = 0;
    DWORD t0, tprev, tnow;
    int frames = 0, i, crashed = 0;
    WCHAR res[256];

    gl = LoadLibraryW(L"libgles_cm.dll");
    if (!gl) { MessageBoxW(NULL, L"libgles_cm.dll not found (install the KaiserGPU CAB).", L"cuberun", MB_OK); return 0; }
#define F(ret, name, args) name = (PF_##name)GetProcAddressW(gl, L## #name); \
    if (!name) { MessageBoxW(NULL, L"missing GL function: " L## #name, L"cuberun", MB_OK); return 0; }
    FUNCS
#undef F

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc; wc.hInstance = inst; wc.lpszClassName = L"cuberun";
    RegisterClassW(&wc);
    wnd = CreateWindowW(L"cuberun", L"cuberun", WS_VISIBLE, 0, 0,
                        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, inst, NULL);
    SetForegroundWindow(wnd);
    GetClientRect(wnd, &rc);

    dpy = eglGetDisplay(0);
    if (!eglInitialize(dpy, NULL, NULL) || !eglChooseConfig(dpy, attrs, &cfg, 1, &n) || n < 1 ||
        !(surf = eglCreateWindowSurface(dpy, cfg, wnd, NULL)) ||
        !(ctx = eglCreateContext(dpy, cfg, 0, NULL)) || !eglMakeCurrent(dpy, surf, surf, ctx)) {
        wsprintfW(res, L"EGL setup failed, error %04x", eglGetError());
        MessageBoxW(NULL, res, L"cuberun", MB_OK);
        return 0;
    }

    build_cube();
    glViewport(0, 0, rc.right, rc.bottom);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    {
        float aspect = (float)rc.right / (float)rc.bottom, nearz = 0.5f, w = nearz * 0.6f;
        glFrustumf(-w * aspect, w * aspect, -w, w, nearz, 60.0f);
    }
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);
    glEnable(GL_FOG);
    glFogf(GL_FOG_MODE, (GLfloat)GL_LINEAR);
    glFogf(GL_FOG_START, 15.0f);
    glFogf(GL_FOG_END, 45.0f);
    glFogfv(GL_FOG_COLOR, fogc);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cube_v);
    glNormalPointer(GL_FLOAT, 0, cube_n);

    for (i = 0; i < NOBS; i++) respawn(i, -10.0f - i * 2.0f);

    t0 = tprev = GetTickCount();
    while (!quit) {
        float dt;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        tnow = GetTickCount();
        dt = (tnow - tprev) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        tprev = tnow;

        if (!crashed) {
            float steer = (keys_right - keys_left) * 6.0f;
            px += steer * dt;
            if (px < -3.0f) px = -3.0f;
            if (px > 3.0f) px = 3.0f;
            roll += (-steer * 4.0f - roll) * 0.2f;
            speed += 0.35f * dt;
            dist += speed * dt;
            for (i = 0; i < NOBS; i++) {
                oz[i] += speed * dt;
                if (oz[i] > 2.0f) respawn(i, -40.0f);
                if (oz[i] > -0.6f && oz[i] < 0.6f && (ox[i] - px < 0.85f && px - ox[i] < 0.85f)) crashed = 1;
            }
            if (crashed) quit = 1;
        }

        glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glRotatef(roll, 0, 0, 1);
        glTranslatef(-px, -1.2f, -3.0f);

        /* ground stripes give a sense of speed */
        for (i = 0; i < 12; i++) {
            float z = (dist - 4.0f * (float)(int)(dist / 4.0f)) - i * 4.0f;
            draw_cube(0, -0.55f, z, 8.0f, 0.1f, 2.0f, 0.25f, 0.55f, 0.25f);
            draw_cube(0, -0.55f, z - 2.0f, 8.0f, 0.1f, 2.0f, 0.3f, 0.65f, 0.3f);
        }
        /* obstacles */
        for (i = 0; i < NOBS; i++)
            draw_cube(ox[i], 0.0f, oz[i], 1.0f, 1.0f, 1.0f, 0.9f, 0.3f + 0.6f * oc[i], 0.2f + 0.3f * oc[i]);
        /* the player */
        glPushMatrix();
        glTranslatef(px, -0.2f, 0.0f);
        glRotatef(-roll * 2.0f, 0, 0, 1);
        draw_cube(0, 0, 0, 0.8f, 0.25f, 1.0f, 0.2f, 0.4f, 1.0f);
        glPopMatrix();

        eglSwapBuffers(dpy, surf);
        frames++;
    }

    tnow = GetTickCount();
    wsprintfW(res, L"%s\nScore: %d\nFrames: %d in %u ms\nAverage: %d fps",
              crashed ? L"Crashed!" : L"Quit.", (int)dist, frames, tnow - t0,
              frames * 1000 / (int)(tnow - t0 + 1));
    writelog(res);
    eglMakeCurrent(dpy, 0, 0, 0);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surf);
    eglTerminate(dpy);
    DestroyWindow(wnd);
    MessageBoxW(NULL, res, L"cuberun", MB_OK);
    return 0;
}
