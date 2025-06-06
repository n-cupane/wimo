#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static char g_output[MAX_PATH] = "";

static void get_cfg_filename(char *buf)
{
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    char *p = strrchr(buf, '\\');
    strcpy(p ? p + 1 : buf, ".wimoconfig");
}

void print_config(void) {
    char fname[MAX_PATH];
    get_cfg_filename(fname);

    FILE *fp = fopen(fname, "r");
    if (!fp) {
        wprintf(L"(no .wimoconfig found)\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        wprintf(L"%hs\n", line);
    }

    fclose(fp);
}

void cfg_load(void)
{
    char fname[MAX_PATH];
    get_cfg_filename(fname);

    FILE *fp = fopen(fname, "r");
    if (!fp) return;

    char line[512];
    while (fgets(line, sizeof line, fp)) {
        if (strncmp(line, "output=", 7) == 0) {
            strncpy(g_output, line + 7, sizeof g_output - 1);
            g_output[strcspn(g_output, "\r\n")] = '\0';
        }
    }
    fclose(fp);
}

int cfg_set_output(const wchar_t *wpath)
{
    /* converte wchar → utf-8 (binario = UTF-8) */
    char path_utf8[MAX_PATH];
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                                  path_utf8, MAX_PATH, NULL, NULL);
    if (len == 0) return 0;

    CreateDirectoryW(wpath, NULL);

    char fname[MAX_PATH];
    get_cfg_filename(fname);
    FILE *fp = fopen(fname, "w");
    if (!fp) return 0;

    fprintf(fp, "output=%s\n", path_utf8);
    fclose(fp);

    strcpy(g_output, path_utf8);
    return 1;
}

const char *cfg_get_output(void) { return g_output; }
