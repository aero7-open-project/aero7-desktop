#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Aero7::Compat;

SettingsDialog::SettingsDialog(InternetExplorer &internetExplorer,
                               Reason reason, QString missingDesktopId,
                               QWidget *parent)
    : QDialog(parent)
    , m_internetExplorer(internetExplorer)
{
    setWindowTitle(QStringLiteral("Internet Explorer"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("aero7-internet-explorer")));
    setFixedWidth(600);
    setStyleSheet(QStringLiteral(
        "SettingsDialog { background: #F0F0F0; }"
        "QLabel#heading { color: #164B7A; font-size: 13pt; }"
        "QLabel#status { color: #A00000; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("aero7-internet-explorer"))
                        .pixmap(48, 48));
    header->addWidget(icon, 0, Qt::AlignTop);
    auto *headerText = new QVBoxLayout;
    auto *heading = new QLabel(reason == Reason::Settings
        ? QStringLiteral("Internet Explorer Settings")
        : QStringLiteral("Internet Explorer"));
    heading->setObjectName(QStringLiteral("heading"));
    headerText->addWidget(heading);
    QString explanation;
    if (reason == Reason::BackendMissing) {
        explanation = QStringLiteral(
            "The browser previously used by Internet Explorer (%1) is no longer installed. Choose another browser.")
            .arg(missingDesktopId.toHtmlEscaped());
    } else if (reason == Reason::FirstLaunch) {
        explanation = QStringLiteral(
            "Choose the browser Internet Explorer should use.");
    } else {
        explanation = QStringLiteral(
            "Choose the modern browser used behind the permanent Internet Explorer identity.");
    }
    auto *description = new QLabel(explanation);
    description->setWordWrap(true);
    headerText->addWidget(description);
    header->addLayout(headerText, 1);
    layout->addLayout(header);

    auto *selector = new QHBoxLayout;
    selector->addWidget(new QLabel(QStringLiteral("Browser used by Internet Explorer:")));
    m_browser = new QComboBox;
    m_browser->setObjectName(QStringLiteral("browserBackend"));
    for (const BrowserBackend &backend : internetExplorer.installedBrowsers()) {
        m_browser->addItem(QIcon::fromTheme(backend.iconName),
                           backend.displayName, backend.desktopId);
    }
    const BrowserBackend selected = internetExplorer.selectedBackend();
    const int selectedIndex = m_browser->findData(selected.desktopId);
    if (selectedIndex >= 0) m_browser->setCurrentIndex(selectedIndex);
    m_browser->setEnabled(internetExplorer.config().mayChangeBackend());
    selector->addWidget(m_browser, 1);
    layout->addLayout(selector);

    m_makeDefault = new QCheckBox(
        QStringLiteral("Make Internet Explorer my default web browser"));
    m_makeDefault->setChecked(
        internetExplorer.defaultApplications().isInternetExplorerDefault());
    layout->addWidget(m_makeDefault);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    if (!internetExplorer.config().mayChangeBackend()) {
        m_status->setText(QStringLiteral(
            "An administrator policy controls the browser used by Internet Explorer."));
    }
    layout->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Save);
    auto *more = buttons->addButton(QStringLiteral("Get more browsers"),
                                    QDialogButtonBox::ActionRole);
    auto *shortcut = buttons->addButton(QStringLiteral("Create desktop shortcut"),
                                        QDialogButtonBox::ActionRole);
    connect(more, &QPushButton::clicked, this, [] {
        InternetExplorer::openProgramsCenter();
    });
    connect(shortcut, &QPushButton::clicked, this, [this] {
        QString error;
        if (!m_internetExplorer.createDesktopShortcut(&error)) {
            QMessageBox::warning(this, QStringLiteral("Internet Explorer"), error);
        } else {
            m_status->setStyleSheet(QStringLiteral("color: #1B6E1B;"));
            m_status->setText(QStringLiteral("The Internet Explorer shortcut was added to the desktop."));
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);
}

QString SettingsDialog::selectedDesktopId() const
{
    return m_browser->currentData().toString();
}

void SettingsDialog::accept()
{
    if (m_browser->count() == 0) {
        m_status->setText(QStringLiteral("No compatible browser is installed."));
        return;
    }
    QString error;
    if (m_internetExplorer.config().mayChangeBackend()
        && !m_internetExplorer.setBackend(selectedDesktopId(), &error)) {
        m_status->setText(error);
        return;
    }
    if (m_makeDefault->isChecked()) {
        if (!m_internetExplorer.defaultApplications()
                 .setInternetExplorerDefault(&error)) {
            m_status->setText(error);
            return;
        }
    } else if (m_internetExplorer.defaultApplications()
                   .isInternetExplorerDefault()) {
        if (!m_internetExplorer.defaultApplications().restorePreviousDefaults(&error)
            && !error.isEmpty()) {
            m_status->setText(error);
            return;
        }
    }
    QDialog::accept();
}

int showNoBrowserDialog(QWidget *parent)
{
    QMessageBox message(parent);
    message.setWindowTitle(QStringLiteral("Internet Explorer"));
    message.setWindowIcon(QIcon::fromTheme(QStringLiteral("aero7-internet-explorer")));
    message.setIcon(QMessageBox::Warning);
    message.setText(QStringLiteral("A web browser could not be found on this computer."));
    message.setInformativeText(QStringLiteral(
        "Install a browser using Programs Center and try again."));
    auto *programs = message.addButton(QStringLiteral("Open Programs Center"),
                                       QMessageBox::ActionRole);
    message.addButton(QMessageBox::Close);
    message.exec();
    if (message.clickedButton() == programs) {
        InternetExplorer::openProgramsCenter();
    }
    return 1;
}
