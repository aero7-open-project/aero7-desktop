/*
    SPDX-FileCopyrightText: 2014 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "directorypicker.h"

#include <QProcess>

#include <KLocalizedString>

DirectoryPicker::DirectoryPicker(QObject *parent)
    : QObject(parent)
{
}

DirectoryPicker::~DirectoryPicker()
{
    delete m_dialogProcess;
}

QUrl DirectoryPicker::url() const
{
    return m_url;
}

void DirectoryPicker::open()
{
    if (m_dialogProcess && m_dialogProcess->state() != QProcess::NotRunning)
        return;
    if (!m_dialogProcess) {
        m_dialogProcess = new QProcess(this);
        connect(m_dialogProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, &DirectoryPicker::dialogAccepted);
    }
    m_dialogProcess->setProgram(QStringLiteral("aero7-file-dialog"));
    m_dialogProcess->setArguments({QStringLiteral("--mode"), QStringLiteral("folder"),
                                   QStringLiteral("--app-id"), QStringLiteral("aero7-desktop-folder")});
    m_dialogProcess->start();
}

void DirectoryPicker::dialogAccepted()
{
    if (!m_dialogProcess || m_dialogProcess->exitStatus() != QProcess::NormalExit
        || m_dialogProcess->exitCode() != 0)
        return;
    const QString path = QString::fromUtf8(m_dialogProcess->readAllStandardOutput()).trimmed();
    if (!path.isEmpty()) {
        m_url = QUrl::fromLocalFile(path.section(QLatin1Char('\n'), 0, 0));
        Q_EMIT urlChanged();
    }
}

#include "moc_directorypicker.cpp"
