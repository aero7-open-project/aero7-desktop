/*
    SPDX-FileCopyrightText: 2026 Aero7 Open Project
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "aero7tasksmodel.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace {
QJsonValue jsonValue(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QUrl) {
        return value.toUrl().toString();
    }
    return QJsonValue::fromVariant(value);
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    Aero7TasksModel model;
    model.setGroupMode(TaskManager::TasksModel::GroupApplications);
    model.setHideActivatedLaunchers(true);
    model.setShellLauncherList({QStringLiteral("applications:aero7-internet-explorer.desktop")});

    QTimer::singleShot(2500, &app, [&app, &model] {
        QJsonObject report;
        report.insert(QStringLiteral("backend"), model.internetExplorerBackend());
        report.insert(QStringLiteral("shellLaunchers"),
                      QJsonArray::fromStringList(model.shellLauncherList()));

        QJsonArray rows;
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, 0);
            const auto exposed = [&model, &index](int role) {
                return model.data(index, role);
            };
            const auto raw = [&model, &index](int role) {
                return model.TaskManager::TasksModel::data(index, role);
            };

            QJsonObject item;
            item.insert(QStringLiteral("row"), row);
            item.insert(QStringLiteral("display"),
                        jsonValue(exposed(Qt::DisplayRole)));
            item.insert(QStringLiteral("rawDisplay"),
                        jsonValue(raw(Qt::DisplayRole)));
            item.insert(QStringLiteral("appId"),
                        jsonValue(exposed(TaskManager::AbstractTasksModel::AppId)));
            item.insert(QStringLiteral("rawAppId"),
                        jsonValue(raw(TaskManager::AbstractTasksModel::AppId)));
            item.insert(QStringLiteral("launcher"),
                        jsonValue(exposed(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon)));
            item.insert(QStringLiteral("rawLauncher"),
                        jsonValue(raw(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon)));
            item.insert(QStringLiteral("isLauncher"),
                        jsonValue(exposed(TaskManager::AbstractTasksModel::IsLauncher)));
            item.insert(QStringLiteral("isWindow"),
                        jsonValue(exposed(TaskManager::AbstractTasksModel::IsWindow)));
            item.insert(QStringLiteral("isGroupParent"),
                        jsonValue(exposed(TaskManager::AbstractTasksModel::IsGroupParent)));
            item.insert(QStringLiteral("childCount"),
                        jsonValue(exposed(TaskManager::AbstractTasksModel::ChildCount)));
            rows.append(item);
        }
        report.insert(QStringLiteral("rows"), rows);
        fprintf(stdout, "%s\n", QJsonDocument(report).toJson(QJsonDocument::Indented).constData());
        app.quit();
    });

    return app.exec();
}
