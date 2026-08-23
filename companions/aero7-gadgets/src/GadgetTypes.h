#pragma once

#include <QJsonObject>
#include <QSize>
#include <QString>

struct GadgetDefinition {
    QString id;
    QString name;
    QSize smallSize;
    QSize largeSize;
    bool supportsLarge = false;
    bool hasSettings = false;
    bool networked = false;
};

struct GadgetState {
    QString id;
    QString instance;
    QString monitor;
    int x = 0;
    int y = 0;
    QString size = QStringLiteral("small");
    int opacity = 100;
    bool alwaysOnTop = false;
    QJsonObject settings;
};

inline QJsonObject stateToJson(const GadgetState &state)
{
    return {
        {QStringLiteral("id"), state.id},
        {QStringLiteral("instance"), state.instance},
        {QStringLiteral("monitor"), state.monitor},
        {QStringLiteral("x"), state.x},
        {QStringLiteral("y"), state.y},
        {QStringLiteral("size"), state.size},
        {QStringLiteral("opacity"), state.opacity},
        {QStringLiteral("alwaysOnTop"), state.alwaysOnTop},
        {QStringLiteral("settings"), state.settings},
    };
}

inline GadgetState stateFromJson(const QJsonObject &object)
{
    GadgetState state;
    state.id = object.value(QStringLiteral("id")).toString();
    state.instance = object.value(QStringLiteral("instance")).toString();
    state.monitor = object.value(QStringLiteral("monitor")).toString();
    state.x = object.value(QStringLiteral("x")).toInt();
    state.y = object.value(QStringLiteral("y")).toInt();
    state.size = object.value(QStringLiteral("size")).toString(QStringLiteral("small"));
    state.opacity = object.value(QStringLiteral("opacity")).toInt(100);
    state.alwaysOnTop = object.value(QStringLiteral("alwaysOnTop")).toBool(false);
    state.settings = object.value(QStringLiteral("settings")).toObject();
    return state;
}
