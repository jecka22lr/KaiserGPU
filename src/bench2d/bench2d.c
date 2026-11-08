/* bench2d - KaiserGPU 2D (GDI) benchmark. Writes \bench2d.log */
#include <windows.h>

static char out[4096]; static int on;
static void put(const char *s) { while (*s && on < 4090) out[on++] = *s++; out[on] = 0; }
static void dec(unsigned v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); put(b + i); }

static HWND wnd; static int W, H;

static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_PAINT) { PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void pump(void) { MSG msg; while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }

typedef void (*TESTFN)(HDC, int);
static HDC memdc, dibdc; static HBITMAP membmp, dibbmp, oldm, oldd; static void *dibbits;

static void t_fill(HDC dc, int i)
{
    RECT r = { 0, 0, W, H };
    HBRUSH b = CreateSolidBrush(RGB(i * 37 & 255, i * 91 & 255, i * 13 & 255));
    FillRect(dc, &r, b); DeleteObject(b);
}
static void t_fill_small(HDC dc, int i)
{
    int k;
    HBRUSH b = CreateSolidBrush(RGB(i * 37 & 255, 80, 160));
    for (k = 0; k < 50; k++) { RECT r; r.left = (k * 37 + i) % (W - 32); r.top = (k * 53 + i * 3) % (H - 32); r.right = r.left + 32; r.bottom = r.top + 32; FillRect(dc, &r, b); }
    DeleteObject(b);
}
static void t_scroll(HDC dc, int i) { BitBlt(dc, 0, 0, W, H - 4, dc, 0, 4, SRCCOPY); }
static void t_blt_compat(HDC dc, int i) { BitBlt(dc, 0, 0, W, H, memdc, 0, 0, SRCCOPY); }
static void t_blt_dib(HDC dc, int i) { BitBlt(dc, 0, 0, W, H, dibdc, 0, 0, SRCCOPY); }
static void t_stretch(HDC dc, int i) { StretchBlt(dc, 0, 0, W, H, dibdc, 0, 0, W / 2, H / 2, SRCCOPY); }
static void t_text(HDC dc, int i)
{
    int y; RECT r = { 0, 0, W, H };
    SetBkMode(dc, OPAQUE);
    for (y = 0; y < H; y += 16) ExtTextOutW(dc, 2, y, ETO_OPAQUE, NULL, L"The quick brown fox jumps over the lazy dog 0123", 48, NULL);
}
static void t_patblt(HDC dc, int i)
{
    HBRUSH b = CreateSolidBrush(RGB(255, i & 255, 0)), o = (HBRUSH)SelectObject(dc, b);
    PatBlt(dc, 0, 0, W, H, PATINVERT);
    SelectObject(dc, o); DeleteObject(b);
}
static void t_lines(HDC dc, int i)
{
    int k; POINT p[2];
    for (k = 0; k < 100; k++) { p[0].x = (k * 7 + i) % W; p[0].y = 0; p[1].x = W - 1 - p[0].x; p[1].y = H - 1; Polyline(dc, p, 2); }
}
static void t_text1(HDC dc, int i) { ExtTextOutW(dc, 2, (i * 16) % (H - 16), ETO_OPAQUE, NULL, L"The quick brown fox jumps over the lazy dog 0123", 48, NULL); }
static void t_line1(HDC dc, int i) { POINT p[2]; p[0].x = (i * 7) % W; p[0].y = 0; p[1].x = W - 1 - p[0].x; p[1].y = H - 1; Polyline(dc, p, 2); }
static void t_hline1(HDC dc, int i) { POINT p[2]; p[0].x = 0; p[0].y = i % H; p[1].x = W - 1; p[1].y = i % H; Polyline(dc, p, 2); }
static void t_rect1(HDC dc, int i) { RECT r; HBRUSH b = (HBRUSH)GetStockObject(GRAY_BRUSH); r.left = (i * 37) % (W - 32); r.top = (i * 53) % (H - 32); r.right = r.left + 32; r.bottom = r.top + 32; FillRect(dc, &r, b); }
static void t_pixel1(HDC dc, int i) { SetPixel(dc, i % W, (i / W) % H, RGB(255, 0, 0)); }
static void t_dibwrite(HDC dc, int i)    /* CPU writes pixels, then blit (game-style) */
{
    unsigned short *p = (unsigned short *)dibbits; int k, n = W * H;
    unsigned short c = (unsigned short)(i * 0x0841);
    for (k = 0; k < n; k++) p[k] = c ^ (unsigned short)k;
    BitBlt(dc, 0, 0, W, H, dibdc, 0, 0, SRCCOPY);
}

static void run_dc(const char *name, TESTFN f, int ms, HDC dc)
{
    DWORD t0 = GetTickCount(), t; int n = 0;
    do { f(dc, n); n++; t = GetTickCount() - t0; } while (t < (DWORD)ms);
    put(name); put(": "); dec(n); put(" in "); dec(t); put(" ms = "); dec(n * 1000 / (t ? t : 1)); put("/s\r\n");
    pump();
}
/* time single calls of f, histogram in 1 ms buckets */
static void hist(const char *name, TESTFN f, int count, HDC dc)
{
    LARGE_INTEGER fq, a, b; int k, h[8] = {0}; unsigned tot = 0, mx = 0;
    QueryPerformanceFrequency(&fq);
    for (k = 0; k < count; k++) {
        unsigned us;
        QueryPerformanceCounter(&a); f(dc, k); QueryPerformanceCounter(&b);
        us = (unsigned)((b.QuadPart - a.QuadPart) * 1000000 / fq.QuadPart);
        tot += us; if (us > mx) mx = us;
        h[us < 500 ? 0 : us < 2000 ? 1 : us < 5000 ? 2 : us < 10000 ? 3 : us < 15000 ? 4 : us < 20000 ? 5 : us < 35000 ? 6 : 7]++;
    }
    put(name); put(": avg "); dec(tot / count); put("us max "); dec(mx); put("us  [<.5 <2 <5 <10 <15 <20 <35 >35ms] ");
    for (k = 0; k < 8; k++) { dec(h[k]); put(k < 7 ? " " : "\r\n"); }
    pump();
}
static void run(const char *name, TESTFN f, int ms)
{
    HDC dc = GetDC(wnd); DWORD t0 = GetTickCount(), t; int n = 0;
    do { f(dc, n); n++; t = GetTickCount() - t0; } while (t < (DWORD)ms);

    t = GetTickCount() - t0;
    ReleaseDC(wnd, dc);
    put(name); put(": "); dec(n); put(" in "); dec(t); put(" ms = "); dec(n * 1000 / (t ? t : 1)); put("/s\r\n");
    pump();
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    WNDCLASSW wc; HDC dc; HANDLE h; DWORD w;
    struct { BITMAPINFOHEADER h; DWORD m[3]; } bi;

    W = GetSystemMetrics(SM_CXSCREEN); H = GetSystemMetrics(SM_CYSCREEN);
    memset(&wc, 0, sizeof(wc)); wc.lpfnWndProc = wp; wc.hInstance = inst; wc.lpszClassName = L"bench2d";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    wnd = CreateWindowW(L"bench2d", L"bench2d", WS_POPUP | WS_VISIBLE, 0, 0, W, H, NULL, NULL, inst, NULL);
    SetForegroundWindow(wnd); UpdateWindow(wnd); pump();

    dc = GetDC(wnd);
    put("bench2d 2, screen "); dec(W); put("x"); dec(H); put(" bpp "); dec(GetDeviceCaps(dc, BITSPIXEL)); put("\r\n");
    memdc = CreateCompatibleDC(dc); membmp = CreateCompatibleBitmap(dc, W, H); oldm = (HBITMAP)SelectObject(memdc, membmp);
    PatBlt(memdc, 0, 0, W, H, WHITENESS);
    memset(&bi, 0, sizeof(bi));
    bi.h.biSize = sizeof(bi.h); bi.h.biWidth = W; bi.h.biHeight = -H; bi.h.biPlanes = 1; bi.h.biBitCount = 16; bi.h.biCompression = BI_BITFIELDS;
    bi.m[0] = 0xF800; bi.m[1] = 0x07E0; bi.m[2] = 0x001F;
    dibdc = CreateCompatibleDC(dc); dibbmp = CreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &dibbits, NULL, 0);
    oldd = (HBITMAP)SelectObject(dibdc, dibbmp);
    { int k; unsigned short *p = (unsigned short *)dibbits; for (k = 0; k < W * H; k++) p[k] = (unsigned short)(k * 7); }
    ReleaseDC(wnd, dc);

    put("-- on screen\r\n");
    { HDC dc = GetDC(wnd);
      hist("text line  ", t_text1, 60, dc);
      hist("diag line  ", t_line1, 30, dc);
      hist("horiz line ", t_hline1, 60, dc);
      hist("32x32 fill ", t_rect1, 200, dc);
      hist("setpixel   ", t_pixel1, 200, dc);
      ReleaseDC(wnd, dc); }
    put("-- offscreen bitmap\r\n");
    hist("text line  ", t_text1, 60, memdc);
    hist("diag line  ", t_line1, 30, memdc);
    hist("horiz line ", t_hline1, 60, memdc);
    hist("32x32 fill ", t_rect1, 200, memdc);
    hist("setpixel   ", t_pixel1, 200, memdc);
    put("-- offscreen dib\r\n");
    hist("text line  ", t_text1, 60, dibdc);
    hist("diag line  ", t_line1, 30, dibdc);
    hist("32x32 fill ", t_rect1, 200, dibdc);
    put("-- throughput\r\n");
    run("text 20 lines screen   ", t_text, 1500);
    run_dc("text 20 lines offscreen", t_text, 1500, memdc);
    run("lines x100 screen      ", t_lines, 1500);
    run_dc("lines x100 offscreen   ", t_lines, 1500, memdc);
    run("blt compat bmp->screen", t_blt_compat, 1500);
    SelectObject(memdc, oldm); DeleteObject(membmp); DeleteDC(memdc);
    SelectObject(dibdc, oldd); DeleteObject(dibbmp); DeleteDC(dibdc);

    h = CreateFileW(L"\\bench2d.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, out, on, &w, NULL); CloseHandle(h); }
    DestroyWindow(wnd);
    {
        WCHAR wb[4096]; int i;
        for (i = 0; i <= on; i++) wb[i] = (unsigned char)out[i];
        MessageBoxW(NULL, wb, L"bench2d", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
    }
    return 0;
}
