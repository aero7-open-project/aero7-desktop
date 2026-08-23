#include "GadgetManager.h"

#include "GadgetGallery.h"
#include "GadgetWindow.h"

#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QUuid>

GadgetManager::GadgetManager(QObject *parent)
    : QObject(parent)
    , m_definitions(builtinDefinitions())
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, &GadgetManager::saveSession);
    const auto watchScreen = [this](QScreen *screen) {
        connect(screen, &QScreen::geometryChanged, this, &GadgetManager::handleScreensChanged);
        connect(screen, &QScreen::availableGeometryChanged, this, &GadgetManager::handleScreensChanged);
        connect(screen, &QScreen::logicalDotsPerInchChanged, this, &GadgetManager::handleScreensChanged);
    };
    for (QScreen *screen : QGuiApplication::screens()) watchScreen(screen);
    connect(qApp, &QGuiApplication::screenAdded, this, [this, watchScreen](QScreen *screen) {
        watchScreen(screen);
        handleScreensChanged();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, &GadgetManager::handleScreensChanged);
}

GadgetManager::~GadgetManager()
{
    if (!m_restoring) {
        saveSession();
    }
}

QList<GadgetDefinition> GadgetManager::builtinDefinitions()
{
    return {
        {QStringLiteral("org.aero7.gadgets.calendar"), QStringLiteral("Calendar"), {126, 138}, {252, 220}, true, true, false},
        {QStringLiteral("org.aero7.gadgets.clock"), QStringLiteral("Clock"), {132, 132}, {240, 240}, true, true, false},
        {QStringLiteral("org.aero7.gadgets.cpu"), QStringLiteral("CPU Meter"), {132, 98}, {260, 150}, true, false, false},
        {QStringLiteral("org.aero7.gadgets.currency"), QStringLiteral("Currency"), {126, 80}, {252, 130}, true, true, true},
        {QStringLiteral("org.aero7.gadgets.feeds"), QStringLiteral("Feed Headlines"), {126, 118}, {260, 220}, true, true, true},
        {QStringLiteral("org.aero7.gadgets.picturepuzzle"), QStringLiteral("Picture Puzzle"), {126, 126}, {250, 250}, true, true, false},
        {QStringLiteral("org.aero7.gadgets.slideshow"), QStringLiteral("Slide Show"), {126, 96}, {260, 190}, true, true, false},
        {QStringLiteral("org.aero7.gadgets.weather"), QStringLiteral("Weather"), {126, 82}, {270, 185}, true, true, true},
        {QStringLiteral("org.aero7.gadgets.mediacenter"), QStringLiteral("Media Center"), {232, 166}, {310, 230}, true, true, false},
    };
}

const GadgetDefinition *GadgetManager::definition(const QString &id) const
{
    for (const auto &definition : m_definitions) {
        if (definition.id == id) {
            return &definition;
        }
    }
    return nullptr;
}

QString GadgetManager::layoutPath() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/aero7/gadgets");
    QDir().mkpath(directory);
    return directory + QStringLiteral("/layout.json");
}

QScreen *GadgetManager::screenForName(const QString &name) const
{
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == name) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

QJsonObject GadgetManager::defaultSettings(const QString &id) const
{
    if (id.endsWith(QStringLiteral("clock"))) {
        return {{QStringLiteral("timezone"), QStringLiteral("Europe/Amsterdam")},
                {QStringLiteral("label"), QString()}, {QStringLiteral("seconds"), true},
                {QStringLiteral("face"), 0}};
    }
    if (id.endsWith(QStringLiteral("calendar"))) {
        return {{QStringLiteral("firstDay"), 1}, {QStringLiteral("highlightToday"), true},
                {QStringLiteral("weekNumbers"), false}};
    }
    if (id.endsWith(QStringLiteral("currency"))) {
        return {{QStringLiteral("base"), QStringLiteral("EUR")},
                {QStringLiteral("target"), QStringLiteral("USD")}, {QStringLiteral("amount"), 1.0}};
    }
    if (id.endsWith(QStringLiteral("feeds"))) {
        return {{QStringLiteral("feed"), QStringLiteral("https://github.com/memegeko/aero7-repo/releases.atom")},
                {QStringLiteral("refreshMinutes"), 30}, {QStringLiteral("count"), 5},
                {QStringLiteral("openLinks"), true}};
    }
    if (id.endsWith(QStringLiteral("picturepuzzle"))) {
        return {{QStringLiteral("difficulty"), 4}, {QStringLiteral("image"), QStringLiteral("aero7-flower")}};
    }
    if (id.endsWith(QStringLiteral("slideshow"))) {
        return {{QStringLiteral("folder"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
                {QStringLiteral("delaySeconds"), 10}, {QStringLiteral("shuffle"), false},
                {QStringLiteral("transition"), QStringLiteral("fade")}};
    }
    if (id.endsWith(QStringLiteral("weather"))) {
        return {{QStringLiteral("location"), QStringLiteral("Doetinchem")},
                {QStringLiteral("latitude"), 51.965}, {QStringLiteral("longitude"), 6.288},
                {QStringLiteral("unit"), QStringLiteral("celsius")},
                {QStringLiteral("refreshMinutes"), 30}};
    }
    if (id.endsWith(QStringLiteral("mediacenter"))) {
        return {{QStringLiteral("player"), QStringLiteral("auto")}};
    }
    return {};
}

GadgetState GadgetManager::defaultState(const GadgetDefinition &definition, int sequence) const
{
    Q_UNUSED(sequence)
    GadgetState state;
    state.id = definition.id;
    state.instance = uniqueInstance(definition.id);
    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1024, 768);
    state.monitor = screen ? screen->name() : QString();
    constexpr int topMargin = 22;
    constexpr int rightMargin = 34;
    constexpr int gap = 10;
    int columnRight = available.right() - rightMargin;
    int y = available.top() + topMargin;
    for (int attempt = 0; attempt < 128; ++attempt) {
        const QRect candidate(QPoint(columnRight - definition.smallSize.width() + 1, y), definition.smallSize);
        const GadgetWindow *collision = nullptr;
        for (const GadgetWindow *window : m_windows) {
            if (candidate.adjusted(-gap, -gap, gap, gap).intersects(window->bodyGeometry())) {
                collision = window;
                break;
            }
        }
        if (!collision && candidate.bottom() <= available.bottom()) {
            state.x = candidate.x() - available.x();
            state.y = candidate.y() - available.y();
            break;
        }
        if (collision) {
            y = collision->bodyGeometry().bottom() + gap + 1;
            continue;
        }
        columnRight -= definition.smallSize.width() + 28;
        y = available.top() + topMargin;
    }
    if (state.x == 0 && state.y == 0) {
        state.x = qMax(8, available.width() - definition.smallSize.width() - rightMargin);
        state.y = topMargin;
    }
    state.settings = defaultSettings(definition.id);
    return state;
}

QString GadgetManager::uniqueInstance(const QString &id) const
{
    const QString slug = id.section(QLatin1Char('.'), -1);
    return slug + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128).left(8);
}

GadgetWindow *GadgetManager::createWindow(GadgetState state)
{
    const auto *definition = this->definition(state.id);
    if (!definition) {
        qWarning().noquote() << "Aero7 Gadgets: ignoring unknown gadget id" << state.id;
        return nullptr;
    }
    if (state.instance.isEmpty()) {
        state.instance = uniqueInstance(state.id);
    }
    if (state.settings.isEmpty()) {
        state.settings = defaultSettings(state.id);
    }
    if (state.id.endsWith(QStringLiteral("feeds"))) {
        const QString feed = state.settings.value(QStringLiteral("feed")).toString();
        if (feed == QStringLiteral("https://aero7.org/news.xml")
            || feed == QStringLiteral("https://github.com/aero7-open-project/aero7-desktop/releases.atom")) {
            state.settings.insert(QStringLiteral("feed"), QStringLiteral("https://github.com/memegeko/aero7-repo/releases.atom"));
        }
    }

    QScreen *screen = screenForName(state.monitor);
    if (screen) {
        state.monitor = screen->name();
    }
    auto *window = new GadgetWindow(*definition, state, this);
    m_windows.push_back(window);
    connect(window, &GadgetWindow::stateChanged, this, &GadgetManager::scheduleSave);
    connect(window, &GadgetWindow::closeRequested, this, [this, window]() {
        m_windows.removeOne(window);
        window->deleteLater();
        scheduleSave();
        emit LayoutChanged();
    });
    const QRect screenRect = screen ? screen->availableGeometry() : QRect(0, 0, 1024, 768);
    QPoint global = screenRect.topLeft() + QPoint(state.x, state.y);
    global.setX(qBound(screenRect.left(), global.x(), screenRect.right() - window->bodySize().width() + 1));
    global.setY(qBound(screenRect.top(), global.y(), screenRect.bottom() - window->bodySize().height() + 1));
    window->placeOnDesktopLayer(screen, global);
    window->show();
    return window;
}

void GadgetManager::restoreSession()
{
    m_restoring = true;
    QFile file(layoutPath());
    bool restored = false;
    if (file.open(QIODevice::ReadOnly)) {
        const auto document = QJsonDocument::fromJson(file.readAll());
        const auto array = document.object().value(QStringLiteral("gadgets")).toArray();
        for (const auto &entry : array) {
            restored |= createWindow(stateFromJson(entry.toObject())) != nullptr;
        }
    }
    if (!restored) {
        createWindow(defaultState(*definition(QStringLiteral("org.aero7.gadgets.clock")), 0));
        createWindow(defaultState(*definition(QStringLiteral("org.aero7.gadgets.weather")), 1));
    }
    m_restoring = false;
    scheduleSave();
}

void GadgetManager::saveSession()
{
    if (m_restoring) {
        return;
    }
    QJsonArray gadgets;
    for (GadgetWindow *window : std::as_const(m_windows)) {
        gadgets.append(stateToJson(window->stateForSave()));
    }
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("gadgets"), gadgets}};
    QSaveFile file(layoutPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        if (!file.commit()) qWarning().noquote() << "Aero7 Gadgets: could not save layout" << file.errorString();
    } else {
        qWarning().noquote() << "Aero7 Gadgets: could not open layout for writing" << file.errorString();
    }
}

void GadgetManager::scheduleSave()
{
    if (!m_restoring) {
        m_saveTimer.start();
    }
}

void GadgetManager::ShowGallery()
{
    if (!m_gallery) {
        m_gallery = new GadgetGallery(this);
        connect(m_gallery, &QObject::destroyed, this, [this]() { m_gallery = nullptr; });
    }
    m_gallery->show();
    m_gallery->raise();
    m_gallery->activateWindow();
}

QString GadgetManager::AddGadget(const QString &id)
{
    return addGadgetAt(id, {});
}

QString GadgetManager::addGadgetAt(const QString &id, const QPoint &globalPosition)
{
    const auto *item = definition(id);
    if (!item) {
        return {};
    }
    GadgetState state = defaultState(*item, m_windows.size());
    if (auto *window = createWindow(state)) {
        if (!globalPosition.isNull()) {
            window->setDesktopPosition(snappedPosition(window, globalPosition - QPoint(window->bodySize().width() / 2,
                                                                                        window->bodySize().height() / 2)));
        }
        scheduleSave();
        emit LayoutChanged();
        return window->instanceId();
    }
    return {};
}

void GadgetManager::RemoveGadget(const QString &instance)
{
    for (GadgetWindow *window : std::as_const(m_windows)) {
        if (window->instanceId() == instance) {
            m_windows.removeOne(window);
            window->deleteLater();
            scheduleSave();
            emit LayoutChanged();
            return;
        }
    }
}

void GadgetManager::ResetLayout()
{
    const auto old = m_windows;
    m_windows.clear();
    for (GadgetWindow *window : old) {
        window->deleteLater();
    }
    createWindow(defaultState(*definition(QStringLiteral("org.aero7.gadgets.clock")), 0));
    createWindow(defaultState(*definition(QStringLiteral("org.aero7.gadgets.weather")), 1));
    scheduleSave();
    emit LayoutChanged();
}

QPoint GadgetManager::snappedPosition(const GadgetWindow *moving, const QPoint &requested) const
{
    constexpr int threshold = 10;
    QPoint result = requested;
    QRect candidate(requested, moving->bodySize());
    QScreen *screen = QGuiApplication::screenAt(candidate.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect bounds = screen ? screen->availableGeometry() : QRect(0, 0, 1024, 768);

    if (qAbs(candidate.left() - bounds.left()) <= threshold) result.setX(bounds.left());
    if (qAbs(candidate.right() - bounds.right()) <= threshold) result.setX(bounds.right() - candidate.width() + 1);
    if (qAbs(candidate.top() - bounds.top()) <= threshold) result.setY(bounds.top());
    if (qAbs(candidate.bottom() - bounds.bottom()) <= threshold) result.setY(bounds.bottom() - candidate.height() + 1);

    candidate.moveTopLeft(result);
    for (GadgetWindow *window : m_windows) {
        if (window == moving || !window->isVisible()) continue;
        const QRect other = window->bodyGeometry();
        if (qAbs(candidate.left() - other.right() - 1) <= threshold) result.setX(other.right() + 1);
        if (qAbs(candidate.right() - other.left() + 1) <= threshold) result.setX(other.left() - candidate.width());
        if (qAbs(candidate.top() - other.bottom() - 1) <= threshold) result.setY(other.bottom() + 1);
        if (qAbs(candidate.bottom() - other.top() + 1) <= threshold) result.setY(other.top() - candidate.height());
        if (qAbs(candidate.left() - other.left()) <= threshold) result.setX(other.left());
        if (qAbs(candidate.top() - other.top()) <= threshold) result.setY(other.top());
    }
    result.setX(qBound(bounds.left(), result.x(), bounds.right() - candidate.width() + 1));
    result.setY(qBound(bounds.top(), result.y(), bounds.bottom() - candidate.height() + 1));
    return result;
}

void GadgetManager::handleScreensChanged()
{
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) return;
    for (GadgetWindow *window : std::as_const(m_windows)) {
        QScreen *screen = QGuiApplication::screenAt(window->bodyGeometry().center());
        if (!screen) screen = QGuiApplication::primaryScreen();
        const QRect bounds = screen->availableGeometry();
        QPoint clamped = window->bodyGeometry().topLeft();
        clamped.setX(qBound(bounds.left(), clamped.x(), bounds.right() - window->bodySize().width() + 1));
        clamped.setY(qBound(bounds.top(), clamped.y(), bounds.bottom() - window->bodySize().height() + 1));
        if (clamped != window->bodyGeometry().topLeft()) {
            window->setDesktopPosition(clamped);
            window->notifyGeometryChanged();
        }
    }
    scheduleSave();
}
