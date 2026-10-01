#pragma once

#include <QObject>
#include <QQmlEngine>

class GadgetDrop final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit GadgetDrop(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE bool add(const QString &id, const QString &screenName, int x, int y) const;
};
