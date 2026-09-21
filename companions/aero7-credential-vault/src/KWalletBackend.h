// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "VaultBackend.h"
#include <QVariantMap>
#include <QDBusVariant>
#include <QDBusObjectPath>

class KWalletBackend final : public VaultBackend
{
    Q_OBJECT
public:
    explicit KWalletBackend(QObject *parent = nullptr);
    ~KWalletBackend() override;
    State state() const override { return m_state; }
    void unlock(quintptr window) override;
    void lock() override;
    QStringList entries() override;
    bool read(const QString &target, Credential &value) override;
    bool write(const QString &target, const Credential &value) override;
    bool remove(const QString &target) override;
private:
    bool ready();
    void disconnected();
    QString collection(bool *resolved = nullptr) const;
    bool collectionLocked() const;
    void openHandle();
    void showPrompt(const QString &path);
    int m_handle = -1;
    quint64 m_generation = 0;
    QString m_collection;
    QString m_prompt;
    quintptr m_window = 0;
    State m_state = Locked;
private slots:
    void serviceCollectionChanged(const QDBusObjectPath &path);
    void serviceCollectionDeleted(const QDBusObjectPath &path);
    void walletClosed(int handle);
    void promptCompleted(bool dismissed, const QDBusVariant &result);
    void collectionChanged(const QString &interface, const QVariantMap &changed,
                           const QStringList &invalidated);
};
