#include "GadgetGallery.h"

#include "GadgetManager.h"
#include "GadgetPainter.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int idRole = Qt::UserRole + 1;

class GadgetList final : public QListWidget
{
public:
    explicit GadgetList(GadgetManager *manager, QWidget *parent)
        : QListWidget(parent), m_manager(manager) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QListWidget::mousePressEvent(event);
        if (event->button() == Qt::LeftButton) {
            if (QListWidgetItem *item = itemAt(event->position().toPoint())) {
                m_dragId = item->data(idRole).toString();
                m_dragIcon = item->icon().pixmap(64, 64);
                m_pressGlobal = desktopPoint(event);
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_dragId.isEmpty() && event->buttons().testFlag(Qt::LeftButton)) {
            const QPoint global = desktopPoint(event);
            if (!m_dragging && (global - m_pressGlobal).manhattanLength() >= QApplication::startDragDistance()) {
                m_dragging = true;
                m_dragPreview = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput);
                m_dragPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
                m_dragPreview->setAttribute(Qt::WA_ShowWithoutActivating);
                m_dragPreview->setStyleSheet(QStringLiteral("background:transparent;"));
                m_dragPreview->setPixmap(m_dragIcon);
                m_dragPreview->setFixedSize(m_dragIcon.size());
                m_dragPreview->show();
            }
            if (m_dragging) {
                m_dragPreview->move(global + QPoint(10, 10));
                event->accept();
                return;
            }
        }
        QListWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_dragging) {
            const QPoint global = desktopPoint(event);
            if (!window()->geometry().contains(global)) {
                m_manager->addGadgetAt(m_dragId, global);
            }
            clearDrag();
            event->accept();
            return;
        }
        clearDrag();
        QListWidget::mouseReleaseEvent(event);
    }

private:
    QPoint desktopPoint(const QMouseEvent *event) const
    {
        if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
            // Wayland deliberately withholds global pointer coordinates, but
            // Qt/KWin still exposes this top-level window's configured frame.
            // Reconstruct the desktop point from the viewport-local event.
            return window()->geometry().topLeft()
                + viewport()->mapTo(window(), event->position().toPoint());
        }
        return event->globalPosition().toPoint();
    }

    void clearDrag()
    {
        if (m_dragPreview) {
            m_dragPreview->deleteLater();
            m_dragPreview = nullptr;
        }
        m_dragId.clear();
        m_dragIcon = {};
        m_dragging = false;
    }

    GadgetManager *m_manager;
    QLabel *m_dragPreview = nullptr;
    QString m_dragId;
    QPixmap m_dragIcon;
    QPoint m_pressGlobal;
    bool m_dragging = false;
};
}

GadgetGallery::GadgetGallery(GadgetManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(QStringLiteral("Desktop Gadgets"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-widgets")));
    setFixedSize(478, 350);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 6);
    root->setSpacing(4);

    auto *navigation = new QHBoxLayout;
    const QString pageButtonStyle = QStringLiteral("QToolButton { border:1px solid #a8adb2; border-radius:9px; background:#f8f8f8; color:#303030; font:bold 12pt 'Segoe UI'; padding:0; } QToolButton:hover { background:#eaf5ff; border-color:#6fa1cf; } QToolButton:disabled { border-color:#d7d7d7; color:#c3c3c3; background:#fafafa; }");
    m_previous = new QToolButton(this); m_previous->setText(QStringLiteral("‹")); m_previous->setFixedSize(19, 19); m_previous->setStyleSheet(pageButtonStyle);
    m_pageLabel = new QLabel(QStringLiteral("Page 1 of 2"), this);
    m_next = new QToolButton(this); m_next->setText(QStringLiteral("›")); m_next->setFixedSize(19, 19); m_next->setStyleSheet(pageButtonStyle);
    navigation->addWidget(m_previous); navigation->addWidget(m_pageLabel); navigation->addWidget(m_next); navigation->addStretch(1);
    m_search = new QLineEdit(this); m_search->setPlaceholderText(QStringLiteral("Search gadgets")); m_search->setClearButtonEnabled(true); m_search->setFixedWidth(200);
    navigation->addWidget(m_search);
    root->addLayout(navigation);

    m_list = new GadgetList(manager, this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Fixed);
    m_list->setIconSize(QSize(66, 66));
    m_list->setGridSize(QSize(110, 104));
    m_list->setSpacing(0);
    m_list->setWordWrap(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragEnabled(false);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#fbfbfb; border:1px solid #c9c9c9; outline:0; font:9pt 'Segoe UI'; }"
        "QListWidget::item { padding:3px; border:1px solid transparent; }"
        "QListWidget::item:hover { background:#eef7ff; border:1px solid #b8d6f2; }"
        "QListWidget::item:selected { background:#dceeff; border:1px solid #7da2ce; color:#111; }"));
    root->addWidget(m_list, 1);

    auto *footer = new QHBoxLayout;
    auto *detailsButton = new QToolButton(this); detailsButton->setText(QStringLiteral("⌄  Show details")); detailsButton->setAutoRaise(true); detailsButton->setCheckable(true);
    footer->addWidget(detailsButton); footer->addStretch(1);
    auto *online = new QPushButton(QIcon::fromTheme(QStringLiteral("internet-web-browser")), QStringLiteral("Get more gadgets online"), this);
    online->setFlat(true); online->setStyleSheet(QStringLiteral("QPushButton { color:#0655bd; text-decoration:underline; border:0; }"));
    footer->addWidget(online);
    root->addLayout(footer);
    m_details = new QLabel(this); m_details->setVisible(false); m_details->setStyleSheet(QStringLiteral("color:#4b4b4b; padding:2px 8px;")); root->addWidget(m_details);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { m_page = 0; rebuild(); });
    connect(m_previous, &QToolButton::clicked, this, [this]() { if (m_page > 0) { --m_page; rebuild(); } });
    connect(m_next, &QToolButton::clicked, this, [this]() { if (m_page < 1) { ++m_page; rebuild(); } });
    // itemActivated already covers the platform double-click gesture and
    // keyboard activation. Connecting itemDoubleClicked as well adds twice.
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *) { addCurrent(); });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &GadgetGallery::showItemMenu);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        m_details->setText(current ? current->text() + QStringLiteral(" — Double-click or drag to add this gadget.") : QString());
    });
    connect(detailsButton, &QToolButton::toggled, this, [this, detailsButton](bool checked) {
        m_details->setVisible(checked); detailsButton->setText(checked ? QStringLiteral("⌃  Hide details") : QStringLiteral("⌄  Show details"));
    });
    connect(online, &QPushButton::clicked, this, []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/aero7-open-project"))); });
    rebuild();
}

void GadgetGallery::rebuild()
{
    m_list->clear();
    const QString query = m_search->text().trimmed();
    QList<GadgetDefinition> filtered;
    for (const auto &definition : m_manager->definitions()) {
        if (query.isEmpty() || definition.name.contains(query, Qt::CaseInsensitive)) filtered << definition;
    }
    const bool searching = !query.isEmpty();
    const int pages = searching ? 1 : 2;
    if (m_page >= pages) m_page = pages - 1;
    m_pageLabel->setText(QStringLiteral("Page %1 of %2").arg(m_page + 1).arg(pages));
    m_previous->setEnabled(m_page > 0);
    m_next->setEnabled(m_page + 1 < pages);
    for (int i = 0; i < filtered.size(); ++i) {
        if (!searching && ((m_page == 0 && i >= 8) || (m_page == 1 && i < 8))) continue;
        const GadgetDefinition &definition = filtered.at(i);
        auto *item = new QListWidgetItem(GadgetPainter::preview(definition, QSize(66, 66)), definition.name, m_list);
        item->setData(idRole, definition.id);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setToolTip(QStringLiteral("Double-click to add %1").arg(definition.name));
    }
    if (m_list->count()) m_list->setCurrentRow(0);
}

void GadgetGallery::addCurrent()
{
    if (QListWidgetItem *item = m_list->currentItem()) m_manager->AddGadget(item->data(idRole).toString());
}

void GadgetGallery::showItemMenu(const QPoint &position)
{
    QListWidgetItem *item = m_list->itemAt(position);
    if (!item) return;
    m_list->setCurrentItem(item);
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral("QMenu { background:#f5f5f5; border:1px solid #777; font:9pt 'Segoe UI'; } QMenu::item { padding:5px 32px 5px 24px; } QMenu::item:selected { background:#dceeff; }"));
    QAction *add = menu.addAction(QStringLiteral("Add"));
    connect(add, &QAction::triggered, this, &GadgetGallery::addCurrent);
    menu.exec(m_list->viewport()->mapToGlobal(position));
}
