// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

class PinsStore
{
public:
    PinsStore(QString configPath, QString defaultsPath);

    [[nodiscard]] QStringList load() const;
    [[nodiscard]] bool save(const QStringList &pins) const;

private:
    [[nodiscard]] static QStringList readPins(const QString &path, bool *valid = nullptr);
    [[nodiscard]] static QStringList normalize(const QStringList &pins);

    QString m_configPath;
    QString m_defaultsPath;
};
