// SPDX-License-Identifier: Apache-2.0
#include "window_chrome.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sw
{
namespace
{
QIcon captionIcon()
{
    QIcon result;
    for (int size : {16, 20, 24, 32, 48, 64})
    {
        QPixmap image(size, size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#293e35"));
        painter.drawRoundedRect(QRectF(0, 0, size, size), size / 5.0, size / 5.0);
        QIcon(":/brand/lightning.svg").paint(&painter, QRect(1, 1, size - 2, size - 2));
        result.addPixmap(image);
    }
    return result;
}

class WindowChrome : public QObject
{
public:
    explicit WindowChrome(QObject *parent) : QObject(parent)
    {
        setObjectName("workbenchWindowChrome");
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        qApp->installEventFilter(this);
    }

    const QIcon windowIcon = captionIcon();
    Theme theme;
    BackgroundSpec specification;
    QImage image;
    QList<QPointer<BackgroundSurface>> captions;

    void refresh(BackgroundSurface *caption)
    {
        auto host = caption->parentWidget();
        host->setWindowIcon(windowIcon);
        const auto handle = host->windowHandle();
        int height = handle ? handle->safeAreaMargins().top() : 0;
#ifdef Q_OS_WIN
        if (handle)
        {
            // Qt's safe-area API can report the 96-DPI caption height on a
            // scaled monitor. Reserve the actual native height, in logical px.
            const auto native = reinterpret_cast<HWND>(handle->winId());
            const UINT dpi = GetDpiForWindow(native);
            const int pixels = GetSystemMetricsForDpi(SM_CYCAPTION, dpi)
                + GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi)
                + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            height = qMax(height, qCeil(pixels / handle->devicePixelRatio()));
        }
#endif
        auto margins = host->property("workbenchFrameMargins").value<QMargins>();
        margins.setTop(qMax(margins.top(), height));
        if (host->contentsMargins() != margins) { host->setContentsMargins(margins); }
        caption->setGeometry(0, 0, host->width(), host->isFullScreen() ? 0 : height);
        caption->base = theme.background;
        caption->background.set(specification, image);
        caption->setVisible(height > 0 && !host->isFullScreen());
        caption->raise();
        caption->update();
    }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto host = qobject_cast<QWidget *>(object);
        if (!host || !host->isWindow()) { return false; }
        const auto type = host->windowType();
        if (type != Qt::Window && type != Qt::Dialog && type != Qt::Tool) { return false; }
        if (event->type() == QEvent::Polish && !host->property("workbenchChrome").toBool())
        {
            // Keep native resize/move/system-menu handling and button flags.
            // Qt reserves its safe area for layouts while painting the caption
            // text and buttons over our background.
            host->setProperty("workbenchChrome", true);
            host->setProperty("workbenchFrameMargins", QVariant::fromValue(host->contentsMargins()));
            host->setWindowFlags(host->windowFlags() | Qt::CustomizeWindowHint | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
            host->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea);
            auto caption = new BackgroundSurface(host);
            caption->setObjectName("windowCaptionBackground");
            caption->windowAligned = true;
            caption->setAttribute(Qt::WA_TransparentForMouseEvents);
            caption->setFocusPolicy(Qt::NoFocus);
            captions.append(caption);
            refresh(caption);
        }
        if (host->property("workbenchChrome").toBool()
            && (event->type() == QEvent::Show || event->type() == QEvent::Resize
                || event->type() == QEvent::WindowStateChange || event->type() == QEvent::SafeAreaMarginsChange
                || event->type() == QEvent::DevicePixelRatioChange))
        {
            if (auto caption = host->findChild<BackgroundSurface *>("windowCaptionBackground", Qt::FindDirectChildrenOnly))
            {
                refresh(caption);
            }
        }
        return false;
    }
};

WindowChrome *controller()
{
    // QObject ownership and QPointers also cover dialogs destroyed during exec().
    const auto existing = qApp->findChild<QObject *>("workbenchWindowChrome", Qt::FindDirectChildrenOnly);
    if (existing) { return static_cast<WindowChrome *>(existing); }
    return new WindowChrome(qApp);
}
}

void installWindowChrome()
{
    controller();
}

void updateWindowChrome(const Theme &theme, const BackgroundSpec &spec, const QImage &image)
{
    auto frames = controller();
    frames->theme = theme;
    frames->specification = spec;
    frames->image = image;
    qApp->styleHints()->setColorScheme(theme.dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    frames->captions.removeIf([](const auto &caption) { return caption.isNull(); });
    for (auto caption : frames->captions)
    {
        frames->refresh(caption);
    }
}
}
