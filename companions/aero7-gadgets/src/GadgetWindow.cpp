#include "GadgetWindow.h"

#include "GadgetManager.h"
#include "GadgetOptionsDialog.h"
#include "RuntimeServices.h"

#include <KWindowSystem>
#include <LayerShellQt/window.h>

#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEnterEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

namespace {
constexpr int controlsWidth = 29;

QString menuStyle()
{
    return QStringLiteral(
        "QMenu { background:#f5f5f5; color:#111; border:1px solid #777; padding:2px; font:9pt 'Segoe UI'; }"
        "QMenu::item { padding:4px 28px 4px 25px; min-width:120px; }"
        "QMenu::item:selected { background:#dceeff; border:1px solid #7da2ce; }"
        "QMenu::separator { height:1px; background:#c9c9c9; margin:3px 5px; }"
        "QMenu::indicator { width:13px; height:13px; left:6px; }");
}
}

GadgetWindow::GadgetWindow(const GadgetDefinition &definition, GadgetState state, GadgetManager *manager)
    : QWidget(nullptr)
    , m_definition(definition)
    , m_state(std::move(state))
    , m_manager(manager)
{
    setObjectName(QStringLiteral("Aero7GadgetWindow"));
    setWindowTitle(QStringLiteral("Aero7 Gadget: ") + m_definition.name + QLatin1Char(' ') + m_state.instance);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::Tool, true);
    setWindowFlag(Qt::NoDropShadowWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);

    m_controlsAnimation.setTargetObject(this);
    m_controlsAnimation.setPropertyName("controlsOpacity");
    m_controlsAnimation.setDuration(170);
    m_controlsAnimation.setEasingCurve(QEasingCurve::InOutQuad);

    if (m_definition.id.endsWith(QStringLiteral("clock"))) {
        m_repaintTimer.setInterval(m_state.settings.value(QStringLiteral("seconds")).toBool(true) ? 1000 : 60000);
        connect(&m_repaintTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
        m_repaintTimer.start();
    } else if (m_definition.id.endsWith(QStringLiteral("calendar"))) {
        m_repaintTimer.setInterval(60000);
        connect(&m_repaintTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
        m_repaintTimer.start();
    }

    connect(KWindowSystem::self(), &KWindowSystem::showingDesktopChanged, this, [this](bool showing) {
        show();
        if (m_layerShell) {
            m_layerShell->setLayer(showing || m_state.alwaysOnTop ? LayerShellQt::Window::LayerTop
                                                                  : LayerShellQt::Window::LayerBottom);
        } else if (m_state.alwaysOnTop) {
            raise();
        } else {
            lower();
        }
    });

    m_networkTimer.setSingleShot(false);
    connect(&m_networkTimer, &QTimer::timeout, this, [this]() { updateNetworkData(false); });
    connect(&m_slideTimer, &QTimer::timeout, this, &GadgetWindow::nextSlide);
    m_slideAnimation.setDuration(420);
    m_slideAnimation.setStartValue(0.0); m_slideAnimation.setEndValue(1.0);
    connect(&m_slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) { m_data.slideTransition = value.toReal(); update(); });

    updateWindowSize();
    setWindowOpacity(qBound(20, m_state.opacity, 100) / 100.0);
    initializeRuntime();
}

QSize GadgetWindow::bodySize() const
{
    if (m_state.size == QStringLiteral("large") && m_definition.supportsLarge) {
        return m_definition.largeSize;
    }
    return m_definition.smallSize;
}

QRect GadgetWindow::bodyRect() const
{
    return QRect(QPoint(0, 0), bodySize());
}

QRect GadgetWindow::bodyGeometry() const
{
    return QRect(m_layerShell ? m_desktopPosition : pos(), bodySize());
}

GadgetState GadgetWindow::stateForSave() const
{
    GadgetState result = m_state;
    QScreen *screen = m_desktopScreen ? m_desktopScreen.data() : QGuiApplication::screenAt(bodyGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        result.monitor = screen->name();
        result.x = bodyGeometry().x() - screen->availableGeometry().x();
        result.y = bodyGeometry().y() - screen->availableGeometry().y();
    }
    return result;
}

void GadgetWindow::placeOnDesktopLayer(QScreen *screen, const QPoint &globalPosition)
{
    m_desktopScreen = screen ? screen : QGuiApplication::primaryScreen();
    m_desktopPosition = globalPosition;
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        createWinId();
        m_layerShell = LayerShellQt::Window::get(windowHandle());
        m_layerShell->setScope(QStringLiteral("aero7-desktop-gadgets"));
        m_layerShell->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                                 | LayerShellQt::Window::AnchorLeft);
        m_layerShell->setExclusiveZone(0);
        m_layerShell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
        m_layerShell->setActivateOnShow(false);
        m_layerShell->setScreen(m_desktopScreen);
        m_layerShell->setDesiredSize(size());
        applyWindowLayer();
        setDesktopPosition(globalPosition);
    } else {
        move(globalPosition);
        applyWindowLayer();
    }
}

void GadgetWindow::setDesktopPosition(const QPoint &globalPosition)
{
    if (!m_layerShell) {
        move(globalPosition);
        return;
    }
    QScreen *screen = QGuiApplication::screenAt(QRect(globalPosition, bodySize()).center());
    if (!screen) screen = m_desktopScreen ? m_desktopScreen.data() : QGuiApplication::primaryScreen();
    if (screen && screen != m_desktopScreen) {
        m_desktopScreen = screen;
        m_layerShell->setScreen(screen);
    }
    m_desktopPosition = globalPosition;
    const QRect screenGeometry = m_desktopScreen ? m_desktopScreen->geometry() : QRect(QPoint(), QSize(1024, 768));
    m_layerShell->setMargins(QMargins(globalPosition.x() - screenGeometry.x(),
                                     globalPosition.y() - screenGeometry.y(), 0, 0));
}

void GadgetWindow::notifyGeometryChanged()
{
    emit stateChanged();
}

void GadgetWindow::setControlsOpacity(qreal opacity)
{
    if (qFuzzyCompare(m_controlsOpacity, opacity)) return;
    m_controlsOpacity = opacity;
    update();
}

void GadgetWindow::updateWindowSize()
{
    resize(bodySize().width() + controlsWidth, bodySize().height());
    if (m_layerShell) m_layerShell->setDesiredSize(size());
    updateGeometry();
    applyWindowLayer();
}

void GadgetWindow::applyWindowLayer()
{
    if (m_layerShell) {
        m_layerShell->setLayer(KWindowSystem::showingDesktop() || m_state.alwaysOnTop
                                   ? LayerShellQt::Window::LayerTop : LayerShellQt::Window::LayerBottom);
        return;
    }
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) return;
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, m_state.alwaysOnTop);
    setWindowFlag(Qt::WindowStaysOnBottomHint, !m_state.alwaysOnTop);
    if (visible) show();
}

void GadgetWindow::initializeRuntime()
{
    RuntimeServices *services = RuntimeServices::instance();
    if (m_definition.id.endsWith(QStringLiteral("cpu"))) {
        connect(services, &RuntimeServices::systemUpdated, this,
                [this](double cpu, double memory, quint64 used, quint64 total) {
                    m_data.cpuPercent = cpu; m_data.memoryPercent = memory;
                    m_data.memoryUsed = used; m_data.memoryTotal = total; update();
                });
    }
    if (m_definition.id.endsWith(QStringLiteral("currency"))) {
        connect(services, &RuntimeServices::currencyUpdated, this,
                [this](const QString &base, const QString &target, double rate, const QString &updated, bool stale, const QString &error) {
                    if (base != m_state.settings.value(QStringLiteral("base")).toString(QStringLiteral("EUR"))
                        || target != m_state.settings.value(QStringLiteral("target")).toString(QStringLiteral("USD"))) return;
                    m_data.currencyRate = rate; m_data.currencyUpdated = updated; m_data.currencyStale = stale; m_data.error = error; update();
                });
        m_networkTimer.start(6 * 60 * 60 * 1000);
    }
    if (m_definition.id.endsWith(QStringLiteral("weather"))) {
        connect(services, &RuntimeServices::weatherUpdated, this,
                [this](const QString &location, double temperature, int code, const QString &updated, bool stale, const QString &error,
                       const QStringList &days, const QVector<double> &temperatures, const QVector<int> &codes) {
                    if (location != m_state.settings.value(QStringLiteral("location")).toString(QStringLiteral("Doetinchem"))) return;
                    m_data.temperature = temperature; m_data.weatherCode = code;
                    m_data.weatherUpdated = updated; m_data.weatherStale = stale; m_data.error = error;
                    m_data.forecastDays = days; m_data.forecastTemperatures = temperatures; m_data.forecastCodes = codes; update();
                });
        m_networkTimer.start(qMax(15, m_state.settings.value(QStringLiteral("refreshMinutes")).toInt(30)) * 60 * 1000);
    }
    if (m_definition.id.endsWith(QStringLiteral("feeds"))) {
        connect(services, &RuntimeServices::feedUpdated, this,
                [this](const QString &url, const QStringList &titles, const QStringList &links, const QString &error) {
                    if (url != QUrl::fromUserInput(m_state.settings.value(QStringLiteral("feed")).toString()).toString()) return;
                    m_data.feedTitles = titles; m_data.feedLinks = links; m_data.error = error; update();
                });
        m_networkTimer.start(qMax(5, m_state.settings.value(QStringLiteral("refreshMinutes")).toInt(30)) * 60 * 1000);
    }
    if (m_definition.id.endsWith(QStringLiteral("mediacenter"))) {
        connect(services, &RuntimeServices::mediaUpdated, this,
                [this](const QString &title, const QString &artist, const QString &album, const QString &, bool playing, bool available) {
                    m_data.mediaTitle = title; m_data.mediaArtist = artist; m_data.mediaAlbum = album;
                    m_data.mediaPlaying = playing; m_data.mediaAvailable = available; update();
                });
        connect(services, &RuntimeServices::mediaArtUpdated, this,
                [this](const QString &, const QImage &image) { m_data.mediaArt = image; update(); });
    }
    if (m_definition.id.endsWith(QStringLiteral("picturepuzzle"))) initializePuzzle();
    if (m_definition.id.endsWith(QStringLiteral("slideshow"))) loadSlides();
    QTimer::singleShot(0, this, [this]() { updateNetworkData(false); });
}

void GadgetWindow::updateNetworkData(bool force)
{
    RuntimeServices *services = RuntimeServices::instance();
    if (m_definition.id.endsWith(QStringLiteral("currency"))) {
        services->requestCurrency(m_state.settings.value(QStringLiteral("base")).toString(QStringLiteral("EUR")),
                                  m_state.settings.value(QStringLiteral("target")).toString(QStringLiteral("USD")), force);
    } else if (m_definition.id.endsWith(QStringLiteral("weather"))) {
        services->requestWeather(m_state.settings.value(QStringLiteral("location")).toString(QStringLiteral("Doetinchem")),
                                 m_state.settings.value(QStringLiteral("latitude")).toDouble(51.965),
                                 m_state.settings.value(QStringLiteral("longitude")).toDouble(6.288),
                                 m_state.settings.value(QStringLiteral("unit")).toString() == QStringLiteral("fahrenheit"), force);
    } else if (m_definition.id.endsWith(QStringLiteral("feeds"))) {
        services->requestFeed(m_state.settings.value(QStringLiteral("feed")).toString(QStringLiteral("https://github.com/memegeko/aero7-repo/releases.atom")), force);
    }
}

void GadgetWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    m_data.slideshowControlsVisible = m_definition.id.endsWith(QStringLiteral("slideshow")) && m_controlsOpacity > .05;
    m_data.slideshowPaused = m_slidePaused;
    GadgetPainter::paint(painter, m_definition, m_state, m_data, bodyRect());
    paintControls(painter);
}

QRect GadgetWindow::controlRect(Control control) const
{
    const int x = bodySize().width() + 2;
    switch (control) {
    case Control::Close: return QRect(x, 3, 23, 22);
    case Control::Size: return QRect(x, 26, 23, 22);
    case Control::Options: return QRect(x, 49, 23, 22);
    case Control::Drag: return QRect(x, 72, 23, qMax(24, height() - 75));
    default: return {};
    }
}

GadgetWindow::Control GadgetWindow::controlAt(const QPoint &position) const
{
    for (Control control : {Control::Close, Control::Size, Control::Options, Control::Drag}) {
        if (control == Control::Size && !m_definition.supportsLarge) continue;
        if (control == Control::Options && !m_definition.hasSettings) continue;
        if (controlRect(control).contains(position)) return control;
    }
    return Control::None;
}

void GadgetWindow::paintControls(QPainter &p)
{
    if (m_controlsOpacity <= .01) return;
    p.save();
    p.setOpacity(m_controlsOpacity);
    for (Control control : {Control::Close, Control::Size, Control::Options, Control::Drag}) {
        if (control == Control::Size && !m_definition.supportsLarge) continue;
        if (control == Control::Options && !m_definition.hasSettings) continue;
        QRect r = controlRect(control);
        QLinearGradient gradient(r.topLeft(), r.bottomLeft());
        gradient.setColorAt(0, QColor(59, 66, 71, 238)); gradient.setColorAt(1, QColor(14, 18, 21, 245));
        p.setBrush(gradient); p.setPen(QPen(QColor(255, 255, 255, 80), 1)); p.drawRoundedRect(r, 2, 2);
        p.setPen(QPen(Qt::white, 1.5));
        if (control == Control::Close) {
            p.drawLine(r.center() + QPoint(-4, -4), r.center() + QPoint(4, 4));
            p.drawLine(r.center() + QPoint(4, -4), r.center() + QPoint(-4, 4));
        } else if (control == Control::Size) {
            p.drawRect(r.center().x() - 5, r.center().y() - 5, 10, 10);
            p.drawLine(r.center() + QPoint(1, -3), r.center() + QPoint(4, -3));
            p.drawLine(r.center() + QPoint(4, -3), r.center() + QPoint(4, 0));
        } else if (control == Control::Options) {
            p.drawEllipse(r.center(), 5, 5);
            p.drawLine(r.center() + QPoint(-7, 0), r.center() + QPoint(7, 0));
            p.drawLine(r.center() + QPoint(0, -7), r.center() + QPoint(0, 7));
        } else {
            p.setBrush(Qt::white); p.setPen(Qt::NoPen);
            for (int y = r.top() + 6; y < r.bottom() - 3; y += 6)
                for (int x = r.left() + 7; x <= r.right() - 7; x += 6) p.drawEllipse(QPointF(x, y), 1.2, 1.2);
        }
    }
    p.restore();
}

void GadgetWindow::enterEvent(QEnterEvent *)
{
    m_controlsAnimation.stop();
    m_controlsAnimation.setStartValue(m_controlsOpacity);
    m_controlsAnimation.setEndValue(1.0);
    m_controlsAnimation.start();
}

void GadgetWindow::leaveEvent(QEvent *)
{
    if (m_dragging) return;
    m_controlsAnimation.stop();
    m_controlsAnimation.setStartValue(m_controlsOpacity);
    m_controlsAnimation.setEndValue(0.0);
    m_controlsAnimation.start();
}

void GadgetWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) return;
    if (event->button() != Qt::LeftButton) return;
    m_pressedControl = controlAt(event->position().toPoint());
    if (m_pressedControl == Control::None && bodyRect().contains(event->position().toPoint())) {
        if (m_definition.id.endsWith(QStringLiteral("calendar")) && m_state.size == QStringLiteral("large")) {
            handleCalendarClick(event->position().toPoint());
            return;
        }
        if (m_definition.id.endsWith(QStringLiteral("currency"))) {
            handleCurrencyClick(event->position().toPoint());
            return;
        }
        if (m_definition.id.endsWith(QStringLiteral("picturepuzzle"))) {
            movePuzzleTile(event->position().toPoint());
            return;
        }
        if (m_definition.id.endsWith(QStringLiteral("slideshow")) && handleSlideClick(event->position().toPoint())) return;
        if (m_definition.id.endsWith(QStringLiteral("feeds"))) {
            openFeedItem(event->position().toPoint());
            return;
        }
        if (m_definition.id.endsWith(QStringLiteral("mediacenter"))
            && event->position().y() >= bodyRect().height() * .72) {
            handleMediaClick(event->position().toPoint());
            return;
        }
        m_dragging = true;
        m_dragOffset = m_layerShell
            ? event->position().toPoint()
            : event->globalPosition().toPoint() - pos();
        m_dragStartPosition = m_layerShell ? m_desktopPosition : pos();
    } else if (m_pressedControl == Control::Drag) {
        m_dragging = true;
        m_dragOffset = m_layerShell
            ? event->position().toPoint()
            : event->globalPosition().toPoint() - pos();
        m_dragStartPosition = m_layerShell ? m_desktopPosition : pos();
    }
}

void GadgetWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint requested = m_layerShell
            ? m_dragStartPosition + event->position().toPoint() - m_dragOffset
            : event->globalPosition().toPoint() - m_dragOffset;
        setDesktopPosition(m_manager->snappedPosition(this, requested));
        emit stateChanged();
    }
}

void GadgetWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    const Control released = controlAt(event->position().toPoint());
    if (m_pressedControl != Control::None && released == m_pressedControl) {
        if (released == Control::Close) emit closeRequested();
        else if (released == Control::Size) setSizeMode(m_state.size == QStringLiteral("small") ? QStringLiteral("large") : QStringLiteral("small"));
        else if (released == Control::Options) showOptions();
    }
    m_dragging = false;
    m_pressedControl = Control::None;
    notifyGeometryChanged();
}

void GadgetWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_definition.supportsLarge) {
        setSizeMode(m_state.size == QStringLiteral("small") ? QStringLiteral("large") : QStringLiteral("small"));
    }
}

void GadgetWindow::contextMenuEvent(QContextMenuEvent *event)
{
    showContextMenu(event->globalPos());
}

void GadgetWindow::showContextMenu(const QPoint &globalPosition)
{
    QMenu menu;
    menu.setStyleSheet(menuStyle());
    QAction *moveAction = menu.addAction(QStringLiteral("Move"));
    connect(moveAction, &QAction::triggered, this, [this]() { if (windowHandle()) windowHandle()->startSystemMove(); });
    if (m_definition.supportsLarge) {
        QMenu *sizeMenu = menu.addMenu(QStringLiteral("Size"));
        QAction *small = sizeMenu->addAction(QStringLiteral("Small")); small->setCheckable(true); small->setChecked(m_state.size == QStringLiteral("small"));
        QAction *large = sizeMenu->addAction(QStringLiteral("Large")); large->setCheckable(true); large->setChecked(m_state.size == QStringLiteral("large"));
        connect(small, &QAction::triggered, this, [this]() { setSizeMode(QStringLiteral("small")); });
        connect(large, &QAction::triggered, this, [this]() { setSizeMode(QStringLiteral("large")); });
    }
    QAction *above = menu.addAction(QStringLiteral("Always on top")); above->setCheckable(true); above->setChecked(m_state.alwaysOnTop);
    connect(above, &QAction::toggled, this, &GadgetWindow::setAlwaysOnTop);
    QMenu *opacity = menu.addMenu(QStringLiteral("Opacity"));
    for (int percent : {100, 80, 60, 40, 20}) {
        QAction *action = opacity->addAction(QString::number(percent) + QLatin1Char('%'));
        action->setCheckable(true); action->setChecked(m_state.opacity == percent);
        connect(action, &QAction::triggered, this, [this, percent]() { setOpacityPercent(percent); });
    }
    if (m_definition.hasSettings) {
        menu.addSeparator();
        QAction *options = menu.addAction(QStringLiteral("Options"));
        connect(options, &QAction::triggered, this, &GadgetWindow::showOptions);
    }
    menu.addSeparator();
    QAction *close = menu.addAction(QStringLiteral("Close gadget"));
    connect(close, &QAction::triggered, this, &GadgetWindow::closeRequested);
    menu.exec(globalPosition);
}

void GadgetWindow::setSizeMode(const QString &mode)
{
    if (!m_definition.supportsLarge || (mode != QStringLiteral("small") && mode != QStringLiteral("large"))) return;
    const QRect oldGeometry = bodyGeometry();
    m_state.size = mode;
    if (m_definition.id.endsWith(QStringLiteral("calendar")) && mode == QStringLiteral("small")) m_data.calendarMonthOffset = 0;
    updateWindowSize();
    QPoint requested = oldGeometry.topLeft();
    QScreen *screen = QGuiApplication::screenAt(oldGeometry.center());
    if (!screen) screen = m_desktopScreen ? m_desktopScreen.data() : QGuiApplication::primaryScreen();
    if (screen && qAbs(oldGeometry.right() - screen->availableGeometry().right()) <= 40)
        requested.setX(oldGeometry.right() - bodySize().width() + 1);
    setDesktopPosition(m_manager->snappedPosition(this, requested));
    emit stateChanged();
    update();
}

void GadgetWindow::setOpacityPercent(int percent)
{
    m_state.opacity = qBound(20, percent, 100);
    setWindowOpacity(m_state.opacity / 100.0);
    emit stateChanged();
}

void GadgetWindow::setAlwaysOnTop(bool enabled)
{
    m_state.alwaysOnTop = enabled;
    applyWindowLayer();
    emit stateChanged();
}

void GadgetWindow::showOptions()
{
    GadgetOptionsDialog dialog(m_definition, m_state.settings, this);
    if (dialog.exec() != QDialog::Accepted) return;
    m_state.settings = dialog.settings();
    if (m_definition.id.endsWith(QStringLiteral("clock"))) {
        m_repaintTimer.setInterval(m_state.settings.value(QStringLiteral("seconds")).toBool(true) ? 1000 : 60000);
    }
    if (m_definition.id.endsWith(QStringLiteral("picturepuzzle"))) initializePuzzle();
    if (m_definition.id.endsWith(QStringLiteral("slideshow"))) loadSlides();
    updateNetworkData(true);
    emit stateChanged();
    update();
}

void GadgetWindow::initializePuzzle()
{
    const int difficulty = qBound(3, m_state.settings.value(QStringLiteral("difficulty")).toInt(4), 5);
    m_data.puzzleTiles.clear();
    for (int i = 0; i < difficulty * difficulty; ++i) m_data.puzzleTiles << i;
    int empty = difficulty * difficulty - 1;
    for (int move = 0; move < difficulty * difficulty * 30; ++move) {
        QList<int> candidates;
        const int row = empty / difficulty;
        const int column = empty % difficulty;
        if (row > 0) candidates << empty - difficulty;
        if (row < difficulty - 1) candidates << empty + difficulty;
        if (column > 0) candidates << empty - 1;
        if (column < difficulty - 1) candidates << empty + 1;
        const int selected = candidates.at(QRandomGenerator::global()->bounded(candidates.size()));
        m_data.puzzleTiles.swapItemsAt(empty, selected);
        empty = selected;
    }
    m_data.puzzleMoves = 0;
    const QString imageName = m_state.settings.value(QStringLiteral("image")).toString(QStringLiteral("aero7-flower"));
    const QString customImage = m_state.settings.value(QStringLiteral("customImage")).toString();
    QImage image;
    if (imageName == QStringLiteral("custom") && !customImage.isEmpty()) image.load(customImage);
    if (image.isNull()) {
        image = QImage(512, 512, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QLinearGradient sky(0, 0, 0, image.height());
        if (imageName == QStringLiteral("aero7-aurora")) {
            sky.setColorAt(0, QColor(11, 20, 83)); sky.setColorAt(.55, QColor(30, 154, 191)); sky.setColorAt(1, QColor(43, 106, 58));
        } else if (imageName == QStringLiteral("aero7-landscape")) {
            sky.setColorAt(0, QColor(77, 180, 245)); sky.setColorAt(.58, QColor(202, 240, 250)); sky.setColorAt(1, QColor(79, 166, 41));
        } else {
            sky.setColorAt(0, QColor(40, 75, 110)); sky.setColorAt(.45, QColor(232, 179, 40)); sky.setColorAt(1, QColor(75, 116, 48));
        }
        painter.fillRect(image.rect(), sky);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF center(280, 250);
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 16; ++i) {
            painter.save(); painter.translate(center); painter.rotate(i * 22.5);
            QLinearGradient petal(0, -145, 0, -35); petal.setColorAt(0, QColor(255, 222, 26)); petal.setColorAt(1, QColor(221, 66, 17));
            painter.setBrush(petal); painter.drawEllipse(QRectF(-28, -154, 56, 130)); painter.restore();
        }
        painter.setBrush(QColor(94, 55, 19)); painter.drawEllipse(center, 48, 48);
    }
    m_data.puzzleImage = image;
    update();
}

void GadgetWindow::movePuzzleTile(const QPoint &position)
{
    const int difficulty = qBound(3, m_state.settings.value(QStringLiteral("difficulty")).toInt(4), 5);
    const QRect board = bodyRect().adjusted(3, 3, -3, -3);
    if (!board.contains(position)) return;
    const int column = qBound(0, int((position.x() - board.left()) * difficulty / double(board.width())), difficulty - 1);
    const int row = qBound(0, int((position.y() - board.top()) * difficulty / double(board.height())), difficulty - 1);
    const int selected = row * difficulty + column;
    const int empty = m_data.puzzleTiles.indexOf(difficulty * difficulty - 1);
    if (empty < 0) return;
    const bool adjacent = (qAbs(selected - empty) == 1 && selected / difficulty == empty / difficulty) || qAbs(selected - empty) == difficulty;
    if (!adjacent) return;
    m_data.puzzleTiles.swapItemsAt(selected, empty);
    ++m_data.puzzleMoves;
    update();
}

void GadgetWindow::loadSlides()
{
    QString folder = m_state.settings.value(QStringLiteral("folder")).toString();
    if (folder.isEmpty()) folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir directory(folder);
    m_slideFiles.clear();
    const QStringList filters{QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.webp"), QStringLiteral("*.bmp"), QStringLiteral("*.gif")};
    for (const QFileInfo &file : directory.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name)) m_slideFiles << file.absoluteFilePath();
    m_slideIndex = 0;
    m_slidePaused = false;
    if (m_slideFiles.isEmpty()) {
        m_data.slideImage = QImage(320, 220, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&m_data.slideImage);
        QLinearGradient background(0, 0, m_data.slideImage.width(), m_data.slideImage.height());
        background.setColorAt(0, QColor(114, 173, 49)); background.setColorAt(.48, QColor(246, 218, 43)); background.setColorAt(1, QColor(45, 116, 48));
        painter.fillRect(m_data.slideImage.rect(), background); painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF center(175, 118); painter.setPen(Qt::NoPen);
        for (int i = 0; i < 18; ++i) { painter.save(); painter.translate(center); painter.rotate(i * 20.0); QLinearGradient petal(0, -96, 0, -20); petal.setColorAt(0, QColor(255, 226, 40)); petal.setColorAt(1, QColor(232, 91, 15)); painter.setBrush(petal); painter.drawEllipse(QRectF(-14, -103, 28, 88)); painter.restore(); }
        painter.setBrush(QColor(88, 52, 17)); painter.drawEllipse(center, 28, 28);
    } else {
        m_data.slideImage = QImage(m_slideFiles.first());
    }
    m_slideTimer.start(qMax(2, m_state.settings.value(QStringLiteral("delaySeconds")).toInt(10)) * 1000);
    update();
}

void GadgetWindow::nextSlide()
{
    if (m_slideFiles.isEmpty()) return;
    const int index = m_state.settings.value(QStringLiteral("shuffle")).toBool(false)
        ? QRandomGenerator::global()->bounded(m_slideFiles.size()) : (m_slideIndex + 1) % m_slideFiles.size();
    showSlideAt(index);
}

void GadgetWindow::previousSlide()
{
    if (m_slideFiles.isEmpty()) return;
    showSlideAt((m_slideIndex - 1 + m_slideFiles.size()) % m_slideFiles.size());
}

void GadgetWindow::showSlideAt(int index)
{
    if (index < 0 || index >= m_slideFiles.size()) return;
    const QImage next(m_slideFiles.at(index)); if (next.isNull()) return;
    m_slideIndex = index;
    m_data.previousSlideImage = m_data.slideImage;
    m_data.slideImage = next;
    if (m_state.settings.value(QStringLiteral("transition")).toString(QStringLiteral("fade")) == QStringLiteral("fade")) {
        m_data.slideTransition = 0.0; m_slideAnimation.stop(); m_slideAnimation.start();
    } else {
        m_data.slideTransition = 1.0; m_data.previousSlideImage = {}; update();
    }
}

void GadgetWindow::handleCalendarClick(const QPoint &position)
{
    if (position.y() > 43) return;
    if (position.x() < bodyRect().width() * .28) --m_data.calendarMonthOffset;
    else if (position.x() > bodyRect().width() * .72) ++m_data.calendarMonthOffset;
    else return;
    m_data.calendarMonthOffset = qBound(-1200, m_data.calendarMonthOffset, 1200);
    update();
}

void GadgetWindow::handleCurrencyClick(const QPoint &position)
{
    const bool top = position.y() < bodyRect().height() / 2;
    const QString codeKey = top ? QStringLiteral("base") : QStringLiteral("target");
    if (position.x() > bodyRect().width() * .62) {
        QMenu menu(this); menu.setStyleSheet(menuStyle());
        const QStringList currencies{QStringLiteral("EUR"), QStringLiteral("USD"), QStringLiteral("GBP"), QStringLiteral("JPY"),
                                     QStringLiteral("CHF"), QStringLiteral("CAD"), QStringLiteral("AUD"), QStringLiteral("CNY")};
        for (const QString &currency : currencies) {
            QAction *action = menu.addAction(currency);
            action->setCheckable(true); action->setChecked(m_state.settings.value(codeKey).toString() == currency);
            connect(action, &QAction::triggered, this, [this, codeKey, currency]() {
                m_state.settings.insert(codeKey, currency); updateNetworkData(true); emit stateChanged(); update();
            });
        }
        menu.exec(mapToGlobal(position));
        return;
    }
    const double rate = m_data.currencyRate > 0.0 ? m_data.currencyRate : 1.0;
    const double baseAmount = m_state.settings.value(QStringLiteral("amount")).toDouble(1.0);
    const double current = top ? baseAmount : baseAmount * rate;
    bool accepted = false;
    const double amount = QInputDialog::getDouble(this, QStringLiteral("Currency"),
                                                   top ? QStringLiteral("%1 amount:").arg(m_state.settings.value(QStringLiteral("base")).toString())
                                                       : QStringLiteral("%1 amount:").arg(m_state.settings.value(QStringLiteral("target")).toString()),
                                                   current, 0.0, 1000000000.0, 2, &accepted);
    if (!accepted) return;
    m_state.settings.insert(QStringLiteral("amount"), top ? amount : amount / rate);
    emit stateChanged(); update();
}

bool GadgetWindow::handleSlideClick(const QPoint &position)
{
    if (!m_data.slideshowControlsVisible || position.y() < bodyRect().height() - 34) return false;
    const int third = bodyRect().width() / 3;
    if (position.x() < third) previousSlide();
    else if (position.x() < third * 2) {
        m_slidePaused = !m_slidePaused;
        if (m_slidePaused) m_slideTimer.stop();
        else m_slideTimer.start(qMax(2, m_state.settings.value(QStringLiteral("delaySeconds")).toInt(10)) * 1000);
        update();
    } else nextSlide();
    return true;
}

void GadgetWindow::openFeedItem(const QPoint &position)
{
    if (m_data.feedLinks.isEmpty()) return;
    const int header = m_state.size == QStringLiteral("large") ? 40 : 33;
    const int lineHeight = m_state.size == QStringLiteral("large") ? 31 : 23;
    const int index = (position.y() - header) / lineHeight;
    if (index >= 0 && index < m_data.feedLinks.size() && m_state.settings.value(QStringLiteral("openLinks")).toBool(true)) {
        const QUrl url = QUrl::fromUserInput(m_data.feedLinks.at(index));
        if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) QDesktopServices::openUrl(url);
    }
}

void GadgetWindow::handleMediaClick(const QPoint &position)
{
    if (position.y() < bodyRect().height() * .72) return;
    const int third = bodyRect().width() / 3;
    if (position.x() < third) RuntimeServices::instance()->mediaCommand(QStringLiteral("Previous"));
    else if (position.x() < third * 2) RuntimeServices::instance()->mediaCommand(QStringLiteral("PlayPause"));
    else RuntimeServices::instance()->mediaCommand(QStringLiteral("Next"));
}
