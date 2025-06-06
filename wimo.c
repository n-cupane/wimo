#include <stdio.h>
#include <wchar.h>
#include "csv.h"
#include "window.h"
#include "monitor.h"
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

// #define UNICODE
// #define _UNICODE
#define WIDE_MATCH(str, val) (wcscmp((str), (val)) == 0)

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
        csv_init();
        monitor_init();

        HANDLE hThread = CreateThread(NULL, 0, monitor_thread, NULL, 0, NULL);
        if (!hThread) {
            fwprintf(stderr, L"Error creating thread\n");
            return 1;
        }

        WaitForSingleObject(hThread, INFINITE);

        CloseHandle(hThread);
        csv_close();
        csv_aggregate_today();

        return 0;
    } else if (wcscmp(argv[1], L"status") == 0) {
        HANDLE evt = OpenEventW(SYNCHRONIZE, FALSE, L"Global\\WimoStopEvent");
        
        if (!evt) {
            wprintf(L"wimo.exe is NOT running.\n");
            return 0;
        }

        CloseHandle(evt);

        ULONGLONG s = win_uptime_seconds(L"wimo.exe");
        if (s == 0) {
            wprintf(L"wimo.exe is running (uptime unknown).\n");
        } else {
            int h = (int) (s / 3600);
            int m = (int) ((s % 3600) / 60);
            int sec = (int) (s % 60);

            wprintf(L"wimo.exe is running for %02d:%02d:%02d (hh:mm:ss)\n", h, m, sec);
        }

        return 0;
    }
    
    return 0;
}