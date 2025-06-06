#pragma once
#include <windows.h>

void  monitor_init(void);
DWORD WINAPI monitor_thread(void *);
