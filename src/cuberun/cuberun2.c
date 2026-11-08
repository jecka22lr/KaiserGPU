/*
 * cuberun 2 - OpenGL ES 1.x test game for the KaiserGPU project.
 *
 * Works in portrait AND landscape. The 2008 driver ignores screen rotation and
 * always presents into the physical 240x320 portrait framebuffer, so in
 * landscape we ask for a 240x320 window at (0,0) and rotate our own projection
 * by 90 degrees. If the picture comes out upside down, press F to flip it
 * (the choice is remembered).
 *
 * Controls: left/right steer, up / centre / space jump, Q or Esc quit, F flip.
 * Dodge cubes, jump over them, grab the spinning gold gems.
 * 3 lives, every 10 gems = extra life. High score is saved.
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
 F(void, glOrthof, (GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat)) \
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

/* ---------------------------------------------------------------- input -- */
static int k_left, k_right, k_jump, quit, flip_req;
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN || m == WM_KEYUP) {
        int d = (m == WM_KEYDOWN);
        if (w == VK_LEFT) k_left = d;
        else if (w == VK_RIGHT) k_right = d;
        else if (w == VK_UP || w == VK_RETURN || w == VK_SPACE) k_jump = d;
        else if (d && (w == VK_ESCAPE || w == VK_BACK || w == 'Q')) quit = 1;
        else if (d && w == 'F') flip_req = 1;
        return 0;
    }
    if (m == WM_CLOSE) { quit = 1; return 0; }
    return DefWindowProcW(h, m, w, l);
}

/* ---------------------------------------------------------------- mesh --- */
static GLfloat cube_v[36 * 3], cube_n[36 * 3];
static void build_cube(void)
{
    static const int f[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}}, {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}},
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}}, {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}},
        {{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1}}, {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}} };
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
static void cube(float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
    glPushMatrix(); glTranslatef(x, y, z); glScalef(sx, sy, sz);
    glColor4f(r, g, b, 1.0f); glDrawArrays(GL_TRIANGLES, 0, 36); glPopMatrix();
}

/* ---------------------------------------------------------------- utils -- */
static unsigned rng = 1;
static float frand(void) { rng = rng * 1103515245u + 12345u; return ((rng >> 8) & 0xffff) / 65535.0f; }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

/* 7-segment digits for the HUD, drawn with flat cubes in a 2D ortho view */
static void digit(int d, float x, float y, float s)
{
    static const unsigned char seg[10] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    /* segments a..g: top, top-right, bottom-right, bottom, bottom-left, top-left, middle */
    static const float sx[7] = {0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.5f};
    static const float sy[7] = {2.0f, 1.5f, 0.5f, 0.0f, 0.5f, 1.5f, 1.0f};
    static const int horiz[7] = {1, 0, 0, 1, 0, 0, 1};
    int i;
    for (i = 0; i < 7; i++)
        if (seg[d] & (1 << i))
            cube(x + sx[i] * s, y + sy[i] * s, 0, horiz[i] ? s : s * 0.22f, horiz[i] ? s * 0.22f : s, 1, 1, 1, 1);
}
static void number(int v, float x, float y, float s)
{
    char buf[12]; int n = 0, i;
    if (v < 0) v = 0;
    do { buf[n++] = (char)(v % 10); v /= 10; } while (v && n < 10);
    for (i = n - 1; i >= 0; i--) { digit(buf[i], x, y, s); x += s * 1.6f; }
}

/* ------------------------------------------------------------ game state -- */
#define NOBS 22
#define NGEM 6
#define NPART 48
static float ox[NOBS], oz[NOBS], oh[NOBS], oc[NOBS];
static float gx[NGEM], gz[NGEM]; static int galive[NGEM];
static float ptx[NPART], pty[NPART], ptz[NPART], pvx[NPART], pvy[NPART], pvz[NPART];

static void respawn_obs(int i, float zfar)
{
    ox[i] = frand() * 6.0f - 3.0f;
    oz[i] = zfar - frand() * 30.0f;
    oh[i] = frand() < 0.25f ? 2.0f : 1.0f;      /* some tall ones you can't jump */
    oc[i] = frand();
}
static void respawn_gem(int i, float zfar)
{
    gx[i] = frand() * 6.0f - 3.0f; gz[i] = zfar - frand() * 40.0f; galive[i] = 1;
}

static void load_save(int *hi, int *rot)
{
    HANDLE h = CreateFileW(L"\\cuberun2.sav", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD r; int v[2];
    if (h == INVALID_HANDLE_VALUE) return;
    if (ReadFile(h, v, sizeof(v), &r, NULL) && r == sizeof(v)) { *hi = v[0]; *rot = v[1]; }
    CloseHandle(h);
}
static void save(int hi, int rot)
{
    HANDLE h = CreateFileW(L"\\cuberun2.sav", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD w; int v[2]; v[0] = hi; v[1] = rot;
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, v, sizeof(v), &w, NULL); CloseHandle(h);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    HMODULE gl; WNDCLASSW wc; HWND wnd; MSG msg;
    EGLDisplay dpy; EGLConfig cfg; EGLSurface surf; EGLContext ctx; EGLint n = 0;
    static const EGLint attrs[] = { EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5,
                                    EGL_DEPTH_SIZE, 16, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    static const GLfloat lpos[] = { -0.4f, 1.0f, 0.6f, 0.0f };
    static const GLfloat lamb[] = { 0.35f, 0.35f, 0.4f, 1.0f };
    static const GLfloat ldif[] = { 0.9f, 0.85f, 0.8f, 1.0f };
    int landscape = GetSystemMetrics(SM_CXSCREEN) > GetSystemMetrics(SM_CYSCREEN);
    int hi = 0, rot = 90, frames, totalframes = 0, i, again = 1;
    DWORD tstart_all = GetTickCount();
    WCHAR res[300];

    gl = LoadLibraryW(L"libgles_cm.dll");
    if (!gl) { MessageBoxW(NULL, L"libgles_cm.dll not found (install the KaiserGPU CAB).", L"cuberun",
                           MB_OK | MB_SETFOREGROUND | MB_TOPMOST); return 0; }
#define F(ret, name, args) name = (PF_##name)GetProcAddressW(gl, L## #name); \
    if (!name) { MessageBoxW(NULL, L"missing GL function: " L## #name, L"cuberun", MB_OK | MB_TOPMOST); return 0; }
    FUNCS
#undef F
    load_save(&hi, &rot);
    rng = GetTickCount() | 1;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc; wc.hInstance = inst; wc.lpszClassName = L"cuberun2";
    RegisterClassW(&wc);
    /* always a physical-portrait-sized window at the origin */
    wnd = CreateWindowW(L"cuberun2", L"cuberun", WS_POPUP | WS_VISIBLE, 0, 0, 240, 320, NULL, NULL, inst, NULL);
    SetForegroundWindow(wnd);

    dpy = eglGetDisplay(0);
    if (!eglInitialize(dpy, NULL, NULL) || !eglChooseConfig(dpy, attrs, &cfg, 1, &n) || n < 1 ||
        !(surf = eglCreateWindowSurface(dpy, cfg, wnd, NULL)) ||
        !(ctx = eglCreateContext(dpy, cfg, 0, NULL)) || !eglMakeCurrent(dpy, surf, surf, ctx)) {
        wsprintfW(res, L"EGL setup failed, error %04x", eglGetError());
        MessageBoxW(NULL, res, L"cuberun", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
        return 0;
    }
    build_cube();
    glViewport(0, 0, 240, 320);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);
    glFogf(GL_FOG_MODE, (GLfloat)GL_LINEAR);
    glFogf(GL_FOG_START, 14.0f);
    glFogf(GL_FOG_END, 44.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cube_v);
    glNormalPointer(GL_FLOAT, 0, cube_n);

    while (again && !quit) {
        float px = 0, py = 0, vy = 0, speed = 8.0f, dist = 0, roll = 0, invuln = 0, dead_t = 0, spin = 0;
        int lives = 3, gems = 0, bonus = 0, dead = 0, score;
        DWORD t0, tprev, tnow;
        for (i = 0; i < NOBS; i++) respawn_obs(i, -14.0f - i * 2.0f);
        for (i = 0; i < NGEM; i++) respawn_gem(i, -20.0f - i * 7.0f);
        frames = 0;
        t0 = tprev = GetTickCount();

        while (!quit) {
            float dt, vw, vh, sky[4], t;
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (flip_req) { rot = -rot; flip_req = 0; }
            tnow = GetTickCount(); dt = (tnow - tprev) / 1000.0f; if (dt > 0.1f) dt = 0.1f; tprev = tnow;
            spin += 180.0f * dt;

            if (!dead) {
                float steer = (k_right - k_left) * 6.5f;
                px = clampf(px + steer * dt, -3.2f, 3.2f);
                roll += (-steer * 4.0f - roll) * 0.2f;
                if (k_jump && py <= 0.0f) vy = 7.0f;
                vy -= 20.0f * dt; py += vy * dt; if (py < 0) { py = 0; vy = 0; }
                speed += 0.3f * dt; dist += speed * dt;
                if (invuln > 0) invuln -= dt;
                for (i = 0; i < NOBS; i++) {
                    oz[i] += speed * dt;
                    if (oz[i] > 2.0f) respawn_obs(i, -42.0f);
                    if (invuln <= 0 && oz[i] > -0.6f && oz[i] < 0.6f &&
                        ox[i] - px < 0.85f && px - ox[i] < 0.85f && py < oh[i] - 0.1f) {
                        lives--; invuln = 1.5f; respawn_obs(i, -42.0f);
                        if (lives <= 0) {
                            dead = 1;
                            for (i = 0; i < NPART; i++) {
                                ptx[i] = px; pty[i] = py; ptz[i] = 0;
                                pvx[i] = frand() * 8 - 4; pvy[i] = frand() * 8 + 2; pvz[i] = frand() * 8 - 6;
                            }
                            break;
                        }
                    }
                }
                for (i = 0; i < NGEM; i++) {
                    gz[i] += speed * dt;
                    if (gz[i] > 2.0f) respawn_gem(i, -50.0f);
                    if (galive[i] && gz[i] > -0.7f && gz[i] < 0.7f && gx[i] - px < 0.9f && px - gx[i] < 0.9f && py < 1.6f) {
                        galive[i] = 0; gems++; bonus += 50;
                        if (gems % 10 == 0 && lives < 5) lives++;
                    }
                }
            } else {
                dead_t += dt;
                for (i = 0; i < NPART; i++) {
                    pvy[i] -= 15.0f * dt; ptx[i] += pvx[i] * dt; pty[i] += pvy[i] * dt; ptz[i] += pvz[i] * dt;
                }
                if (dead_t > 1.8f) break;
            }
            score = (int)dist + bonus;

            /* sky cycles day -> sunset -> night -> day as you go */
            t = dist / 300.0f; t -= (float)(int)t;
            if (t < 0.33f)      { float u = t / 0.33f;          sky[0] = lerp(0.55f, 0.95f, u); sky[1] = lerp(0.70f, 0.55f, u); sky[2] = lerp(0.90f, 0.35f, u); }
            else if (t < 0.66f) { float u = (t - 0.33f) / 0.33f; sky[0] = lerp(0.95f, 0.12f, u); sky[1] = lerp(0.55f, 0.10f, u); sky[2] = lerp(0.35f, 0.25f, u); }
            else                { float u = (t - 0.66f) / 0.34f; sky[0] = lerp(0.12f, 0.55f, u); sky[1] = lerp(0.10f, 0.70f, u); sky[2] = lerp(0.25f, 0.90f, u); }
            sky[3] = 1.0f;

            /* ---- 3D pass ---- */
            vw = landscape ? 320.0f : 240.0f; vh = landscape ? 240.0f : 320.0f;
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            if (landscape) glRotatef((float)rot, 0, 0, 1);
            { float nz = 0.5f, w = nz * 0.6f; glFrustumf(-w * vw / vh, w * vw / vh, -w, w, nz, 60.0f); }
            glMatrixMode(GL_MODELVIEW);
            glClearColor(sky[0], sky[1], sky[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, sky);
            glLoadIdentity();
            glLightfv(GL_LIGHT0, GL_POSITION, lpos);
            glRotatef(roll, 0, 0, 1);
            glTranslatef(-px * 0.85f, -1.3f - py * 0.3f, -3.2f);

            for (i = 0; i < 12; i++) {
                float z = (dist - 4.0f * (float)(int)(dist / 4.0f)) - i * 4.0f;
                cube(0, -0.55f, z, 9.0f, 0.1f, 2.0f, 0.22f, 0.5f, 0.25f);
                cube(0, -0.55f, z - 2.0f, 9.0f, 0.1f, 2.0f, 0.28f, 0.6f, 0.3f);
                cube(-4.6f, 0.0f, z, 0.3f, 1.0f, 0.3f, 0.6f, 0.6f, 0.65f);   /* side posts */
                cube( 4.6f, 0.0f, z, 0.3f, 1.0f, 0.3f, 0.6f, 0.6f, 0.65f);
            }
            for (i = 0; i < NOBS; i++)
                cube(ox[i], oh[i] * 0.5f - 0.5f, oz[i], 1.0f, oh[i], 1.0f,
                     oh[i] > 1.5f ? 0.75f : 0.9f, oh[i] > 1.5f ? 0.2f : 0.3f + 0.6f * oc[i], oh[i] > 1.5f ? 0.6f : 0.2f + 0.3f * oc[i]);
            for (i = 0; i < NGEM; i++)
                if (galive[i]) {
                    glPushMatrix(); glTranslatef(gx[i], 0.4f, gz[i]); glRotatef(spin, 0, 1, 0); glRotatef(45, 1, 0, 1);
                    cube(0, 0, 0, 0.45f, 0.45f, 0.45f, 1.0f, 0.85f, 0.15f); glPopMatrix();
                }
            if (!dead) {
                if (invuln <= 0 || ((int)(invuln * 10) & 1)) {
                    glPushMatrix(); glTranslatef(px, -0.2f + py, 0.0f); glRotatef(-roll * 2.0f, 0, 0, 1);
                    cube(0, 0, 0, 0.8f, 0.25f, 1.0f, 0.2f, 0.45f, 1.0f);
                    cube(0, 0.18f, 0.1f, 0.35f, 0.15f, 0.4f, 0.6f, 0.85f, 1.0f);
                    glPopMatrix();
                }
            } else {
                for (i = 0; i < NPART; i++)
                    cube(ptx[i], pty[i] - 0.2f, ptz[i], 0.18f, 0.18f, 0.18f, 0.3f + 0.7f * (i & 1), 0.4f, 1.0f - 0.5f * (i & 1));
            }

            /* ---- HUD pass ---- */
            glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            if (landscape) glRotatef((float)rot, 0, 0, 1);
            glOrthof(0, vw, 0, vh, -10, 10);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            number(score, 8, vh - 26, 9);
            for (i = 0; i < lives; i++) cube(vw - 14 - i * 16, vh - 14, 0, 11, 11, 1, 0.25f, 0.5f, 1.0f);
            for (i = 0; i < gems % 10; i++) cube(vw - 10 - i * 9, vh - 30, 0, 6, 6, 1, 1.0f, 0.85f, 0.15f);

            eglSwapBuffers(dpy, surf);
            frames++;
        }
        totalframes += frames;
        if (quit) break;

        score = (int)dist + bonus;
        if (score > hi) hi = score;
        save(hi, rot);
        tnow = GetTickCount();
        wsprintfW(res, L"Game over!\n\nScore: %d   (best: %d)\nGems: %d\nAverage: %d fps\n\nPlay again?",
                  score, hi, gems, frames * 1000 / (int)(tnow - t0 + 1));
        again = (MessageBoxW(wnd, res, L"cuberun", MB_YESNO | MB_SETFOREGROUND | MB_TOPMOST) == IDYES);
        SetForegroundWindow(wnd);
        k_left = k_right = k_jump = 0;
    }
    save(hi, rot);
    eglMakeCurrent(dpy, 0, 0, 0);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surf);
    eglTerminate(dpy);
    DestroyWindow(wnd);
    {
        HANDLE h = CreateFileW(L"\\cuberun.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        char a[200]; DWORD w; int len;
        wsprintfW(res, L"cuberun2 %s rot=%d best=%d frames=%d time=%ums",
                  landscape ? L"landscape" : L"portrait", rot, hi, totalframes, GetTickCount() - tstart_all);
        len = WideCharToMultiByte(CP_ACP, 0, res, -1, a, sizeof(a), NULL, NULL);
        if (h != INVALID_HANDLE_VALUE) { WriteFile(h, a, len > 0 ? len - 1 : 0, &w, NULL); CloseHandle(h); }
    }
    return 0;
}
