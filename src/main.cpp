#include <QApplication>
#include <QDir>
#include <QIcon>
#include "mainwindow.h"
#include "theme.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
// Ask Windows 11 for a dark, rounded title bar that matches the new theme.
// Uses a runtime lookup so the app still builds and runs on Windows 10.
void applyWindowChrome(HWND hwnd) {
    if (!hwnd) return;
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    using DwmSetAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto set = reinterpret_cast<DwmSetAttribute>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (set) {
        const BOOL dark = TRUE;
        set(hwnd, 20, &dark, sizeof(dark));          // DWMWA_USE_IMMERSIVE_DARK_MODE
        const DWORD rounded = 2;                     // DWMWCP_ROUND
        set(hwnd, 33, &rounded, sizeof(rounded));    // DWMWA_WINDOW_CORNER_PREFERENCE
        const COLORREF caption = RGB(12, 11, 22);
        set(hwnd, 35, &caption, sizeof(caption));    // DWMWA_CAPTION_COLOR
        const COLORREF border = RGB(40, 36, 62);
        set(hwnd, 34, &border, sizeof(border));      // DWMWA_BORDER_COLOR
        const COLORREF text = RGB(232, 232, 244);
        set(hwnd, 36, &text, sizeof(text));          // DWMWA_TEXT_COLOR
    }
    FreeLibrary(dwm);
}
} // namespace
#endif

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("OpenClip");
    app.setWindowIcon(QIcon(":/icon.png"));
    app.setStyle("Fusion");
    app.setStyleSheet(appStyleSheet());
    // Look for a bundled ffmpeg/ffprobe next to the program first.
    const QString extra = QDir::toNativeSeparators(app.applicationDirPath());
    const QString path = extra + QDir::listSeparator() + qEnvironmentVariable("PATH");
    qputenv("PATH", path.toLocal8Bit());
    MainWindow w;
    w.show();
#ifdef Q_OS_WIN
    applyWindowChrome(reinterpret_cast<HWND>(w.winId()));
#endif
    return app.exec();
}
