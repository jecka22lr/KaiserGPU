#include <windows.h>
#include <string.h>

static char out[2048]; static int on;
static void put(const char *s) { while (*s && on < 2040) out[on++] = *s++; out[on] = 0; }
static void hex(unsigned v) { char b[9]; int k; for (k = 0; k < 8; k++) b[k] = "0123456789abcdef"[(v >> (28 - 4 * k)) & 15]; b[8] = 0; put(b); }
static void dec(unsigned v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); put(b + i); }

static void fileinfo(const WCHAR *path, const char *label)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    unsigned char b[0x100]; DWORD r = 0, sz;
    put(label); put(": ");
    if (h == INVALID_HANDLE_VALUE) { put("cannot open, err "); dec(GetLastError()); put("\r\n"); return; }
    sz = GetFileSize(h, NULL);
    ReadFile(h, b, sizeof(b), &r, NULL);
    CloseHandle(h);
    put("size "); dec(sz);
    if (r >= 0x40) {
        unsigned pe = b[0x3c] | (b[0x3d] << 8);
        put(" MZ="); put(b[0] == 'M' && b[1] == 'Z' ? "y" : "n");
        if (pe + 6 < r) { put(" PE="); put(b[pe] == 'P' && b[pe + 1] == 'E' ? "y" : "n"); put(" mach="); hex(b[pe + 4] | (b[pe + 5] << 8)); }
    }
    put("\r\n");
}

static WCHAR g_dir[MAX_PATH]; static int g_dirlen;
static void tryload(const WCHAR *name, const char *label)
{
    HMODULE m; WCHAR full[MAX_PATH]; int i = 0;
    if (name[0] != L'@') { memcpy(full, g_dir, g_dirlen * 2); while (name[i]) { full[g_dirlen + i] = name[i]; i++; } full[g_dirlen + i] = 0; name = full; } else name++;
    SetLastError(0);
    m = LoadLibraryW(name);
    put(label); put(m ? "  OK" : "  FAIL ");
    if (!m) dec(GetLastError());
    else FreeLibrary(m);
    put("\r\n");
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cmd, int show)
{
    HANDLE h; DWORD w;
    put("dlltest 2\r\n");
    { int n = GetModuleFileNameW(NULL, g_dir, MAX_PATH); while (n > 0 && g_dir[n-1] != L'\\') n--; g_dir[n] = 0; g_dirlen = n; { char nb[MAX_PATH]; int k; for (k = 0; k <= n; k++) nb[k] = (char)g_dir[k]; put("dir: "); put(nb); put("\r\n"); } }
    fileinfo(L"\\Windows\\libgles_cm.dll", "Windows\\libgles_cm.dll");
    fileinfo(L"\\Windows\\d3dm_ati.dll", "Windows\\d3dm_ati.dll");
    tryload(L"rw_f.dll", "f known-good shim");
    tryload(L"rw_e.dll", "e HTC original    ");
    tryload(L"rw_a.dll", "a rotwrap1 as-is  ");
    tryload(L"rw_b.dll", "b stripped        ");
    tryload(L"rw_c.dll", "c stripped+crt    ");
    tryload(L"rw_d.dll", "d no forwarders   ");
    tryload(L"rw_g.dll", "g rotwrap1 mach 1c2");
    tryload(L"rw_h.dll", "h crt + mach 1c2  ");
    tryload(L"@libgles_cm.dll", "installed libgles_cm");
    h = CreateFileW(L"\\dlltest.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, out, on, &w, NULL); CloseHandle(h); }
    {
        WCHAR wb[2048]; int i;
        for (i = 0; i <= on; i++) wb[i] = (unsigned char)out[i];
        MessageBoxW(NULL, wb, L"dlltest", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
    }
    return 0;
}
