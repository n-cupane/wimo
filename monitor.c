#include "csv.h"
#include "window.h"
#include <windows.h>
#include <stdlib.h>

static HANDLE g_stop_event = NULL;

static void update_stats(HWND, ULONGLONG);

void monitor_init(void) {
    if (!g_stop_event)
        g_stop_event = CreateEventW(NULL, TRUE, FALSE,
                                    L"Global\\WimoStopEvent");
}


DWORD WINAPI monitor_thread(void* _) {
    HWND last = NULL;
    ULONGLONG t0 = GetTickCount64();

    monitor_init();

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

void update_stats(HWND hwnd, ULONGLONG delta_ms) {
    wchar_t* title = win_get_title(hwnd);

    if (title) {
        if (title[0] != L'\0') {
            csv_record(title, delta_ms / 1000.0);
        }
        free(title);
    }
}