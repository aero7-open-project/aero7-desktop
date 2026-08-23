// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QStringList>

class ApplicationsModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool showAll READ showAll WRITE setShowAll NOTIFY showAllChanged)
    Q_PROPERTY(int totalCount READ totalCount CONSTANT)

public:
    enum Role {
        StorageIdRole = Qt::UserRole + 1,
        NameRole,
        GenericNameRole,
        IconNameRole,
        CategoriesRole,
        PinnedRole,
        RecentRole,
    };
    Q_ENUM(Role)

    explicit ApplicationsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString query() const;
    void setQuery(const QString &query);
    [[nodiscard]] bool showAll() const;
    void setShowAll(bool showAll);
    [[nodiscard]] int totalCount() const;

    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE bool launchStorageId(const QString &storageId);
    Q_INVOKABLE bool togglePin(int row);
    Q_INVOKABLE int rowForStorageId(const QString &storageId) const;
    Q_INVOKABLE QString dumpState() const;

Q_SIGNALS:
    void queryChanged();
    void showAllChanged();
    void applicationLaunched();

private:
    struct Application {
        QString storageId;
        QString name;
        QString genericName;
        QString iconName;
        QStringList categories;
        bool pinned = false;
        qint64 lastUsed = 0;
    };

    void loadApplications();
    void loadState();
    void saveState() const;
    void rebuildVisible();
    bool launchApplication(Application &application);
    [[nodiscard]] QString configPath() const;
    [[nodiscard]] QString defaultsPath() const;

    QList<Application> m_applications;
    QList<int> m_visible;
    QString m_query;
    bool m_showAll = false;
};
