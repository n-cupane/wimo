#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <shlwapi.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#define WIDE_MATCH(str, val) (wcscmp((str), (val)) == 0)

#if !defined(_MSC_VER) && (__STDC_VERSION__ < 202311L && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) && !defined(wcsdup))
wchar_t *wcsdup_custom(const wchar_t *s) {
    if (s == NULL) return NULL;
    size_t len = wcslen(s) + 1; 
    wchar_t *d = (wchar_t *)malloc(len * sizeof(wchar_t));
    if (d == NULL) return NULL; 
    memcpy(d, s, len * sizeof(wchar_t)); 
    return d;
}
#define WCSDUP_TO_USE wcsdup_custom
#elif defined(_MSC_VER)
#define WCSDUP_TO_USE _wcsdup
#else
#define WCSDUP_TO_USE wcsdup
#endif

// Funzione di utilità: ottiene il titolo della finestra
void print_window_title(HWND hwnd) {
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    printf(" -> Finestra: \"%s\"\n", title);
}

// Simulazione della funzione update_stats
void update_stats(HWND hwnd, ULONGLONG delta_ms) {
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    printf("[INFO] Tempo su \"%s\": %.2f secondi\n", title, delta_ms / 1000.0);
}

wchar_t* extract_field(const wchar_t* input, const wchar_t* separator, int expected_separators, int field_index) {
    if (!input || !separator || expected_separators < 1 || field_index < 0) {
        return NULL;
    }

    int count = 0;
    const wchar_t* pos = input;

    while ((pos = wcsstr(pos, separator)) != NULL) {
        count++;
        pos += wcslen(separator);
    }

    if (count != expected_separators) {
        return NULL;
    }

    const wchar_t* start = input;
    wchar_t* end = NULL;

    for (int i = 0; i <= field_index; i++) {
        end = wcsstr(start, separator);

        if (i == field_index) {
            if (!end) end = input + wcslen(input);
            break;
        }

        if (!end) return NULL;

        start = end + wcslen(separator);
    }

    size_t len = (size_t) (end - start);
    if (len == 0) return NULL;

    wchar_t* result = malloc((len + 1) * sizeof(wchar_t));
    if (!result) return NULL;

    wcsncpy(result, start, len);
    result[len] = L'\0';

    return result;
}

wchar_t* get_clean_title(const wchar_t* exe_name, const wchar_t* original_title) {
    if (WIDE_MATCH(exe_name, L"Code.exe")) {
        return extract_field(original_title, L" - ", 2, 1);
    } else {
        return original_title;
    }
}

// wchar_t* get_clean_title(const wchar_t* exe_name, const wchar_t* original_title) {
//     // È buona norma che le stringhe di input non modificate siano const.
//     // La funzione restituisce una NUOVA stringa allocata, che il chiamante deve liberare.

//     if (original_title == NULL) {
//         return NULL; // Non si può fare nulla con un titolo nullo
//     }
//     if (exe_name == NULL) {
//         // Se exe_name è nullo, comportati come nel caso default: restituisci una copia dell'originale.
//         return WCSDUP_TO_USE(original_title);
//     }

//     if (wcscmp(exe_name, L"Code.exe") == 0) {
//         // Ci aspettiamo il formato "QUALCOSA - PARTE_DESIDERATA - VISUAL STUDIO CODE" (o simile)
//         // Vogliamo estrarre PARTE_DESIDERATA.

//         wchar_t* title_copy = WCSDUP_TO_USE(original_title); // Crea una copia modificabile per wcstok_s
//         if (!title_copy) {
//             return NULL; // Fallimento dell'allocazione
//         }

//         wchar_t* context = NULL; // Puntatore di contesto per wcstok_s
//         const wchar_t* delimiter = L" - "; // Delimitatore usato per "splittare"
//         wchar_t* result_str_to_return = NULL;

//         // Estrai la prima parte (QUALCOSA)
//         wchar_t* token1 = wcstok_s(title_copy, delimiter, &context);
//         if (token1) {
//             // Estrai la seconda parte (PARTE_DESIDERATA)
//             wchar_t* token2 = wcstok_s(NULL, delimiter, &context);
//             if (token2) {
//                 // Controlla se esiste una terza parte (VISUAL STUDIO CODE) per confermare il formato
//                 wchar_t* token3 = wcstok_s(NULL, delimiter, &context);
//                 if (token3) {
//                     // Formato "token1 - token2 - token3" confermato.
//                     // Vogliamo token2. Duplichiamolo perché title_copy verrà liberato.
//                     result_str_to_return = WCSDUP_TO_USE(token2);
//                 }
//                 // Se non ci sono 3 parti (es. solo "token1 - token2"),
//                 // result_str_to_return rimane NULL, e si passerà al comportamento di default.
//             }
//         }

//         free(title_copy); // Libera la copia temporanea usata per la tokenizzazione

//         if (result_str_to_return) {
//             return result_str_to_return; // Restituisci la parte desiderata duplicata (il chiamante la libera)
//         }
//         // Se il parsing specifico per "Code.exe" non è riuscito a estrarre
//         // la parte desiderata nel formato atteso, si passa al comportamento di default.
//     }
    
//     // Caso default: per qualsiasi altro exe_name, o se il parsing per "Code.exe" non ha prodotto un risultato specifico.
//     // Restituisci una copia duplicata del titolo originale.
//     return WCSDUP_TO_USE(original_title); // Il chiamante deve liberare questa stringa.
// }

void print_window_info(HWND h) {
    wchar_t title[256], exe[256];

    GetWindowTextW(h, title, 256);

    DWORD pid;
    GetWindowThreadProcessId(h, &pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if (p) {
        GetModuleFileNameExW(p, NULL, exe, 256);
        PathStripPathW(exe);
        CloseHandle(p);
    } else {
        wcscpy(exe, L"(accesso negato)");
    }

    wchar_t* clean_title = get_clean_title(exe, title);

    if (clean_title != NULL) {
        wcscpy(title, clean_title);
        free(clean_title);
    }

    wprintf(L"[INFO] Finestra attiva: \"%s\" | Processo: %s\n", title, exe);


}

// Thread che monitora la finestra attiva
DWORD WINAPI poller(void* _) {
    HWND last = NULL;
    ULONGLONG t0 = GetTickCount64();
    for (;;) {
        HWND h = GetForegroundWindow();
        if (h && h != last) {                      // cambio finestra
            ULONGLONG now = GetTickCount64();
            if (last) update_stats(last, now - t0); // accredita delta
            last = h;
            t0 = now;
            // print_window_title(h);
            print_window_info(h);
        }
        Sleep(1000); // poll ogni secondo
    }
    return 0;
}

int main() {
    printf("Avvio monitoraggio delle finestre attive...\n");

    // Avvio del thread
    HANDLE hThread = CreateThread(NULL, 0, poller, NULL, 0, NULL);
    if (!hThread) {
        fprintf(stderr, "Errore nella creazione del thread\n");
        return 1;
    }

    // Il main thread resta vivo per permettere al thread di polling di lavorare
    // Puoi sostituire questa parte con un messaggio di uscita o altro meccanismo
    printf("Premi INVIO per uscire.\n");
    getchar();

    // Terminazione pulita (non obbligatoria qui, ma buona pratica)
    TerminateThread(hThread, 0);
    CloseHandle(hThread);

    return 0;
}
