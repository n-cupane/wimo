#pragma once
#include <wchar.h>
#include <windows.h>

const char *csv_get_filename(void); 
void csv_init(void);
void csv_close(void);
void csv_record(const wchar_t *title, double s);
void csv_aggregate_today(void);
void csv_dump_today(void);
void csv_dump(const char *filename);
void csv_prepare_today_name(void);
wchar_t* merge_all_csv_lines(int year, int month);
void dump_year_month(int year, int month);