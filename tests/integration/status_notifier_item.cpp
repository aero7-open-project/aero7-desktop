// SPDX-License-Identifier: MIT
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QJsonDocument>
#include <QJsonObject>

class TestStatusNotifierItem final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category READ category CONSTANT)
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ title CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(QString IconName READ iconName CONSTANT)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ menu CONSTANT)

public:
    using QObject::QObject;
    QString category() const { return QStringLiteral("ApplicationStatus"); }
    QString id() const { return QStringLiteral("aero7-status-notifier-test"); }
    QString title() const { return QStringLiteral("Aero7 Tray Protocol Test"); }
    QString status() const { return QStringLiteral("Active"); }
    QString iconName() const { return QStringLiteral("dialog-information"); }
    bool itemIsMenu() const { return false; }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/NO_DBUSMENU")); }

public Q_SLOTS:
    Q_SCRIPTABLE void Activate(int, int) { ++m_activations; }
    Q_SCRIPTABLE void SecondaryActivate(int, int) { ++m_activations; }
    Q_SCRIPTABLE void ContextMenu(int, int) { }
    Q_SCRIPTABLE void Scroll(int, const QString &) { }
    Q_SCRIPTABLE QString dumpState() const
    {
        return QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("activations"), m_activations}})
                                     .toJson(QJsonDocument::Compact));
    }

Q_SIGNALS:
    void NewIcon();

private:
    int m_activations = 0;
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.aero7.TestStatusNotifier"))) return 73;
    TestStatusNotifierItem item;
    if (!bus.registerObject(QStringLiteral("/StatusNotifierItem"), &item,
                            QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals
                                | QDBusConnection::ExportScriptableSlots)) return 70;
    QDBusInterface watcher(QStringLiteral("org.kde.StatusNotifierWatcher"), QStringLiteral("/StatusNotifierWatcher"),
                           QStringLiteral("org.kde.StatusNotifierWatcher"));
    watcher.call(QStringLiteral("RegisterStatusNotifierItem"), QStringLiteral("org.aero7.TestStatusNotifier"));
    return application.exec();
}

#include "status_notifier_item.moc"
