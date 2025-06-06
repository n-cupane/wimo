#pragma once
#include <windows.h>
#include <wchar.h>

wchar_t   *win_get_title(HWND);
ULONGLONG  win_uptime_seconds(const wchar_t *exe);
