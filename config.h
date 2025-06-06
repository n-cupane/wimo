#pragma once
#include <wchar.h>

void cfg_load(void);
int cfg_set_output(const wchar_t *path);
const char *cfg_get_output(void);
void print_config(void);