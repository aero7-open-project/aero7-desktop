// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>

struct Credential { QString user; QString password; };

class VaultBackend : public QObject
{
    Q_OBJECT
public:
    enum State { Locked, Opening, Unlocked };
    using QObject::QObject;
    virtual State state() const = 0;
    virtual void unlock(quintptr window) = 0;
    virtual void lock() = 0;
    virtual QStringList entries() = 0;
    virtual bool read(const QString &target, Credential &value) = 0;
    virtual bool write(const QString &target, const Credential &value) = 0;
    virtual bool remove(const QString &target) = 0;
signals:
    void changed();
    void error(const QString &message);
};
