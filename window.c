#include "window.h"
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <wchar.h>
#include <stdlib.h>

#define WIDE_MATCH(s,v) (wcscmp((s),(v))==0)

static ULONGLONG filetime_to_ull(const FILETIME *ft)
{
    return ((ULONGLONG)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
}

ULONGLONG win_uptime_seconds(const wchar_t *exe)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = { .dwSize = sizeof(pe) };
    ULONGLONG secs = 0;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (WIDE_MATCH(pe.szExeFile, exe)) {         
                HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                         FALSE, pe.th32ProcessID);
                if (hp) {
                    FILETIME ftCreate, ftExit, ftKernel, ftUser, ftNow;
                    if (GetProcessTimes(hp, &ftCreate, &ftExit,
                                        &ftKernel, &ftUser)) {
                        GetSystemTimeAsFileTime(&ftNow);
                        ULONGLONG diff100ns =
                            filetime_to_ull(&ftNow) - filetime_to_ull(&ftCreate);
                        secs = diff100ns / 10000000ULL;          
                    }
                    CloseHandle(hp);
                }
                break;                                           
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return secs;
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
    if (WIDE_MATCH(exe_name, L"Code.exe")) {
        return extract_field(original_title, L" - ", 2, 1);
    } else if (WIDE_MATCH(exe_name, L"chrome.exe")) {
        return _wcsdup(L"Google Chrome");
    } else {
        return original_title;
    }
}

wchar_t* win_get_title(HWND h) {
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