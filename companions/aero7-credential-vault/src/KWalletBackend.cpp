// SPDX-License-Identifier: GPL-3.0-or-later
#include "KWalletBackend.h"
#include <KWallet>
#include <QCoreApplication>
#include <QDataStream>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QIODevice>
#include <QMap>

namespace {
const QString walletName = QStringLiteral("Aero7 Credentials");
const QString folderName = QStringLiteral("Generic Credentials");
const QString walletService = QStringLiteral("org.kde.kwalletd6");
const QString walletPath = QStringLiteral("/modules/kwalletd6");
// KWallet's activatable name, also used by its compatibility bridge. The
// generic Secret Service name may be absent or owned by a different provider.
const QString secretService = QStringLiteral("org.kde.secretservicecompat");
const QString collectionInterface = QStringLiteral("org.freedesktop.Secret.Collection");

QString appId() { return QCoreApplication::applicationName(); }

QDBusMessage walletMessage(const QString &method, const QVariantList &arguments)
{
    auto message = QDBusMessage::createMethodCall(walletService, walletPath,
                                                 "org.kde.KWallet", method);
    message.setArguments(arguments);
    return message;
}

QDBusMessage call(const QString &method, const QVariantList &arguments)
{
    return QDBusConnection::sessionBus().call(walletMessage(method, arguments), QDBus::Block, 10000);
}

QVariant secretProperty(const QString &path, const QString &interface, const QString &key)
{
    auto message = QDBusMessage::createMethodCall(secretService, path,
                                                 "org.freedesktop.DBus.Properties", "Get");
    message.setArguments({interface, key});
    QDBusReply<QDBusVariant> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
    return reply.isValid() ? reply.value().variant() : QVariant{};
}
}

KWalletBackend::KWalletBackend(QObject *parent) : VaultBackend(parent)
{
    auto bus = QDBusConnection::sessionBus();
    auto *watcher = new QDBusServiceWatcher(walletService, bus,
        QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &KWalletBackend::disconnected);
    auto *secrets = new QDBusServiceWatcher(secretService, bus,
        QDBusServiceWatcher::WatchForUnregistration, this);
    connect(secrets, &QDBusServiceWatcher::serviceUnregistered, this, &KWalletBackend::disconnected);
    bus.connect(walletService, walletPath, "org.kde.KWallet", "walletClosedId",
                this, SLOT(walletClosed(int)));
    // KWallet 6.29 reports an external lock here without emitting the
    // collection's PropertiesChanged(Locked). Observe both protocol paths.
    bus.connect(secretService, "/org/freedesktop/secrets", "org.freedesktop.Secret.Service",
                "CollectionChanged", this, SLOT(serviceCollectionChanged(QDBusObjectPath)));
    bus.connect(secretService, "/org/freedesktop/secrets", "org.freedesktop.Secret.Service",
                "CollectionDeleted", this, SLOT(serviceCollectionDeleted(QDBusObjectPath)));
}

KWalletBackend::~KWalletBackend()
{
    if (!m_prompt.isEmpty()) {
        const auto message = QDBusMessage::createMethodCall(secretService, m_prompt,
            "org.freedesktop.Secret.Prompt", "Dismiss");
        QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
    }
    if (m_handle >= 0) call("close", {m_handle, false, appId()});
}

bool KWalletBackend::collectionLocked() const
{
    const auto value = secretProperty(m_collection, collectionInterface, "Locked");
    // An unknown or malformed lock state is not proof of an unlocked vault.
    // Do not coerce strings/numbers into the protocol's boolean property.
    return value.metaType().id() != QMetaType::Bool || value.toBool();
}

QString KWalletBackend::collection(bool *resolved) const
{
    const auto value = secretProperty("/org/freedesktop/secrets", "org.freedesktop.Secret.Service", "Collections");
    if (resolved) *resolved = value.isValid();
    if (!value.isValid()) return {};
    const auto paths = qdbus_cast<QList<QDBusObjectPath>>(value);
    QString result;
    for (const auto &path : paths) {
        if (secretProperty(path.path(), collectionInterface, "Label").toString() != walletName) continue;
        if (!result.isEmpty()) {
            if (resolved) *resolved = false;
            return {}; // Never guess between duplicate labels or create another.
        }
        result = path.path();
    }
    return result;
}

bool KWalletBackend::ready()
{
    if (m_state != Unlocked || m_handle < 0) return false;
    if (collectionLocked()) { disconnected(); return false; }
    return true;
}

void KWalletBackend::disconnected()
{
    ++m_generation;
    if (!m_prompt.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(secretService, m_prompt,
            "org.freedesktop.Secret.Prompt", "Completed", this,
            SLOT(promptCompleted(bool,QDBusVariant)));
        m_prompt.clear();
    }
    if (!m_collection.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(secretService, m_collection,
            "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
            SLOT(collectionChanged(QString,QVariantMap,QStringList)));
    }
    m_collection.clear();
    m_handle = -1;
    m_state = Locked;
    emit changed();
}

void KWalletBackend::walletClosed(int handle)
{
    if (handle == m_handle) disconnected();
}

void KWalletBackend::serviceCollectionChanged(const QDBusObjectPath &path)
{
    if (m_state != Unlocked || path.path() != m_collection) return;
    const auto generation = m_generation;
    auto message = QDBusMessage::createMethodCall(secretService, m_collection,
        "org.freedesktop.DBus.Properties", "Get");
    message.setArguments({collectionInterface, QStringLiteral("Locked")});
    auto *request = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, 1000), this);
    connect(request, &QDBusPendingCallWatcher::finished, this, [this, request, generation] {
        QDBusPendingReply<QDBusVariant> reply = *request;
        request->deleteLater();
        if (generation != m_generation || m_state != Unlocked) return;
        if (reply.isError() || reply.value().variant().metaType().id() != QMetaType::Bool
            || reply.value().variant().toBool()) disconnected();
    });
}

void KWalletBackend::serviceCollectionDeleted(const QDBusObjectPath &path)
{
    if (!m_collection.isEmpty() && path.path() == m_collection) disconnected();
}

void KWalletBackend::collectionChanged(const QString &interface, const QVariantMap &changed,
                                       const QStringList &invalidated)
{
    if (interface == collectionInterface
        && ((changed.contains("Locked")
             && (changed.value("Locked").metaType().id() != QMetaType::Bool
                 || changed.value("Locked").toBool()))
            || invalidated.contains("Locked"))) disconnected();
}

void KWalletBackend::unlock(quintptr window)
{
    if (m_state != Locked) return;
    if (!KWallet::Wallet::isEnabled()) {
        emit error(tr("The vault is disabled by your account's wallet settings. That existing override has been preserved. Sign out after enabling the feature; an account-specific disable must be removed separately."));
        return;
    }
    m_state = Opening;
    const auto generation = ++m_generation;
    m_window = window;
    emit changed();
    // Handle Secret Service prompts directly. KWallet 6.29's libsecret bridge
    // hangs when ksecretd sends an array instead of an object path on cancelled
    // collection creation. A dismissed prompt has no usable result; ignore it.
    bool resolved = false;
    m_collection = collection(&resolved);
    if (!resolved) {
        disconnected();
        emit error(tr("The vault collection list is unavailable or ambiguous. No credentials were changed."));
        return;
    }
    const bool creating = m_collection.isEmpty();
    auto message = QDBusMessage::createMethodCall(secretService, "/org/freedesktop/secrets",
        "org.freedesktop.Secret.Service", creating ? "CreateCollection" : "Unlock");
    if (creating) {
        QVariantMap properties{{"org.freedesktop.Secret.Collection.Label", walletName}};
        message.setArguments({properties, QString()});
    } else {
        message.setArguments({QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(m_collection)})});
    }
    auto *request = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, 10000), this);
    connect(request, &QDBusPendingCallWatcher::finished, this, [this, request, creating, generation] {
        request->deleteLater();
        if (generation != m_generation) return;
        QString prompt;
        bool failed;
        if (creating) {
            QDBusPendingReply<QDBusObjectPath, QDBusObjectPath> reply = *request;
            failed = reply.isError();
            if (!failed) prompt = reply.argumentAt<1>().path();
        } else {
            QDBusPendingReply<QList<QDBusObjectPath>, QDBusObjectPath> reply = *request;
            failed = reply.isError();
            if (!failed) prompt = reply.argumentAt<1>().path();
        }
        if (failed) {
            disconnected();
            emit error(tr("The vault service could not prepare the requested operation. No credentials were changed."));
        } else if (prompt != "/" && !prompt.isEmpty()) showPrompt(prompt);
        else openHandle();
    });
}

void KWalletBackend::showPrompt(const QString &path)
{
    m_prompt = path;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.connect(secretService, path, "org.freedesktop.Secret.Prompt", "Completed",
                     this, SLOT(promptCompleted(bool,QDBusVariant)))) {
        disconnected();
        emit error(tr("The vault password prompt could not be connected."));
        return;
    }
    auto message = QDBusMessage::createMethodCall(secretService, path,
        "org.freedesktop.Secret.Prompt", "Prompt");
    // No X11 window identifier is exported by this Wayland application.
    message.setArguments({QString()});
    const auto generation = m_generation;
    auto *request = new QDBusPendingCallWatcher(bus.asyncCall(message, 10000), this);
    connect(request, &QDBusPendingCallWatcher::finished, this, [this, request, generation] {
        QDBusPendingReply<> reply = *request;
        request->deleteLater();
        if (generation == m_generation && !m_prompt.isEmpty() && reply.isError()) {
            disconnected();
            emit error(tr("The vault password prompt could not be opened."));
        }
    });
}

void KWalletBackend::promptCompleted(bool dismissed, const QDBusVariant &result)
{
    Q_UNUSED(result); // Verify the actual collection, not a prompt's supplied path.
    if (m_state != Opening || m_prompt.isEmpty()) return;
    QDBusConnection::sessionBus().disconnect(secretService, m_prompt,
        "org.freedesktop.Secret.Prompt", "Completed", this,
        SLOT(promptCompleted(bool,QDBusVariant)));
    m_prompt.clear();
    if (dismissed) {
        disconnected();
        emit error(tr("The vault was not unlocked. You can try again."));
    } else openHandle();
}

void KWalletBackend::openHandle()
{
    m_collection = collection();
    if (m_collection.isEmpty() || collectionLocked()) {
        disconnected();
        emit error(tr("The credential collection could not be verified. No credentials were changed."));
        return;
    }
    const auto generation = m_generation;
    // The collection is already unlocked by its native password prompt. Use
    // the reply-bearing KWallet method to obtain only the compatibility handle.
    auto pending = QDBusConnection::sessionBus().asyncCall(
        walletMessage("open", {walletName, QVariant::fromValue(qlonglong(m_window)), appId()}), 10000);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
        QDBusPendingReply<int> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation) return;
        if (reply.isError() || reply.value() < 0) {
            disconnected();
            emit error(tr("The vault was not unlocked. You can try again."));
            return;
        }
        m_handle = reply.value();
        m_collection = collection();
        QDBusReply<bool> exists = call("hasFolder", {m_handle, folderName, appId()});
        bool folderReady = exists.isValid() && exists.value();
        if (exists.isValid() && !exists.value()) {
            QDBusReply<bool> created = call("createFolder", {m_handle, folderName, appId()});
            folderReady = created.isValid() && created.value();
        }
        if (m_collection.isEmpty() || collectionLocked() || !folderReady) {
            call("close", {m_handle, false, appId()});
            disconnected();
            emit error(tr("The credential collection could not be verified. No credentials were changed."));
            return;
        }
        QDBusConnection::sessionBus().connect(secretService, m_collection,
            "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
            SLOT(collectionChanged(QString,QVariantMap,QStringList)));
        m_state = Unlocked;
        emit changed();
    });
}

void KWalletBackend::lock()
{
    if (m_state != Unlocked) return;
    // Closing a compatibility handle alone does not lock the encrypted data.
    // Lock the actual Secret Service collection and verify its Locked property.
    const QString path = m_collection;
    const int handle = m_handle;
    const auto generation = m_generation;
    m_state = Opening;
    emit changed();
    auto message = QDBusMessage::createMethodCall(secretService, "/org/freedesktop/secrets",
                                                  "org.freedesktop.Secret.Service", "Lock");
    message.setArguments({QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(path)})});
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, 10000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, path, handle, generation] {
        QDBusPendingReply<QList<QDBusObjectPath>, QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation) return;
        const auto locked = secretProperty(path, collectionInterface, "Locked");
        if (reply.isError() || locked.metaType().id() != QMetaType::Bool) {
            // Losing verification is not evidence of an unlocked wallet. Drop
            // local access without claiming the service completed the lock.
            disconnected();
            emit error(tr("The vault lock could not be verified. Credentials have been hidden. Sign out before leaving this computer."));
            return;
        }
        if (!locked.toBool()) {
            m_state = Unlocked;
            emit changed();
            emit error(tr("The vault could not be locked. Try again before leaving this computer."));
            return;
        }
        call("close", {handle, true, appId()});
        disconnected();
    });
}

QStringList KWalletBackend::entries()
{
    if (!ready()) return {};
    QDBusReply<QStringList> reply = call("entryList", {m_handle, folderName, appId()});
    if (!reply.isValid()) { disconnected(); emit error(tr("The vault connection was lost.")); return {}; }
    return reply.value();
}

bool KWalletBackend::read(const QString &target, Credential &value)
{
    if (!ready()) return false;
    QDBusReply<QByteArray> reply = call("readMap", {m_handle, folderName, target, appId()});
    if (!reply.isValid()) return false;
    QMap<QString, QString> fields;
    QDataStream stream(reply.value());
    stream >> fields;
    if (stream.status() != QDataStream::Ok || fields.value("format") != "aero7-v1") return false;
    value = {fields.value("username"), fields.value("password")};
    return true;
}

bool KWalletBackend::write(const QString &target, const Credential &value)
{
    if (!ready()) return false;
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << QMap<QString, QString>{{"format", "aero7-v1"},
        {"username", value.user}, {"password", value.password}};
    QDBusReply<int> reply = call("writeMap", {m_handle, folderName, target, bytes, appId()});
    if (!reply.isValid() || reply.value() != 0) return false;
    emit changed();
    return true;
}

bool KWalletBackend::remove(const QString &target)
{
    if (!ready()) return false;
    QDBusReply<int> reply = call("removeEntry", {m_handle, folderName, target, appId()});
    if (!reply.isValid() || reply.value() != 0) return false;
    emit changed();
    return true;
}
