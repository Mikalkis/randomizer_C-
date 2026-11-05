// © Mikalkis
// Генератор случайных чисел — порт tkinter-версии random_generator.py на WinAPI.
// Функционал сохранён: диапазон Min/Max, режим «Без повторов», история из
// 5 последних чисел, кнопка «Очистить», центрирование окна, те же сообщения.

#include <windows.h>

#include <cerrno>
#include <climits>
#include <cwchar>
#include <deque>
#include <random>
#include <set>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"RandApp";
constexpr wchar_t kWindowTitle[] = L"Rand";
constexpr int kHistoryCapacity = 5;
// Клиентская область оригинального окна tkinter: 210x200.
constexpr int kClientWidth = 210;
constexpr int kClientHeight = 200;

constexpr int kIdMinEdit = 101;
constexpr int kIdMaxEdit = 102;
constexpr int kIdGenerate = 103;
constexpr int kIdClear = 104;
constexpr int kIdNoRepeat = 105;
constexpr int kIdResult = 106;
constexpr int kIdHistoryCell0 = 110; // ячейки истории: 110..114

// lightyellow из оригинала — подсветка последнего числа в истории.
constexpr COLORREF kHighlight = RGB(255, 255, 224);

// Состояние генератора: использованные числа (для режима «Без повторов»)
// и последние выпавшие значения.
struct AppState {
    std::mt19937 rng{std::random_device{}()};
    std::set<int> used_numbers;
    std::deque<int> history;
};

HWND g_window{};
int g_dpi = 96; // системный DPI, уточняется в WinMain до создания окна

// Перевод "базовых пикселей" (при 96 DPI) в реальные на текущем DPI.
int scaled(int pixels) {
    return MulDiv(pixels, g_dpi, 96);
}

HWND g_min_edit{};
HWND g_max_edit{};
HWND g_generate_btn{};
HWND g_clear_btn{};
HWND g_no_repeat{};
HWND g_result{};
HWND g_history_cells[kHistoryCapacity]{};
HFONT g_font{};      // системный шрифт сообщений для всех надписей
HFONT g_big_font{};  // Arial 30pt для текущего числа
HBRUSH g_highlight_brush{CreateSolidBrush(kHighlight)};

AppState g_state;
// Индекс подсвеченной ячейки истории; -1 — ничего не подсвечено.
int g_highlight = -1;

int history_index(HWND control) {
    for (int i = 0; i < kHistoryCapacity; ++i)
        if (g_history_cells[i] == control)
            return i;
    return -1;
}

void set_text(HWND control, const std::wstring& text) {
    SetWindowTextW(control, text.c_str());
}

// Разбор целого из поля ввода. Единственное место чтения внешних данных.
bool try_parse_edit(HWND edit, int& out) {
    wchar_t buffer[64]{};
    GetWindowTextW(edit, buffer, 64);
    wchar_t* end = nullptr;
    errno = 0;
    const long value = std::wcstol(buffer, &end, 10);
    if (end == buffer || *end != L'\0' || errno == ERANGE ||
        value < INT_MIN || value > INT_MAX)
        return false;
    out = static_cast<int>(value);
    return true;
}

void refresh_history() {
    for (HWND cell : g_history_cells)
        set_text(cell, L"");
    int index = 0;
    for (int number : g_state.history)
        set_text(g_history_cells[index++], std::to_wstring(number));
    g_highlight = static_cast<int>(g_state.history.size()) - 1;
    InvalidateRect(g_window, nullptr, TRUE);
}

bool no_repeat_checked() {
    return SendMessageW(g_no_repeat, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void update_button_state() {
    if (!no_repeat_checked()) {
        EnableWindow(g_generate_btn, TRUE);
        return;
    }
    int min_value = 0, max_value = 0;
    if (!try_parse_edit(g_min_edit, min_value) ||
        !try_parse_edit(g_max_edit, max_value))
        return;
    const long long total =
        static_cast<long long>(max_value) - min_value + 1;
    EnableWindow(g_generate_btn,
                 static_cast<long long>(g_state.used_numbers.size()) < total
                     ? TRUE
                     : FALSE);
}

void generate() {
    int min_value = 0, max_value = 0;
    if (!try_parse_edit(g_min_edit, min_value) ||
        !try_parse_edit(g_max_edit, max_value)) {
        MessageBoxW(g_window, L"Введите целые числа", L"Ошибка", MB_ICONERROR);
        return;
    }
    if (min_value > max_value) {
        MessageBoxW(g_window, L"Минимум больше максимума", L"Ошибка",
                    MB_ICONERROR);
        return;
    }

    int number = min_value;
    if (no_repeat_checked()) {
        // Свободные значения считаем напрямую по диапазону: размер `used`
        // может превышать текущий диапазон, если пользователь его сузил.
        long long unused = 0;
        for (int value = min_value; value <= max_value; ++value)
            if (g_state.used_numbers.count(value) == 0)
                ++unused;
        if (unused == 0) {
            EnableWindow(g_generate_btn, FALSE);
            MessageBoxW(g_window, L"Все числа использованы", L"Готово",
                        MB_ICONINFORMATION);
            return;
        }
        // Равновероятный выбор среди неиспользованных: пропускаем `skip`
        // свободных значений диапазона по возрастанию.
        std::uniform_int_distribution<long long> pick(0, unused - 1);
        long long skip = pick(g_state.rng);
        // Счётчик long long: при max_value == INT_MAX инкремент int был бы UB.
        for (long long value = min_value; value <= max_value; ++value) {
            if (g_state.used_numbers.count(static_cast<int>(value)) != 0)
                continue;
            if (skip == 0) {
                number = static_cast<int>(value);
                break;
            }
            --skip;
        }
        g_state.used_numbers.insert(number);
    } else {
        std::uniform_int_distribution<int> pick(min_value, max_value);
        number = pick(g_state.rng);
    }

    set_text(g_result, std::to_wstring(number));

    g_state.history.push_back(number);
    if (g_state.history.size() > static_cast<std::size_t>(kHistoryCapacity))
        g_state.history.pop_front();
    refresh_history();

    update_button_state();
}

void clear_all() {
    g_state.used_numbers.clear();
    g_state.history.clear();
    set_text(g_result, L"—");
    refresh_history();
    EnableWindow(g_generate_btn, TRUE);
}

HWND create_control(HWND parent, const wchar_t* class_name,
                    const wchar_t* text, DWORD style, DWORD ex_style, int x,
                    int y, int w, int h, int id) {
    return CreateWindowExW(
        ex_style, class_name, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
}

void apply_font(HWND control, HFONT font) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HFONT create_message_font() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);
    return CreateFontIndirectW(&metrics.lfMessageFont);
}

HFONT create_big_font() {
    return CreateFontW(-MulDiv(30, g_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                       L"Arial");
}

void create_controls(HWND parent) {
    const HWND min_label = create_control(parent, L"STATIC", L"Min", SS_RIGHT,
                                          0, scaled(8), scaled(11), scaled(34),
                                          scaled(18), 0);
    g_min_edit = create_control(parent, L"EDIT", L"1", WS_TABSTOP | WS_BORDER, 0,
                                scaled(48), scaled(9), scaled(50), scaled(22),
                                kIdMinEdit);
    g_generate_btn = create_control(
        parent, L"BUTTON", L"Пуск", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, scaled(108),
        scaled(8), scaled(92), scaled(24), kIdGenerate);
    const HWND max_label = create_control(
        parent, L"STATIC", L"Max", SS_RIGHT, 0, scaled(8), scaled(39), scaled(34),
        scaled(18), 0);
    g_max_edit = create_control(parent, L"EDIT", L"6", WS_TABSTOP | WS_BORDER, 0,
                                scaled(48), scaled(37), scaled(50), scaled(22),
                                kIdMaxEdit);
    g_clear_btn = create_control(parent, L"BUTTON", L"Очистить",
                                 WS_TABSTOP | BS_PUSHBUTTON, 0, scaled(108),
                                 scaled(36), scaled(92), scaled(24), kIdClear);
    g_no_repeat = create_control(
        parent, L"BUTTON", L"Без повторов", WS_TABSTOP | BS_AUTOCHECKBOX, 0,
        scaled(8), scaled(66), scaled(194), scaled(18), kIdNoRepeat);
    g_result = create_control(parent, L"STATIC", L"—",
                              SS_CENTER | SS_CENTERIMAGE | SS_NOPREFIX, 0,
                              scaled(8), scaled(90), scaled(194), scaled(44),
                              kIdResult);
    const HWND history_label = create_control(
        parent, L"STATIC", L"История:", SS_CENTER, 0, scaled(8), scaled(140),
        scaled(194), scaled(18), 0);

    for (int i = 0; i < kHistoryCapacity; ++i) {
        // Ячейка 34 + отступ 4.
        const int x = scaled(12 + i * 38);
        g_history_cells[i] = create_control(
            parent, L"STATIC", L"", SS_CENTER | SS_CENTERIMAGE,
            WS_EX_STATICEDGE, x, scaled(162), scaled(34), scaled(22),
            kIdHistoryCell0 + i);
    }

    for (HWND control : {g_min_edit, g_max_edit, g_generate_btn, g_clear_btn,
                         g_no_repeat, g_result})
        apply_font(control, g_font);
    for (HWND cell : g_history_cells)
        apply_font(cell, g_font);
    apply_font(g_result, g_big_font);
    apply_font(min_label, g_font);
    apply_font(max_label, g_font);
    apply_font(history_label, g_font);
}

LRESULT CALLBACK wnd_proc(HWND window, UINT message, WPARAM w_param,
                          LPARAM l_param) {
    switch (message) {
        case WM_CTLCOLORSTATIC: {
            // Подсветка последнего числа в истории.
            const int index = history_index(reinterpret_cast<HWND>(l_param));
            if (index >= 0 && index == g_highlight) {
                SetBkColor(reinterpret_cast<HDC>(w_param), kHighlight);
                return reinterpret_cast<LRESULT>(g_highlight_brush);
            }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(w_param)) {
                case kIdGenerate:
                    generate();
                    return 0;
                case kIdClear:
                    clear_all();
                    return 0;
                case kIdNoRepeat:
                    if (HIWORD(w_param) == BN_CLICKED)
                        update_button_state();
                    return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = wnd_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // Иконка из app.rc; если не загрузилась — системная.
    HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!icon)
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hIcon = icon;
    window_class.hIconSm = icon;
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    window_class.lpszClassName = kWindowClass;
    RegisterClassExW(&window_class);

    // resizable(False, False) из оригинала.
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    // Реальный системный DPI — до всех расчётов размеров.
    {
        HDC screen = GetDC(nullptr);
        g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
        ReleaseDC(nullptr, screen);
        if (g_dpi < 96)
            g_dpi = 96; // защита от нестандартных значений (логические экраны)
    }
    RECT rect{0, 0, scaled(kClientWidth), scaled(kClientHeight)};
    AdjustWindowRect(&rect, style, FALSE);

    g_window = CreateWindowExW(
        0, kWindowClass, kWindowTitle, style, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
        instance, nullptr);

    g_font = create_message_font();
    g_big_font = create_big_font();
    create_controls(g_window);

    // Центрирование окна, как в оригинале (winfo_screenwidth/height).
    RECT window_rect{};
    GetWindowRect(g_window, &window_rect);
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    SetWindowPos(g_window, nullptr,
                 (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - height) / 2, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(g_window, show);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // Tab/Enter между элементами, как в tkinter.
        if (!IsDialogMessageW(g_window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}
