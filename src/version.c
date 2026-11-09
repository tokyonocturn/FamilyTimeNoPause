/*
 * FamilyTimeNoPause
 *
 * Drop-in `version.dll` for Family Time.
 *
 * The player (UnityPlayer.dll) imports VERSION.dll, so a DLL with that name placed
 * beside FamilyTime.exe is loaded by the game itself. That means no launcher, no
 * injector step, and no game file is modified: the proxy forwards the handful of
 * VERSION.dll exports to the real system DLL and, once the game window exists,
 * subclasses it and swallows the deactivation messages.
 *
 * Why that is the fix: the player stops simulating the moment it stops being the
 * foreground window, because Unity receives WM_ACTIVATEAPP(0) and
 * WM_ACTIVATE(WA_INACTIVE) and the game ships with runInBackground disabled.
 * Application.runInBackground is exposed but its setter does not stick in these
 * builds (it reads back False), so the reliable lever is to stop the engine from
 * ever learning it was deactivated. The replacement window procedure has to be a
 * valid address inside the player, which is why this lives in a DLL the player
 * loads rather than in an external tool.
 *
 * Nothing here touches game files, memory of other processes, or the network.
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#define EXPORT __declspec(dllexport)

#define PRODUCT_VERSION "1.0.0"

/* ------------------------------------------------------------------ logging */

static char g_log[MAX_PATH];

static void logline(const char *fmt, ...)
{
    FILE *f;
    va_list ap;

    if (g_log[0] == '\0') return;

    f = fopen(g_log, "a");
    if (!f) return;

    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void init_log(void)
{
    char dir[MAX_PATH];
    char folder[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH);

    /* Fall back to the game folder only if the profile variable is unavailable. */
    if (n == 0 || n >= MAX_PATH - 64) {
        if (GetModuleFileNameA(NULL, dir, MAX_PATH) == 0) return;
        {
            char *slash = strrchr(dir, '\\');
            if (slash) *slash = '\0';
        }
    }

    snprintf(folder, sizeof(folder), "%s\\FamilyTimeNoPause", dir);
    CreateDirectoryA(folder, NULL);
    snprintf(g_log, sizeof(g_log), "%s\\log.txt", folder);

    /* One header per game run, so the file describes the current session only. */
    {
        FILE *f = fopen(g_log, "w");
        if (f) {
            fprintf(f, "FamilyTimeNoPause %s\n", PRODUCT_VERSION);
            fclose(f);
        }
    }
}

/* ------------------------------------------------------- real VERSION.dll */

/*
 * The system copy is loaded by full path. A bare LoadLibraryA("version.dll")
 * would resolve to this file and recurse.
 */
static HMODULE g_real;

static void ensure_real(void)
{
    char path[MAX_PATH];
    UINT n;

    if (g_real) return;

    n = GetSystemDirectoryA(path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 16) return;

    lstrcatA(path, "\\version.dll");
    g_real = LoadLibraryExA(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_real) g_real = LoadLibraryA(path);
}

static FARPROC real(const char *name)
{
    ensure_real();
    return g_real ? GetProcAddress(g_real, name) : NULL;
}

/* ------------------------------------------------------------ focus shield */

static WNDPROC g_orig_proc;
static HWND    g_window;
static int     g_swallowed;

static LRESULT CALLBACK shield_wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ACTIVATEAPP && wp == 0) {
        if (g_swallowed < 8) {
            g_swallowed++;
            logline("swallowed WM_ACTIVATEAPP(deactivate) #%d", g_swallowed);
        }
        return 0;
    }
    if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) {
        if (g_swallowed < 8) {
            g_swallowed++;
            logline("swallowed WM_ACTIVATE(WA_INACTIVE) #%d", g_swallowed);
        }
        return 0;
    }
    if (msg == WM_KILLFOCUS) return 0;
    if (msg == WM_NCACTIVATE && wp == 0) return TRUE;

    return CallWindowProcA(g_orig_proc, h, msg, wp, lp);
}

static BOOL CALLBACK find_window_cb(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    RECT r;

    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;
    if (GetWindow(h, GW_OWNER) != NULL) return TRUE;
    if (!GetWindowRect(h, &r)) return TRUE;
    if ((r.right - r.left) < 200 || (r.bottom - r.top) < 200) return TRUE;

    *(HWND *)lp = h;
    return FALSE;
}

static HWND find_game_window(void)
{
    HWND h = NULL;
    EnumWindows(find_window_cb, (LPARAM)&h);
    return h;
}

static void install_shield(HWND h)
{
    g_orig_proc = (WNDPROC)(LONG_PTR)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)shield_wndproc);
    g_window = h;

    if (!g_orig_proc) {
        logline("subclass FAILED on hwnd %p (err %lu)", (void *)h, GetLastError());
        return;
    }

    logline("shield installed on hwnd %p (previous proc %p)", (void *)h, (void *)g_orig_proc);
}

static DWORD WINAPI shield_thread(LPVOID unused)
{
    HWND h;
    int waited = 0;

    (void)unused;
    init_log();
    logline("loaded into pid %lu; waiting for the game window", GetCurrentProcessId());

    /*
     * The DLL is loaded while the player is still starting, so the window may not
     * exist yet. Unity creates it within a few seconds; give it a generous window
     * and then give up quietly rather than spinning forever.
     */
    h = find_game_window();
    while (!h && waited < 240000) {
        Sleep(250);
        waited += 250;
        h = find_game_window();
    }

    if (!h) {
        logline("no game window found after %d ms; shield not installed", waited);
        return 1;
    }

    logline("game window found after %d ms", waited);
    install_shield(h);

    /* A resolution change or player restart can recreate the window. */
    for (;;) {
        Sleep(2000);
        if (!IsWindow(h)) {
            h = find_game_window();
            if (h) install_shield(h);
            continue;
        }
        if ((WNDPROC)(LONG_PTR)GetWindowLongPtrA(h, GWLP_WNDPROC) != shield_wndproc)
            install_shield(h);
    }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        CreateThread(NULL, 0, shield_thread, NULL, 0, NULL);
    }
    return TRUE;
}

/* ------------------------------------------------- forwarded VERSION exports */

EXPORT BOOL WINAPI GetFileVersionInfoA(LPCSTR a, DWORD b, DWORD c, LPVOID d)
{
    typedef BOOL (WINAPI *FN)(LPCSTR, DWORD, DWORD, LPVOID);
    FN f = (FN)real("GetFileVersionInfoA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return f(a, b, c, d);
}

EXPORT BOOL WINAPI GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d)
{
    typedef BOOL (WINAPI *FN)(LPCWSTR, DWORD, DWORD, LPVOID);
    FN f = (FN)real("GetFileVersionInfoW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return f(a, b, c, d);
}

EXPORT BOOL WINAPI GetFileVersionInfoExA(DWORD flags, LPCSTR a, DWORD b, DWORD c, LPVOID d)
{
    typedef BOOL (WINAPI *FN)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
    FN f = (FN)real("GetFileVersionInfoExA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return f(flags, a, b, c, d);
}

EXPORT BOOL WINAPI GetFileVersionInfoExW(DWORD flags, LPCWSTR a, DWORD b, DWORD c, LPVOID d)
{
    typedef BOOL (WINAPI *FN)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
    FN f = (FN)real("GetFileVersionInfoExW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return f(flags, a, b, c, d);
}

EXPORT DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR a, LPDWORD b)
{
    typedef DWORD (WINAPI *FN)(LPCSTR, LPDWORD);
    FN f = (FN)real("GetFileVersionInfoSizeA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(a, b);
}

EXPORT DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b)
{
    typedef DWORD (WINAPI *FN)(LPCWSTR, LPDWORD);
    FN f = (FN)real("GetFileVersionInfoSizeW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(a, b);
}

EXPORT DWORD WINAPI GetFileVersionInfoSizeExA(DWORD flags, LPCSTR a, LPDWORD b)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPCSTR, LPDWORD);
    FN f = (FN)real("GetFileVersionInfoSizeExA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(flags, a, b);
}

EXPORT DWORD WINAPI GetFileVersionInfoSizeExW(DWORD flags, LPCWSTR a, LPDWORD b)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPCWSTR, LPDWORD);
    FN f = (FN)real("GetFileVersionInfoSizeExW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(flags, a, b);
}

EXPORT DWORD WINAPI VerFindFileA(DWORD flags, LPSTR file, LPSTR winDir, LPSTR appDir,
                                 LPSTR curDir, PUINT curLen, LPSTR destDir, PUINT destLen)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPSTR, LPSTR, LPSTR, LPSTR, PUINT, LPSTR, PUINT);
    FN f = (FN)real("VerFindFileA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(flags, file, winDir, appDir, curDir, curLen, destDir, destLen);
}

EXPORT DWORD WINAPI VerFindFileW(DWORD flags, LPWSTR file, LPWSTR winDir, LPWSTR appDir,
                                 LPWSTR curDir, PUINT curLen, LPWSTR destDir, PUINT destLen)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPWSTR, LPWSTR, LPWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
    FN f = (FN)real("VerFindFileW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(flags, file, winDir, appDir, curDir, curLen, destDir, destLen);
}

EXPORT DWORD WINAPI VerInstallFileA(DWORD flags, LPSTR src, LPSTR dst, LPSTR srcDir,
                                    LPSTR dstDir, LPSTR curDir, LPSTR tmp, PUINT tmpLen)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, PUINT);
    FN f = (FN)real("VerInstallFileA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(flags, src, dst, srcDir, dstDir, curDir, tmp, tmpLen);
}

EXPORT DWORD WINAPI VerInstallFileW(DWORD flags, LPWSTR src, LPWSTR dst, LPWSTR srcDir,
                                    LPWSTR dstDir, LPWSTR curDir, LPWSTR tmp, PUINT tmpLen)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, PUINT);
    FN f = (FN)real("VerInstallFileW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return f(flags, src, dst, srcDir, dstDir, curDir, tmp, tmpLen);
}

/* winver.h declares these as returning DWORD with a DWORD size argument. */
EXPORT DWORD WINAPI VerLanguageNameA(DWORD lang, LPSTR buf, DWORD len)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPSTR, DWORD);
    FN f = (FN)real("VerLanguageNameA");
    if (!f) return 0;
    return f(lang, buf, len);
}

EXPORT DWORD WINAPI VerLanguageNameW(DWORD lang, LPWSTR buf, DWORD len)
{
    typedef DWORD (WINAPI *FN)(DWORD, LPWSTR, DWORD);
    FN f = (FN)real("VerLanguageNameW");
    if (!f) return 0;
    return f(lang, buf, len);
}

EXPORT BOOL WINAPI VerQueryValueA(LPCVOID block, LPCSTR sub, LPVOID *out, PUINT len)
{
    typedef BOOL (WINAPI *FN)(LPCVOID, LPCSTR, LPVOID *, PUINT);
    FN f = (FN)real("VerQueryValueA");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return f(block, sub, out, len);
}

EXPORT BOOL WINAPI VerQueryValueW(LPCVOID block, LPCWSTR sub, LPVOID *out, PUINT len)
{
    typedef BOOL (WINAPI *FN)(LPCVOID, LPCWSTR, LPVOID *, PUINT);
    FN f = (FN)real("VerQueryValueW");
    if (!f) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return f(block, sub, out, len);
}
