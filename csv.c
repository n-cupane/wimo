#include "csv.h"
#include "config.h"
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <ctype.h>

void aggregate_csv(const char *filename);

static FILE *g_csv = NULL;
static CRITICAL_SECTION g_csv_lock;
static char g_csv_name[MAX_PATH] = "";

static void csv_write_field_w(FILE *, const wchar_t *);

typedef struct Row {
    wchar_t       *title;
    double         seconds;
    struct Row    *next;
} Row;

static void add_or_accumulate(Row **head, const wchar_t *title, double sec)
{
    for (Row *p = *head; p; p = p->next) {
        if (wcscmp(p->title, title) == 0) {
            p->seconds += sec;
            return;
        }
    }
    /* non trovato: crea nuovo nodo */
    Row *n   = calloc(1, sizeof(*n));
    n->title = _wcsdup(title);
    n->seconds = sec;
    n->next  = *head;
    *head    = n;
}

static void free_rows(Row *head)
{
    while (head) {
        Row *tmp = head->next;
        free(head->title);
        free(head);
        head = tmp;
    }
}

static void sec_to_hms(double secs, wchar_t buf[16])
{
    unsigned tot = (unsigned)(secs + 0.5);          /* arrotonda */
    unsigned h = tot / 3600;
    unsigned m = (tot % 3600) / 60;
    unsigned s = tot % 60;
    swprintf(buf, 16, L"%02u:%02u:%02u", h, m, s);  /*  HH:MM:SS */
}

int ends_with_csv(const wchar_t *filename) {
    size_t len = wcslen(filename);
    if (len < 4) return 0;
    const wchar_t *ext = filename + len - 4;
    return (_wcsicmp(ext, L".csv") == 0);
}

int starts_with_prefix(const wchar_t *filename, const wchar_t *prefix) {
    size_t prefix_len = wcslen(prefix);
    return (_wcsnicmp(filename, prefix, prefix_len) == 0);
}

void path_combine(wchar_t *out, size_t out_size, const wchar_t *dir, const wchar_t *file) {
    size_t dir_len = wcslen(dir);
    if (dir_len > 0 && dir[dir_len-1] != L'\\' && dir[dir_len-1] != L'/') {
        // Aggiunge backslash se manca
        _snwprintf(out, out_size, L"%s\\%s", dir, file);
    } else {
        _snwprintf(out, out_size, L"%s%s", dir, file);
    }
}

void aggregate_csv_w(const wchar_t *wfilename) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return;

    char *filename = (char *)malloc(size);
    if (!filename) return;

    WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, filename, size, NULL, NULL);

    aggregate_csv(filename);

    free(filename);
}

wchar_t* merge_all_csv_lines(int year, int month) {
    const char *daily_output_path_c = cfg_get_output();
    if (!*daily_output_path_c) daily_output_path_c = ".";

    const char *export_path_c = cfg_get_export();
    if (!*export_path_c) {
        wprintf(L"No export path defined.\nDefine an export path with: 'wimo config export <path>'\n");
        return NULL;
    }

    int len_out = MultiByteToWideChar(CP_UTF8, 0, daily_output_path_c, -1, NULL, 0);
    int len_exp = MultiByteToWideChar(CP_UTF8, 0, export_path_c, -1, NULL, 0);
    if (len_out <= 0 || len_exp <= 0) return NULL;

    wchar_t *daily_output_path = (wchar_t*)malloc(len_out * sizeof(wchar_t));
    wchar_t *export_path = (wchar_t*)malloc(len_exp * sizeof(wchar_t));
    if (!daily_output_path || !export_path) {
        free(daily_output_path);
        free(export_path);
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, daily_output_path_c, -1, daily_output_path, len_out);
    MultiByteToWideChar(CP_UTF8, 0, export_path_c, -1, export_path, len_exp);


    WIN32_FIND_DATAW findData;
    HANDLE hFind;
    wchar_t search_path[MAX_PATH];
    wchar_t filepath[MAX_PATH];

    // Path per trovare tutti i csv nella cartella
    _snwprintf(search_path, MAX_PATH, L"%s\\*.csv", daily_output_path);

    wchar_t *merged_path = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
    if (!merged_path) {
        wprintf(L"Unable to obtain merged path.\n");
        free(daily_output_path);
        free(export_path);
        return NULL;
    }

    _snwprintf(merged_path, MAX_PATH, L"%ls\\%04d%02d.csv", export_path, year, month);

    FILE *out = _wfopen(merged_path, L"w, ccs=UTF-8");
    if (!out) {
        wprintf(L"Error opening output file %ls\n", merged_path);
        free(daily_output_path);
        free(export_path);
        free(merged_path);
        return NULL;
    }

    fwprintf(out, L"Day,Window title,Seconds\n");
    int wrote_header = 1;

    hFind = FindFirstFileW(search_path, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        wprintf(L"No csv file found in %ls\n", daily_output_path);
        fclose(out);
        free(daily_output_path);
        free(export_path);
        free(merged_path);
        return NULL;
    }

    wchar_t prefix[16];
    swprintf(prefix, 16, L"%04d%02d", year, month);

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                && ends_with_csv(findData.cFileName)
                && starts_with_prefix(findData.cFileName, prefix)
            ) {
            path_combine(filepath, MAX_PATH, daily_output_path, findData.cFileName);
            wprintf(L"Merging file: %ls\n", filepath);

            wchar_t day_buf[3] = { L'\0', L'\0', L'\0' };
            day_buf[0] = findData.cFileName[6];
            day_buf[1] = findData.cFileName[7];

            aggregate_csv_w(filepath);

            FILE *in = _wfopen(filepath, L"r, ccs=UTF-8");
            if (!in) {
                wprintf(L"Unable to open %ls\n", filepath);
                continue;
            }

            wchar_t line[1024];
            int first_line = 1;

            while (fgetws(line, 1024, in)) {
                // Salta header (prima riga) per tutti tranne il primo file
                if (first_line) {
                    first_line = 0;
                    continue;
                }
                fwprintf(out, L"%ls,", day_buf);
                fputws(line, out);
            }

            fclose(in);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    fclose(out);

    wprintf(L"Merged file exported at: %ls\n", merged_path);

    free(daily_output_path);
    free(export_path);

    return merged_path;
}

const char *csv_get_filename(void) {
    return g_csv_name;
}

void csv_record(const wchar_t *title, double seconds)
{
    EnterCriticalSection(&g_csv_lock);
    csv_write_field_w(g_csv, title);
    fwprintf(g_csv, L",%.2f\n", seconds);
    fflush(g_csv);
    LeaveCriticalSection(&g_csv_lock);
}


void csv_write_field_w(FILE *fp, const wchar_t *ws)
{
    int quote = 0;
    for (const wchar_t *p = ws; *p; ++p)
        if (*p == L',' || *p == L'"' || *p == L'\n' || *p == L'\r')
            { quote = 1; break; }

    if (quote) fputwc(L'"', fp);

    for (const wchar_t *p = ws; *p; ++p) {
        if (*p == L'"') fputwc(L'"', fp);   /* raddoppia le " interne  */
        fputwc(*p, fp);
    }

    if (quote) fputwc(L'"', fp);
}

void csv_write_field(FILE *fp, const char *s)
{
    int quote = 0;
    for (const char *p = s; *p; ++p)
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r')
            { quote = 1; break; }

    if (quote) fputc('"', fp);

    for (const char *p = s; *p; ++p) {
        if (*p == '"') fputc('"', fp);  /* raddoppia le " interne  */
        fputc(*p, fp);
    }

    if (quote) fputc('"', fp);
}

void csv_init(void) {
    InitializeCriticalSection(&g_csv_lock);

    const char *base = cfg_get_output();
    if (!*base) base = ".";

    SYSTEMTIME st;
    GetLocalTime(&st);

    sprintf(g_csv_name, "%s/%04d%02d%02d.csv", base, st.wYear, st.wMonth, st.wDay);

    if (_access(g_csv_name, 0) == -1) {                 /* il file non esiste   */
        FILE *tmp = fopen(g_csv_name, "w, ccs=UTF-8");  /* stream wide-oriented */
        if (!tmp) {
            perror("fopen-create");
            return;
        }

        if (fwprintf(tmp, L"Window title,Seconds\n") < 0)   /* wide! */
            perror("fwprintf-header");

        fclose(tmp);
    }

    g_csv = fopen(g_csv_name, "a, ccs=UTF-8");
    if (!g_csv) {
        perror("fopen-append");
        return;
    }
}

void csv_close(void)
{
    if (g_csv) {
        fclose(g_csv);
        g_csv = NULL;
    }
    DeleteCriticalSection(&g_csv_lock);
}

void aggregate_csv(const char *filename)
{
    /* 1. leggi e accumula ------------------------------------------------ */
    FILE *in = fopen(filename, "r, ccs=UTF-8");
    if (!in) return;

    Row *list = NULL;
    wchar_t line[1024];

    /* salta header */
    fgetws(line, 1024, in);

    while (fgetws(line, 1024, in)) {
        /* trovare la virgola fuori dalle virgolette */
        int in_q = 0;
        wchar_t *comma = NULL;
        for (wchar_t *p = line; *p; ++p) {
            if (*p == L'"') in_q = !in_q;
            else if (*p == L',' && !in_q) { comma = p; break; }
        }
        if (!comma) continue;                       /* riga malformata */

        *comma = L'\0';
        const wchar_t *title  = line;
        const wchar_t *secstr = comma + 1;

        /* rimuovi CR/LF e spazi finali */
        wchar_t *end = (wchar_t*)secstr + wcslen(secstr);
        while (end > secstr && iswspace(end[-1])) --end;
        *end = L'\0';

        double sec = wcstod(secstr, NULL);

        /* togli eventuali "" esterne dal titolo */
        if (title[0] == L'"') {
            ++title;
            wchar_t *q = wcsrchr((wchar_t*)title, L'"');
            if (q) *q = L'\0';
        }

        add_or_accumulate(&list, title, sec);
    }
    fclose(in);

    /* 2. riscrivi il file ------------------------------------------------- */
    FILE *out = fopen(filename, "w, ccs=UTF-8");
    if (!out) { free_rows(list); return; }

    fwprintf(out, L"Window title,Seconds\n");

    for (Row *p = list; p; p = p->next)
        fwprintf(out, L"%ls,%.2f\n", p->title, p->seconds);

    fclose(out);
    free_rows(list);
}

void csv_aggregate_today(void) {
    if (*g_csv_name) aggregate_csv(g_csv_name);
}

void csv_dump_today(void) {
    if (*g_csv_name) csv_dump(g_csv_name);
}

void csv_dump_w(const wchar_t *wfilename) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return;

    char *filename = (char *)malloc(size);
    if (!filename) return;

    WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, filename, size, NULL, NULL);
    
    size_t max_day = 3;     // minimo "Day"
    size_t max_title = 12;  // minimo "Window title"
    {
        FILE *fp = fopen(filename, "r, ccs=UTF-8");
        if (!fp) return;

        wchar_t line[1024];
        // Salto l'header:
        fgetws(line, 1024, fp);

        while (fgetws(line, 1024, fp)) {
            // la riga ha "Day,Window title,Seconds"
            // Trovo prima virgola (fine Day)
            wchar_t *comma1 = wcschr(line, L',');
            if (!comma1) continue;
            size_t wday = comma1 - line;
            if (wday > max_day) max_day = wday;

            // Trovo seconda virgola (fine Window title)
            wchar_t *comma2 = wcschr(comma1 + 1, L',');
            if (!comma2) continue;
            size_t wtitle = comma2 - (comma1 + 1);
            if (wtitle > max_title) max_title = wtitle;
        }

        fclose(fp);
    }

    // 2) Seconda passata: stampo tabella con separatori corretti
    FILE *fp = fopen(filename, "r, ccs=UTF-8");
    if (!fp) return;

    wchar_t line[1024];
    // Salto l'header:
    fgetws(line, 1024, fp);

    // Colonna Time la fissiamo a 5 caratteri (formato "HH:MM")
    const size_t time_col_width = 5;

    // Stampiamo intestazione:
    //   ┌─ Colonna Day ─┬─ Colonna Window title ─┬─ Colonna Time ─┐
    //   " Day" (max_day) " │ " "Window title" (max_title) " │ " "Time"
    wprintf(
        L" %-*ls │ %-*ls │ %*ls\n",
        (int)max_day,   L"Day",
        (int)max_title, L"Window title",
        (int)time_col_width, L"Time"
    );

    // Stampiamo linea di separazione:
    //  spazio + ─×max_day + spazio + ┼ +
    //  spazio + ─×max_title + spazio + ┼ +
    //  spazio + ─×time_col_width + spazio
    // Corpo riga separazione:
    //   (max_day + 2) trattini, poi '┼',
    //   (max_title + 2) trattini, poi '┼',
    //   (time_col_width + 2) trattini

    // Primo segmento (Day):
    for (size_t i = 0; i < max_day + 2; ++i) fputwc(L'─', stdout);
    fputwc(L'┼', stdout);
    // Secondo segmento (Window title):
    for (size_t i = 0; i < max_title + 2; ++i) fputwc(L'─', stdout);
    fputwc(L'┼', stdout);
    // Terzo segmento (Time):
    for (size_t i = 0; i < time_col_width + 2; ++i) fputwc(L'─', stdout);
    fputwc(L'\n', stdout);

    // Ora stampiamo ogni riga
    while (fgetws(line, 1024, fp)) {
        // Separo Day, Window title, Seconds
        wchar_t *comma1 = wcschr(line, L',');
        if (!comma1) continue;
        *comma1 = L'\0';
        const wchar_t *day_str = line;

        wchar_t *comma2 = wcschr(comma1 + 1, L',');
        if (!comma2) continue;
        *comma2 = L'\0';
        const wchar_t *title = comma1 + 1;

        wchar_t *sec_str = comma2 + 1;
        // Rimuovo eventuali CR/LF/spazi finali in sec_str
        wchar_t *end = sec_str + wcslen(sec_str);
        while (end > sec_str && iswspace(end[-1])) --end;
        *end = L'\0';

        double secs = wcstod(sec_str, NULL);
        unsigned tot = (unsigned)(secs + 0.5);
        unsigned h = tot / 3600;
        unsigned m = (tot % 3600) / 60;
        wchar_t hms[8];
        swprintf(hms, 8, L"%02u:%02u", h, m);

        // Stampo la riga con padding:
        //  spazio + Day (padded a max_day) + " │ " +
        //  Window title (padded a max_title) + " │ " +
        //  Time (padded a time_col_width, right-aligned)
        wprintf(
            L" %-*ls │ %-*ls │ %*ls\n",
            (int)max_day,   day_str,
            (int)max_title, title,
            (int)time_col_width, hms
        );
    }

    fclose(fp);

    free(filename);
}


void csv_dump(const char *filename) {
    /* prima passata: trova la larghezza massima del titolo */
    size_t maxw = 0;
    {
        FILE *fp = fopen(filename, "r, ccs=UTF-8");
        if (!fp) return;

        wchar_t line[1024];
        fgetws(line, 1024, fp);               /* salta header */
        while (fgetws(line, 1024, fp)) {
            int in_q = 0; wchar_t *comma = NULL;
            for (wchar_t *p = line; *p; ++p) {
                if (*p == L'"') in_q = !in_q;
                else if (*p == L',' && !in_q) { comma = p; break; }
            }
            if (!comma) continue;
            *comma = L'\0';
            size_t w = wcslen(line);
            if (w > maxw) maxw = w;
        }
        fclose(fp);
    }
    if (maxw < 12) maxw = 12;                 /* minimo estetico */

    /* seconda passata: stampa in tabella */
    FILE *fp = fopen(filename, "r, ccs=UTF-8");
    if (!fp) return;

    wchar_t line[1024], hms[16];
    fgetws(line, 1024, fp);                   /* salta header */

    /* intestazione */
    wprintf(L"%-*ls │ %ls\n", (int)maxw, L"Window title", L"Time");
    wprintf(L"%.*ls─┼─%.*ls\n",
            (int)maxw, L"────────────────────────────────────────────────────────",
            8, L"────────");

    while (fgetws(line, 1024, fp)) {
        int in_q = 0; wchar_t *comma = NULL;
        for (wchar_t *p = line; *p; ++p) {
            if (*p == L'"') in_q = !in_q;
            else if (*p == L',' && !in_q) { comma = p; break; }
        }
        if (!comma) continue;

        *comma = L'\0';
        const wchar_t *title  = line;
        const wchar_t *secstr = comma + 1;
        double secs = wcstod(secstr, NULL);

        /* HH:MM:SS */
        unsigned tot = (unsigned)(secs + 0.5);
        unsigned h = tot / 3600;
        unsigned m = (tot % 3600) / 60;
        swprintf(hms, 16, L"%02u:%02u", h, m);

        wprintf(L"%-*ls │ %ls\n", (int)maxw, title, hms);
    }
    fclose(fp);
}

void csv_prepare_today_name(void)
{
    if (*g_csv_name) return;         /* già pronto */

    const char *base = cfg_get_output();
    if (!*base) base = ".";

    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf(g_csv_name, "%s/%04d%02d%02d.csv", base, st.wYear, st.wMonth, st.wDay);
}

void dump_year_month(int year, int month) {
    wchar_t* filename = merge_all_csv_lines(year, month);

    if (!filename) {
        wprintf(L"Unable to obtain file name.\n");
        return;
    }

    csv_dump_w(filename);
    free(filename);
}