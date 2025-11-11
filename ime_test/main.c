/* MIT License
 * 
 * Copyright (c) 2025 Bernhard Kölbl
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <imm.h>

WCHAR *input_text = NULL, *display_text = NULL;
HWND status_bar = NULL;
HFONT font = NULL;

void draw_text(HWND hwnd, const WCHAR *text)
{
    PAINTSTRUCT ps;
    HFONT old_font;
    UINT format;
    RECT rc;
    HDC hdc;

    format = DT_LEFT | DT_EXPANDTABS | DT_WORDBREAK;

    GetClientRect(hwnd, &rc);

    rc.top      += 50;
    rc.left     += 50;
    rc.bottom   -= 50;
    rc.right    -= 50;

    hdc = BeginPaint(hwnd, &ps);

    FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW + 1));

    old_font = SelectObject(hdc, font);

    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text, -1, &rc, format);

    SelectObject(hdc, old_font);

    EndPaint(hwnd, &ps);
}

void set_candidate_window_pos(HWND hwnd)
{
    HIMC himc = ImmGetContext(hwnd);

    if (himc)
    {
        CANDIDATEFORM cdf = { 0 };
        POINT pt = { 0 };
        pt.x = 100;
        pt.y = 100;

        cdf.dwStyle = CFS_CANDIDATEPOS;
        cdf.ptCurrentPos = pt;

        printf("candidate pos x:%ld y:%ld\n", cdf.ptCurrentPos.x, cdf.ptCurrentPos.y);

        ImmSetCandidateWindow(himc, &cdf);

        ImmReleaseContext(hwnd, himc);
    }
}

void resize_status_bar(HWND hwnd, int width, int height)
{
    RECT rc, rc2;

    GetClientRect(hwnd, &rc2);
    SendMessageW(status_bar, WM_SIZE, 0, 0);
    GetWindowRect(status_bar, &rc);
    SetWindowPos(status_bar, NULL, 0, rc2.bottom - (rc.bottom - rc.top) , rc2.right, 0, SWP_NOZORDER | SWP_NOSIZE);
    InvalidateRect(hwnd, 0, TRUE);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    printf("WndProc message %#x, wParam %#llx, lParam %#llx\n", msg, wParam, lParam);

    switch (msg)
    {

    case WM_CREATE:
    {
        printf("WM_CREATE\n");

        HIMC himc = ImmAssociateContext(hwnd, ImmCreateContext());
        if (himc)
        {
            ImmSetOpenStatus(himc, TRUE);
            ImmReleaseContext(hwnd, himc);
        }
        else MessageBoxW(hwnd, L"Error", L"Failed to create IME context!", MB_ICONERROR);

        input_text = wcsdup(L"Enter Text (IME)");
        display_text = wcsdup(input_text);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SETFONT:
        font = (HFONT)wParam;
        return 0;

    case WM_PAINT:
        draw_text(hwnd, display_text);
        return 0;

    case WM_SIZE:
        resize_status_bar(hwnd, wParam, lParam);
        break;

    case WM_CHAR:
    {
        printf("WM_CHAR %C\n", (WCHAR)wParam);
        DWORD old_len = wcslen(input_text), new_len = 0;

        if (wParam == 0x8) /* backspace */
            new_len = old_len;
        else
            new_len = old_len + 1;

        WCHAR *buffer = calloc(new_len + 1, sizeof(WCHAR));

        if (buffer)
        {
            printf("%ls\n", buffer);

            if (new_len > old_len)
            {
                memcpy(buffer, input_text, old_len * sizeof(WCHAR));
                buffer[old_len] = (WCHAR)wParam;
            }
            else
            {
                memcpy(buffer, input_text, (new_len ? new_len - 1 : 0) * sizeof(WCHAR));
            }

            printf("%ls\n", buffer);

            free(input_text);
            input_text = buffer;

            free(display_text);
            display_text = wcsdup(input_text);

            InvalidateRect(hwnd, 0, TRUE);
        }

        return 0;
    }

    case WM_IME_COMPOSITION:
        printf("WM_IME_COMPOSITION, %#llx\n", lParam);

        if (lParam & GCS_RESULTSTR)
        {
            HIMC himc = ImmGetContext(hwnd);
            DWORD len = ImmGetCompositionStringW(himc, GCS_RESULTSTR, NULL, 0);
            WCHAR *buffer = calloc(len + sizeof(WCHAR), 1);

            printf("GCS_RESULTSTR\n");

            if (buffer)
            {
                DWORD text_len, old_len;

                ImmGetCompositionStringW(himc, GCS_RESULTSTR, buffer, len);

                old_len = wcslen(input_text);
                text_len = old_len + (len / sizeof(WCHAR)) + 1;
                input_text = realloc(input_text, text_len * sizeof(WCHAR));
                memcpy(input_text + old_len, buffer, len);
                input_text[text_len - 1] = L'\0';

                free(display_text);
                display_text = wcsdup(input_text);

                printf("input text %ls \ndisplay text %ls\n", input_text, display_text);

                free(buffer);
            }

            ImmReleaseContext(hwnd, himc);

            InvalidateRect(hwnd, 0, TRUE);
            return 0;
        }
        else if (lParam & GCS_COMPSTR)
        {
            HIMC himc = ImmGetContext(hwnd);
            DWORD len = ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
            WCHAR *buffer = calloc(len + sizeof(WCHAR), 1);

            printf("GCS_COMPSTR\n");

            if (buffer)
            {
                INT text_len = wcslen(input_text);
                ImmGetCompositionStringW(himc, GCS_COMPSTR, buffer, len);

                free(display_text);
                text_len += (len / sizeof(WCHAR)) + 1;
                display_text = calloc(text_len, sizeof(WCHAR));
                swprintf(display_text, text_len, L"%s%s", input_text, buffer);

                printf("input text %ls \ndisplay text %ls\n", input_text, display_text);

                free(buffer);
            }

            ImmReleaseContext(hwnd, himc);

            InvalidateRect(hwnd, 0, TRUE);

            return 0;
        }
        else
        {
            printf("Other GCS\n");
        }
        break;

        case WM_IME_STARTCOMPOSITION:
        {
            printf("WM_IME_STARTCOMPOSITION\n");
            set_candidate_window_pos(hwnd);
            return 0;
        }

        case WM_IME_CONTROL:
            printf("WM_IME_CONTROL\n");
            break;

        case WM_IME_NOTIFY:
        {
            printf("WM_IME_NOTIFY\n");
            if (wParam == IMN_SETOPENSTATUS)
            {
                HIMC himc = ImmGetContext(hwnd);
                BOOL on = ImmGetOpenStatus(himc);

                printf("IME enabled %u\n", on);

                if (on)
                    SendMessageW(status_bar, SB_SETTEXT, 0, (LPARAM)L"あ");
                else
                    SendMessageW(status_bar, SB_SETTEXT, 0, (LPARAM)L"A");


                ImmReleaseContext(hwnd, himc);
            }
            else if (wParam == IMN_SETCONVERSIONMODE)
            {
                WCHAR buf[20];
                HIMC himc = ImmGetContext(hwnd);
                DWORD mode1, mode2;

                ImmGetConversionStatus(himc, &mode1, &mode2);
                printf("IME status changed to conversion %#lx sentence %#lx\n", mode1, mode2);
                ImmReleaseContext(hwnd, himc);

                swprintf(buf, sizeof(buf), L"%#lx", mode1);
                SendMessageW(status_bar, SB_SETTEXT, 1, (LPARAM)buf);

                swprintf(buf, sizeof(buf), L"%#lx", mode2);
                SendMessageW(status_bar, SB_SETTEXT, 2, (LPARAM)buf);
            }
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *pCmdLine, int nCmdShow)
{
    const INT parts[] = { 150, 300, 450, -1 };
    const WCHAR *class_name  = L"IMETestClass";
    const WCHAR *window_title = L"IME TEST";
    WNDCLASSEXW wc = { 0 };
    HWND hwnd = NULL;
    MSG msg = { 0 };

    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = class_name;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.cbSize        = sizeof(WNDCLASSEXW);

    RegisterClassExW(&wc);

    hwnd = CreateWindowExW(0, class_name, window_title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    status_bar = CreateWindowExW(0, STATUSCLASSNAMEW, NULL, WS_VISIBLE | WS_CHILD,
                                 0, 0, 0, 0, hwnd, NULL, hInstance, NULL);

    SendMessageW(status_bar, SB_SETPARTS, sizeof(parts), (LPARAM)parts);
    SendMessageW(status_bar, SB_SETTEXT, 0, (LPARAM)L"A");
    SendMessageW(status_bar, SB_SETTEXT, 1, (LPARAM)L"?");
    SendMessageW(status_bar, SB_SETTEXT, 2, (LPARAM)L"?");

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

