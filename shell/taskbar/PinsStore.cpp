// SPDX-License-Identifier: MIT
#include "PinsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

PinsStore::PinsStore(QString configPath, QString defaultsPath)
    : m_configPath(std::move(configPath))
    , m_defaultsPath(std::move(defaultsPath))
{
}

QStringList PinsStore::readPins(const QString &path, bool *valid)
{
    if (valid) *valid = false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    const auto object = document.object();
    const auto pinsValue = object.value(QStringLiteral("pins"));
    if (object.value(QStringLiteral("schema")).toInt(-1) != 1 || !pinsValue.isArray()) {
        return {};
    }
    QStringList pins;
    for (const auto &entry : pinsValue.toArray()) {
        if (!entry.isString()) return {};
        pins.append(entry.toString());
    }
    if (valid) *valid = true;
    return normalize(pins);
}

QStringList PinsStore::normalize(const QStringList &pins)
{
    QStringList result;
    QSet<QString> seen;
    for (const auto &rawPin : pins) {
        auto pin = rawPin.trimmed();
        if (pin == QStringLiteral("applications:linux-controlpanel.desktop"))
            pin = QStringLiteral("applications:org.aero7.controlpanel.desktop");
        if (pin.isEmpty() || seen.contains(pin)) {
            continue;
        }
        seen.insert(pin);
        result.append(pin);
    }
    return result;
}

QStringList PinsStore::load() const
{
    if (QFile::exists(m_configPath)) {
        bool valid = false;
        const auto configured = readPins(m_configPath, &valid);
        if (valid) return configured;
    }
    return readPins(m_defaultsPath);
}

bool PinsStore::save(const QStringList &pins) const
{
    if (!QDir().mkpath(QFileInfo(m_configPath).absolutePath())) {
        return false;
    }
    QJsonArray values;
    for (const auto &pin : normalize(pins)) {
        values.append(pin);
    }
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 1);
    root.insert(QStringLiteral("pins"), values);

    QSaveFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}
