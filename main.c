/*
 * main.c - Nepali tray calendar, using the AD<->BS conversion engine from
 * https://github.com/aatishnn/nepdate (date.c / date.h) instead of a
 * hand-rolled table.
 *
 * SETUP: place date.c and date.h (from that repo) in this same folder
 * before building:
 *   curl -O https://raw.githubusercontent.com/aatishnn/nepdate/master/date.c
 *   curl -O https://raw.githubusercontent.com/aatishnn/nepdate/master/date.h
 *
 * Their struct/function shapes (inferred from date.c's usage - verify
 * against your actual downloaded date.h):
 *   struct eng_date { int year, month, date; char day[10]; char emonth[10]; int num_day; };
 *   struct nep_date { int year, month, date; char day[10]; char nmonth[10]; int num_day; };
 *   int eng_to_nep(struct eng_date date, struct nep_date *out);   // AD -> BS
 *   int nep_to_eng(struct nep_date date, struct eng_date *out);   // BS -> AD
 * Both return 0 if the date falls outside the supported range
 * (roughly AD 1944-2033 / BS 2000-2090).
 *
 * BUILD:
 *   make            (see Makefile - now compiles date.c alongside main.c)
 */

#define WIN32_LEAN_AND_MEAN
/* UNICODE / _UNICODE / _CRT_SECURE_NO_WARNINGS are set via the Makefile's /D flags */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "date.h"

/* ---------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------- */
#define WM_TRAYICON        (WM_USER + 1)
#define ID_TRAY_ICON       1001
#define ID_TIMER_REFRESH   2001
#define ID_MENU_EXIT       3001

#define REFRESH_INTERVAL_MS (60 * 1000)   /* check once a minute whether the day rolled over */

static const wchar_t *WND_CLASS_NAME   = L"NepaliTrayCalWndClass";
static const wchar_t *APP_TITLE        = L"Nepali Calendar";

static const wchar_t *g_bsMonthNames[12] = {
    L"बैशाख", L"जेठ", L"असार", L"श्रावण", L"भदौ", L"असोज",
    L"कार्तिक", L"मंसिर", L"पुष", L"माघ", L"फागुन", L"चैत"
};

/* ---------------------------------------------------------------------
 * Today's BS date, via the nepdate engine
 * --------------------------------------------------------------------- */
static int GetTodayBs(struct nep_date *out)
{
    SYSTEMTIME st;
    struct eng_date ed;

    GetLocalTime(&st);

    memset(&ed, 0, sizeof(ed));
    ed.year  = st.wYear;
    ed.month = st.wMonth;
    ed.date  = st.wDay;

    return eng_to_nep(ed, out); /* 0 if today's AD date is outside the table's range */
}

/* ---------------------------------------------------------------------
 * Icon rendering: draw the BS day number into an alpha-blended HICON.
 * (unchanged from before - only the date source changed)
 * --------------------------------------------------------------------- */
static HICON RenderDayIcon(int day, COLORREF fgColor)
{
    int size = GetSystemMetrics(SM_CXSMICON);
    if (size <= 0) size = 16;

    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = -size; /* top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP colorBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, colorBmp);

    RECT full = { 0, 0, size, size };
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(memDC, &full, black);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    HFONT font = CreateFontW(
        (int)(size * 0.82), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(memDC, font);

    wchar_t buf[4];
    swprintf(buf, 4, L"%02d", day);

    RECT textRect = { 0, 0, size, size };
    DrawTextW(memDC, buf, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDC, oldFont);
    DeleteObject(font);

    BYTE fr = GetRValue(fgColor), fg = GetGValue(fgColor), fb = GetBValue(fgColor);
    DWORD *px = (DWORD *)bits;
    for (int i = 0; i < size * size; i++) {
        BYTE b = (BYTE)(px[i] & 0xFF);
        BYTE alpha = b;
        BYTE r = (BYTE)((fr * alpha) / 255);
        BYTE g = (BYTE)((fg * alpha) / 255);
        BYTE bl = (BYTE)((fb * alpha) / 255);
        px[i] = ((DWORD)alpha << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | bl;
    }

    HBITMAP maskBmp = CreateBitmap(size, size, 1, 1, NULL);

    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon    = TRUE;
    ii.hbmMask  = maskBmp;
    ii.hbmColor = colorBmp;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(maskBmp);
    SelectObject(memDC, oldBmp);
    DeleteObject(colorBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);

    return hIcon;
}

/* ---------------------------------------------------------------------
 * Tray icon management
 * --------------------------------------------------------------------- */
static NOTIFYICONDATAW g_nid;
static HICON g_currentIcon = NULL;

/* ---------------------------------------------------------------------
 * System theme detection - so the "normal day" color stays visible
 * whether the taskbar is in light or dark mode.
 * --------------------------------------------------------------------- */
static int IsSystemDarkTheme(void)
{
    DWORD value = 1; /* default to light theme if the key is missing */
    DWORD size = sizeof(value);
    LONG result = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme",
        RRF_RT_REG_DWORD, NULL, &value, &size);

    if (result != ERROR_SUCCESS) return 0; /* key missing -> assume light -> not dark */
    return value == 0; /* 0 = dark theme, 1 = light theme */
}

static void UpdateTrayIconForToday(HWND hwnd)
{
    struct nep_date bs;
    wchar_t tooltip[128];

    if (GetTodayBs(&bs)) {
        /* Saturday is Nepal's weekly holiday - highlight it in red like a
           printed calendar. Other days switch between dark/light text so
           they stay visible against either taskbar theme. */
        COLORREF color;
        if (strcmp(bs.day, "Saturday") == 0) {
            color = RGB(220, 30, 30);
        } else {
            color = IsSystemDarkTheme() ? RGB(235, 235, 235) : RGB(40, 40, 40);
        }
        HICON newIcon = RenderDayIcon(bs.date, color);
        HICON oldIcon = g_currentIcon;
        g_currentIcon = newIcon;

        swprintf(tooltip, 128, L"%hs, %s %d, %d BS",
                 bs.day, g_bsMonthNames[bs.month - 1], bs.date, bs.year);

        if (oldIcon) DestroyIcon(oldIcon);
    } else {
        SYSTEMTIME st;
        GetLocalTime(&st);
        g_currentIcon = RenderDayIcon(st.wDay, RGB(120, 120, 120));
        wcscpy(tooltip, L"BS date unavailable (outside nepdate's supported range)");
    }

    g_nid.hIcon = g_currentIcon;
    wcsncpy(g_nid.szTip, tooltip, ARRAYSIZE(g_nid.szTip) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void AddTrayIcon(HWND hwnd)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = ID_TRAY_ICON;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, APP_TITLE);

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    UpdateTrayIconForToday(hwnd);
}

static void RemoveTrayIcon(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_currentIcon) DestroyIcon(g_currentIcon);
}

/* ---------------------------------------------------------------------
 * Main window procedure
 * --------------------------------------------------------------------- */
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        AddTrayIcon(hwnd);
        SetTimer(hwnd, ID_TIMER_REFRESH, REFRESH_INTERVAL_MS, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == ID_TIMER_REFRESH) {
            UpdateTrayIconForToday(hwnd);
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_MENU_EXIT, L"Exit");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(menu);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_MENU_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ---------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"NepaliTrayCalendar_SingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = WND_CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, WND_CLASS_NAME, APP_TITLE, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
        NULL, NULL, hInstance, NULL);
    (void)hwnd; /* never shown - it only exists to host the tray icon and message loop */

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (mutex) CloseHandle(mutex);
    return (int)msg.wParam;
}
