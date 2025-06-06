#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <shlwapi.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <errno.h>
#include <shellapi.h>

// #define UNICODE
// #define _UNICODE
#define WIDE_MATCH(str, val) (wcscmp((str), (val)) == 0)

static FILE *g_csv = NULL;
static CRITICAL_SECTION g_csv_lock;
static HANDLE g_stop_event = NULL;

void init_csv(void) {
    InitializeCriticalSection(&g_csv_lock);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char filename[MAX_PATH];
    sprintf(filename, "%04d%02d%02d.csv", st.wYear, st.wMonth, st.wDay);

    if (_access(filename, 0) == -1) {                 /* il file non esiste   */
        FILE *tmp = fopen(filename, "w, ccs=UTF-8");  /* stream wide-oriented */
        if (!tmp) {
            perror("fopen-create");
            return;
        }

        if (fwprintf(tmp, L"Window title,Seconds\n") < 0)   /* wide! */
            perror("fwprintf-header");

        fclose(tmp);
    }

    g_csv = fopen(filename, "a, ccs=UTF-8");
    if (!g_csv) {
        perror("fopen-append");
        return;
    }
}

void close_csv(void)
{
    if (g_csv) {
        fclose(g_csv);
        g_csv = NULL;
    }
    DeleteCriticalSection(&g_csv_lock);
}

void csv_write_field_w(FILE *fp, const wchar_t *ws)
{
    int quote = 0;
    for (const wchar_t *p = ws; *p; ++p)
        if (*p == L',' || *p == L'"' || *p == L'\n' || *p == L'\r')
            { quote = 1; break; }

    if (quote) fputwc(L'"', fp);

    for (const wchar_t *p = ws; *p; ++p) {
        if (*p == L'"') fputwc(L'"', fp);   /* raddoppia le " interne  */
        fputwc(*p, fp);
    }

    if (quote) fputwc(L'"', fp);
}

void csv_write_field(FILE *fp, const char *s)
{
    int quote = 0;
    for (const char *p = s; *p; ++p)
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r')
            { quote = 1; break; }

    if (quote) fputc('"', fp);

    for (const char *p = s; *p; ++p) {
        if (*p == '"') fputc('"', fp);  /* raddoppia le " interne  */
        fputc(*p, fp);
    }

    if (quote) fputc('"', fp);
}

wchar_t* extract_field(const wchar_t* input, const wchar_t* separator, int expected_separators, int field_index) {
    if (!input || !separator || expected_separators < 1 || field_index < 0) {
        return NULL;
    }

    int count = 0;
    const wchar_t* pos = input;

    while ((pos = wcsstr(pos, separator)) != NULL) {
        count++;
        pos += wcslen(separator);
    }

    if (count != expected_separators) {
        return NULL;
    }

    const wchar_t* start = input;
    const wchar_t* end = NULL;

    for (int i = 0; i <= field_index; i++) {
        end = wcsstr(start, separator);

        if (i == field_index) {
            if (!end) end = input + wcslen(input);
            break;
        }

        if (!end) return NULL;

        start = end + wcslen(separator);
    }

    size_t len = (size_t) (end - start);
    if (len == 0) return NULL;

    wchar_t* result = malloc((len + 1) * sizeof(wchar_t));
    if (!result) return NULL;

    wcsncpy(result, start, len);
    result[len] = L'\0';

    return result;
}

const wchar_t* get_clean_title(const wchar_t* exe_name, const wchar_t* original_title) {
    wprintf(L"Exe: %ls\n", exe_name);
    
    if (WIDE_MATCH(exe_name, L"Code.exe")) {
        return extract_field(original_title, L" - ", 2, 1);
    } else if (WIDE_MATCH(exe_name, L"chrome.exe")) {
        return _wcsdup(L"Google Chrome");
    } else {
        return original_title;
    }
}

wchar_t* get_window_title(HWND h) {
    wchar_t title[256], exe[256];

    GetWindowTextW(h, title, 256);

    DWORD pid;
    GetWindowThreadProcessId(h, &pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if (p) {
        GetModuleFileNameExW(p, NULL, exe, 256);
        PathStripPathW(exe);
        CloseHandle(p);
    } else {
        exe[0] = L'\0';
    }

    const wchar_t *clean = get_clean_title(exe, title);   
                                                          
    wchar_t *dup = _wcsdup(clean);                        
    if (clean != title) free((void*)clean);               

    return dup;   /* il chiamante deve fare free() */
}

// Simulazione della funzione update_stats
void update_stats(HWND hwnd, ULONGLONG delta_ms) {
    wchar_t* title = get_window_title(hwnd);

    if (title && g_csv) {
        EnterCriticalSection(&g_csv_lock);

        csv_write_field_w(g_csv, title);
        fwprintf(g_csv, L",%.2f\n", delta_ms / 1000.0);

        fflush(g_csv);
        LeaveCriticalSection(&g_csv_lock);
    }

    free(title);
}

// Thread che monitora la finestra attiva
DWORD WINAPI poller(void* _) {
    HWND last = NULL;
    ULONGLONG t0 = GetTickCount64();

    for (;;) {
        DWORD wait = WaitForSingleObject(g_stop_event, 1000);
        if (wait == WAIT_OBJECT_0)
            break;

        HWND h = GetForegroundWindow();
        if (h && h != last) {                      // cambio finestra
            ULONGLONG now = GetTickCount64();
            if (last) update_stats(last, now - t0); // accredita delta
            last = h;
            t0 = now;
        }
    }

    if (last) {
        ULONGLONG now = GetTickCount64();
        update_stats(last, now - t0);
    }

    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc == 1) {
        argv[1] = L"run";
        argc = 2;
    }

    if (wcscmp(argv[1], L"start") == 0) {
        HANDLE evt = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\WimoStopEvent");

        if (evt) {
            wprintf(L"wimo.exe is already running\n");
            CloseHandle(evt);
            return 0;
        }

        wchar_t self[MAX_PATH];
        GetModuleFileNameW(NULL, self, MAX_PATH);
        wchar_t cmd[MAX_PATH + 10];
        swprintf(cmd, _countof(cmd), L"\"%s\" run", self);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            wprintf(L"wimo.exe started in background.\n");
        } else {
            fwprintf(stderr, L"CreateProcess failed (%lu)\n", GetLastError());
        }

        return 0;
    } else if (wcscmp(argv[1], L"stop") == 0) {
        HANDLE evt = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\WimoStopEvent");

        if (!evt) {
            wprintf(L"wimo.exe is not running.\n");
            return 0;
        }

        SetEvent(evt);
        CloseHandle(evt);
        wprintf(L"Stop request sent.\n");
        return 0;
    } else if (wcscmp(argv[1], L"run") == 0) {
        g_stop_event = CreateEventW(NULL, TRUE, FALSE, L"Global\\WimoStopEvent");

        if (!g_stop_event) {
            fwprintf(stderr, L"Unable to create stop event (%lu)\n", GetLastError());
            return 1;
        }

        init_csv();
        HANDLE hThread = CreateThread(NULL, 0, poller, NULL, 0, NULL);
        if (!hThread) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }

        WaitForSingleObject(hThread, INFINITE);

        CloseHandle(hThread);
        CloseHandle(g_stop_event);
        close_csv();

        return 0;
    }
    
    return 0;
}