#include "WindowChrome.h"
#include <QWindow>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#endif
void Platform::applyWindowChrome(QWindow *window) {
#ifdef Q_OS_WIN
    if (!window) return;
    BOOL enabled = TRUE;
    HWND handle = reinterpret_cast<HWND>(window->winId());
    if (FAILED(DwmSetWindowAttribute(handle, 20, &enabled, sizeof(enabled))))
        DwmSetWindowAttribute(handle, 19, &enabled, sizeof(enabled));
#else
    Q_UNUSED(window);
#endif
}
