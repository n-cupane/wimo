#include "csv.h"
#include "window.h"
#include "monitor.h"
#include "config.h"
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <fcntl.h>
#include <io.h>
#include <Windows.h>


// #define UNICODE
// #define _UNICODE
#define WIDE_MATCH(str, val) (wcscmp((str), (val)) == 0)

int is_valid_year(const wchar_t *year);
int is_valid_month(const wchar_t *month);

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);                 /* stdout UTF-8      */
    _setmode(_fileno(stdout), _O_U16TEXT);       /* wide → UTF-16LE   */

    cfg_load();


    if (argc == 1) {
        argv[1] = L"run";
        argc = 2;
    }

    if (wcscmp(argv[1], L"config") == 0) {
        if (argc == 2)
            print_config();

        if (argc >= 3) {
            if (wcscmp(argv[2], L"output") == 0) {
                if (argc < 4) {
                    wprintf(L"%hs\n", cfg_get_output());
                    return 0;
                } else if (argc == 4) {
                    if (cfg_set_output(argv[3]))
                        wprintf(L"Output directory set to \"%ls\"\n", argv[3]);
                    else
                        wprintf(L"Unable to write .wimoconfig\n");
                
                    return 0;
                }
            } else if (wcscmp(argv[2], L"export") == 0) {
                if (argc < 4) {
                    wprintf(L"%hs\n", cfg_get_export());
                    return 0;
                } else if (argc == 4) {
                    if (cfg_set_export(argv[3]))
                        wprintf(L"Export directory set to \"%ls\"\n", argv[3]);
                    else
                        wprintf(L"Unable to write .wimoconfig\n");
                
                    return 0;
                }
            }
        }
    } else if (wcscmp(argv[1], L"start") == 0) {
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
        if (argc == 2) {
            csv_prepare_today_name();

            HANDLE evt = OpenEventW(SYNCHRONIZE, FALSE, L"Global\\WimoStopEvent");
            
            if (!evt) {
                wprintf(L"wimo.exe is NOT running.\n");
                return 0;
            }

            CloseHandle(evt);

            csv_aggregate_today();

            wprintf(L"\n--- Today log ---\n");
            csv_dump_today();

            ULONGLONG s = win_uptime_seconds(L"wimo.exe");
            if (s == 0) {
                wprintf(L"wimo.exe is running (uptime unknown).\n");
            } else {
                int h = (int) (s / 3600);
                int m = (int) ((s % 3600) / 60);
                int sec = (int) (s % 60);

                wprintf(L"wimo.exe is running for %02d:%02d:%02d (hh:mm:ss)\n", h, m, sec);
            }
        } else if (argc == 3) {
            const wchar_t *dash = wcschr(argv[2], L'-');

            if (dash == NULL) {
                wprintf(L"Wrong date format. Expected 'yyyy-MM'.\n");
                return 1;
            }

            size_t len = dash - argv[2];
            wchar_t year[10];
            wcsncpy(year, argv[2], len);
            year[len] = L'\0';

            const wchar_t *month = dash + 1;

            if (!is_valid_year(year)) {
                wprintf(L"Year must be exactly 4 digits.\n");
                return 1;
            }

            if (!is_valid_month(month)) {
                wprintf(L"Month must be exactly 2 digits and between 01 and 12.\n");
                return 1;
            }

            int year_int = (int)wcstol(year, NULL, 10);
            int month_int = (int)wcstol(month, NULL, 10);

            dump_year_month(year_int, month_int);
        }

        return 0;
    } else if (wcscmp(argv[1], L"export") == 0) {

    }
    
    return 0;
}

int is_valid_year(const wchar_t *year) {
    for (int i = 0; i < 4; i++) {
        if (!iswdigit(year[i])) return 0;
    }
    return year[4] == L'\0'; 
}

int is_valid_month(const wchar_t *month) {
    if (month[0] == L'\0' || month[1] == L'\0' || month[2] != L'\0')
        return 0;
    if (!iswdigit(month[0]) || !iswdigit(month[1]))
        return 0;

    int m = (month[0] - L'0') * 10 + (month[1] - L'0');
    return (m >= 1 && m <= 12);
}