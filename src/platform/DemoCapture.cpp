#include "DemoCapture.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLibrary>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <tlhelp32.h>
#include <windows.h>
#else
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef Status
#undef None
#endif
namespace {
QSet<qint64> descendants(qint64 root) {
    QSet<qint64> result;
    if (root <= 0)
        return result;
    result.insert(root);
    QMap<qint64, qint64> parents;
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (snapshot != INVALID_HANDLE_VALUE) {
        if (Process32First(snapshot, &entry))
            do {
                parents[entry.th32ProcessID] = entry.th32ParentProcessID;
            } while (Process32Next(snapshot, &entry));
        CloseHandle(snapshot);
    }
#else
    for (auto name : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        qint64 pid = name.toLongLong(&ok);
        if (!ok)
            continue;
        QFile file("/proc/" + name + "/status");
        if (file.open(QIODevice::ReadOnly)) {
            auto match = QRegularExpression("PPid:\\s*(\\d+)").match(QString::fromLatin1(file.readAll()));
            if (match.hasMatch())
                parents[pid] = match.captured(1).toLongLong();
        }
    }
#endif
    bool more = true;
    while (more) {
        more = false;
        for (auto it = parents.begin(); it != parents.end(); ++it)
            if (result.contains(it.value()) && !result.contains(it.key())) {
                result.insert(it.key());
                more = true;
            }
    }
    return result;
}
#ifdef Q_OS_WIN
struct WindowImages {
    QSet<qint64> processes;
    QList<QImage> images;
};
BOOL CALLBACK collect(HWND window, LPARAM argument) {
    auto data = reinterpret_cast<WindowImages *>(argument);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!data->processes.contains(pid) || !IsWindowVisible(window) || IsIconic(window))
        return TRUE;
    RECT r{};
    GetClientRect(window, &r);
    int width = r.right, height = r.bottom;
    if (width < 2 || height < 2 || width > 4096 || height > 4096)
        return TRUE;
    HDC dc = GetDC(window), memory = CreateCompatibleDC(dc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap) {
        auto old = SelectObject(memory, bitmap);
        PatBlt(memory, 0, 0, width, height, BLACKNESS);
        if (PrintWindow(window, memory, 1 | 2) && bits)
            data->images.append(
                QImage(static_cast<uchar *>(bits), width, height, QImage::Format_RGB32).copy());
        SelectObject(memory, old);
        DeleteObject(bitmap);
    }
    DeleteDC(memory);
    ReleaseDC(window, dc);
    return TRUE;
}
#else
// Resolve the X server's actual client PID; Tk does not set _NET_WM_PID.
struct ClientIdSpec {
    XID client;
    unsigned int mask;
};
struct ClientIdValue {
    ClientIdSpec spec;
    long length;
    void *value;
};
qint64 windowProcess(Display *display, Window window) {
    static QLibrary resource("libXRes.so.1");
    auto query = reinterpret_cast<int (*)(Display *, long, ClientIdSpec *, long *, ClientIdValue **)>(
        resource.resolve("XResQueryClientIds"));
    auto getPid = reinterpret_cast<int (*)(ClientIdValue *)>(resource.resolve("XResGetClientPid"));
    auto destroy =
        reinterpret_cast<void (*)(long, ClientIdValue *)>(resource.resolve("XResClientIdsDestroy"));
    if (!query || !getPid || !destroy)
        return -1;
    ClientIdSpec spec{window, 2};
    long count = 0;
    ClientIdValue *ids = nullptr;
    query(display, 1, &spec, &count, &ids);
    qint64 pid = -1;
    if (ids) {
        for (long i = 0; i < count; ++i) {
            int value = getPid(ids + i);
            if (value > 0)
                pid = value;
        }
        destroy(count, ids);
    }
    return pid;
}
struct CaptureState {
    Display *display = nullptr;
    QMap<Window, qint64> redirected;
    ~CaptureState() {
        if (display)
            XCloseDisplay(display);
    }
};
CaptureState &captureState() {
    static CaptureState state;
    return state;
}
int ignoreXError(Display *, XErrorEvent *) {
    return 0;
}
QList<QImage> captureChildren(const QSet<qint64> &processes) {
    QList<QImage> images;
    if (processes.isEmpty())
        return images;
    static QLibrary composite("libXcomposite.so.1");
    auto redirect =
        reinterpret_cast<void (*)(Display *, Window, int)>(composite.resolve("XCompositeRedirectWindow"));
    using NamePixmap = Pixmap (*)(Display *, Window);
    auto namePixmap = reinterpret_cast<NamePixmap>(composite.resolve("XCompositeNameWindowPixmap"));
    if (!namePixmap)
        return images;
    auto &state = captureState();
    if (!state.display)
        state.display = XOpenDisplay(nullptr);
    Display *display = state.display;
    if (!display)
        return images;
    auto previous = XSetErrorHandler(ignoreXError);
    Atom actual;
    int format;
    unsigned long count = 0, remaining;
    unsigned char *list = nullptr;
    XGetWindowProperty(display, DefaultRootWindow(display), XInternAtom(display, "_NET_CLIENT_LIST", False),
                       0, 4096, False, AnyPropertyType, &actual, &format, &count, &remaining, &list);
    if (list && format == 32)
        for (unsigned long i = 0; i < count && images.size() < 8; ++i) {
            Window window = reinterpret_cast<Window *>(list)[i];
            auto pid = windowProcess(display, window);
            if (!processes.contains(pid))
                continue;
            XWindowAttributes a{};
            if (!XGetWindowAttributes(display, window, &a) || a.map_state != IsViewable || a.width < 2 ||
                a.height < 2 || a.width > 4096 || a.height > 4096)
                continue;
            if (redirect && state.redirected.value(window, -1) != pid) {
                redirect(display, window, 0);
                XSync(display, False);
                state.redirected[window] = pid;
                XClearArea(display, window, 0, 0, 0, 0, True);
                XFlush(display);
            }
            Pixmap pixmap = namePixmap(display, window);
            XSync(display, False);
            if (!pixmap)
                continue;
            XImage *source = XGetImage(display, pixmap, 0, 0, a.width, a.height, AllPlanes, ZPixmap);
            if (source) {
                QImage image(a.width, a.height, QImage::Format_RGB32);
                for (int y = 0; y < a.height; ++y)
                    for (int x = 0; x < a.width; ++x) {
                        unsigned long pixel = XGetPixel(source, x, y);
                        auto component = [pixel](unsigned long mask) {
                            if (!mask)
                                return 0;
                            int shift = 0;
                            while (!(mask & 1)) {
                                mask >>= 1;
                                ++shift;
                            }
                            return int(((pixel >> shift) & mask) * 255 / mask);
                        };
                        image.setPixel(
                            x, y,
                            qRgb(component(source->red_mask ? source->red_mask : a.visual->red_mask),
                                 component(source->green_mask ? source->green_mask : a.visual->green_mask),
                                 component(source->blue_mask ? source->blue_mask : a.visual->blue_mask)));
                    }
                images.append(image);
                XDestroyImage(source);
            }
            XFreePixmap(display, pixmap);
        }
    if (list)
        XFree(list);
    XSync(display, False);
    XSetErrorHandler(previous);
    return images;
}
#endif
} // namespace
void DemoCapture::reset() {
#ifndef Q_OS_WIN
    auto &state = captureState();
    if (state.display)
        XCloseDisplay(state.display);
    state.display = nullptr;
    state.redirected.clear();
#endif
}
QImage DemoCapture::capture(QQuickWindow *ide, qint64 processId) {
    if (!ide || !ide->isVisible())
        return {};
    auto main = ide->grabWindow();
    if (main.isNull())
        return {};
    // Own Qt surfaces are rendered directly; never copy any desktop pixels.
    {
        QPainter p(&main);
        for (auto window : QGuiApplication::topLevelWindows()) {
            auto quick = qobject_cast<QQuickWindow *>(window);
            if (!quick || quick == ide || !quick->isVisible() || quick->property("demoViewer").toBool())
                continue;
            if (quick->type() == Qt::ToolTip || quick->type() == Qt::Popup) {
                auto image = quick->grabWindow();
                if (!image.isNull())
                    p.drawImage((quick->position() - ide->position()) * main.devicePixelRatio(), image);
            }
        }
    }
    auto processes = descendants(processId);
    QList<QImage> children;
#ifdef Q_OS_WIN
    WindowImages data{processes, {}};
    EnumWindows(collect, reinterpret_cast<LPARAM>(&data));
    children = data.images;
#else
    children = captureChildren(processes);
#endif
    main.setDevicePixelRatio(1);
    if (main.width() > 2560 || main.height() > 1600)
        main = main.scaled(2560, 1600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (children.isEmpty())
        return main;
    QImage result(main.width() + 420, qMax(main.height(), int(qMin(children.size(), 3)) * 300),
                  QImage::Format_RGB32);
    result.fill(QColor("#202124"));
    QPainter p(&result);
    p.drawImage(0, 0, main);
    int y = 0;
    for (auto child : children) {
        if (y >= 900)
            break;
        auto image = child.scaled(408, 272, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.setPen(Qt::white);
        p.drawText(main.width() + 6, y + 18, QStringLiteral("Окно запущенной программы"));
        p.drawImage(main.width() + 6, y + 24, image);
        y += 300;
    }
    return result;
}
