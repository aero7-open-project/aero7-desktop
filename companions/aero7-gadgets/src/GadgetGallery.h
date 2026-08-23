#pragma once

#include <QDialog>

class GadgetManager;
class QLabel;
class QLineEdit;
class QListWidget;
class QToolButton;

class GadgetGallery final : public QDialog
{
    Q_OBJECT

public:
    explicit GadgetGallery(GadgetManager *manager, QWidget *parent = nullptr);

private slots:
    void rebuild();
    void addCurrent();
    void showItemMenu(const QPoint &position);

private:
    GadgetManager *m_manager = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_pageLabel = nullptr;
    QLabel *m_details = nullptr;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
    int m_page = 0;
};
