#include "csv.h"
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <ctype.h>

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

    SYSTEMTIME st;
    GetLocalTime(&st);

    sprintf(g_csv_name, "%04d%02d%02d.csv", st.wYear, st.wMonth, st.wDay);

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

void csv_dump_today(void)
{
    if (!*g_csv_name) return;

    /* prima passata: trova la larghezza massima del titolo */
    size_t maxw = 0;
    {
        FILE *fp = fopen(g_csv_name, "r, ccs=UTF-8");
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
    FILE *fp = fopen(g_csv_name, "r, ccs=UTF-8");
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
        unsigned s = tot % 60;
        swprintf(hms, 16, L"%02u:%02u:%02u", h, m, s);

        wprintf(L"%-*ls │ %ls\n", (int)maxw, title, hms);
    }
    fclose(fp);
}


void csv_prepare_today_name(void)
{
    if (*g_csv_name) return;         /* già pronto */

    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf(g_csv_name, "%04d%02d%02d.csv", st.wYear, st.wMonth, st.wDay);
}