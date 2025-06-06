#pragma once
#include <wchar.h>
#include <windows.h>

const char *csv_get_filename(void); 
void csv_init(void);
void csv_close(void);
void csv_record(const wchar_t *title, double s);
void csv_aggregate_today(void);
void csv_dump_today(void);
void csv_prepare_today_name(void);