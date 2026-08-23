// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>

class RecoveryController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString component READ component CONSTANT)
    Q_PROPERTY(QString message READ message CONSTANT)
public:
    explicit RecoveryController(QString component, QObject *parent = nullptr);
    QString component() const;
    QString message() const;

public Q_SLOTS:
    void restartComponent();
    void restartShell();
    void resetShellState();
    void openLogs();
    void signOut();

Q_SIGNALS:
    void operationFinished(const QString &message, bool closeWindow);

private:
    QString unit() const;
    QString m_component;
};
