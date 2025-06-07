#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static char g_output[MAX_PATH] = "";
static char g_export[MAX_PATH] = "";

static void get_cfg_filename(char *buf)
{
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata))) {
        snprintf(buf, MAX_PATH, "%s\\wimo", appdata);
        CreateDirectoryA(buf, NULL);

        strcat(buf, "\\.wimoconfig");
    } else {
        wprintf(L"Unable to access LOCALAPPDATA\n");
    }
}

static int rewrite_config_file(void)
{
    char fname[MAX_PATH];
    get_cfg_filename(fname);

    FILE *fp = fopen(fname, "w");
    if (!fp) return 0;

    // Se g_output è non vuoto, scrivo "output=…"
    if (g_output[0] != '\0') {
        fprintf(fp, "output=%s\n", g_output);
    }
    // Se g_export è non vuoto, scrivo "export=…"
    if (g_export[0] != '\0') {
        fprintf(fp, "export=%s\n", g_export);
    }

    fclose(fp);
    return 1;
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
    if (!fp) {
        char localapp[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localapp))) {
            snprintf(g_output, MAX_PATH, "%s\\wimo\\output", localapp);
            snprintf(g_export, MAX_PATH, "%s\\wimo\\export", localapp);

            CreateDirectoryA(g_output, NULL);
            CreateDirectoryA(g_export, NULL);

            char wimo_base[MAX_PATH];
            snprintf(wimo_base, MAX_PATH, "%s\\wimo", localapp);
            CreateDirectoryA(wimo_base, NULL);
        }

        rewrite_config_file();
        return;
    }

    char line[512];
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "output=", 7) == 0) {
            strncpy(g_output, line + 7, sizeof g_output - 1);
            g_output[sizeof(g_output)-1] = '\0';
        }
        else if (strncmp(line, "export=", 7) == 0) {
            strncpy(g_export, line + 7, sizeof g_export - 1);
            g_export[sizeof(g_export)-1] = '\0';
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

    strcpy(g_output, path_utf8);

    if (!rewrite_config_file()) {
        return 0;
    }
    return 1;
}

int cfg_set_export(const wchar_t *wpath)
{
    char path_utf8[MAX_PATH];
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                                  path_utf8, MAX_PATH, NULL, NULL);
    if (len == 0) return 0;

    CreateDirectoryW(wpath, NULL);

    strcpy(g_export, path_utf8);

    if (!rewrite_config_file()) {
        return 0;
    }
    return 1;
}

const char *cfg_get_output(void) { return g_output; }

const char *cfg_get_export(void) { return g_export; }
