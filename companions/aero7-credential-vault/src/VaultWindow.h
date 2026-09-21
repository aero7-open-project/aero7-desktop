// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
class QLabel;
class QPushButton;
class QTreeWidget;
class VaultBackend;

class VaultWindow : public QWidget
{
    Q_OBJECT
public:
    explicit VaultWindow(VaultBackend *backend);
private:
    void refresh();
    void edit(bool adding);
    void remove();
    void report(const QString &message);
    VaultBackend *m_backend;
    QLabel *m_status;
    QTreeWidget *m_entries;
    QPushButton *m_unlock;
    QPushButton *m_add;
    QPushButton *m_edit;
    QPushButton *m_remove;
};
