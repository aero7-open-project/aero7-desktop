// SPDX-License-Identifier: GPL-3.0-or-later
#include "VaultWindow.h"
#include "VaultBackend.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

VaultWindow::VaultWindow(VaultBackend *backend) : m_backend(backend)
{
    setWindowTitle(tr("Credential Manager"));
    resize(760, 500);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    auto *title = new QLabel(tr("Manage your credentials"));
    title->setStyleSheet("font-size: 20px; color: #1f4e99;");
    layout->addWidget(title);
    auto *description = new QLabel(tr("Store generic credentials in an encrypted KWallet vault. This does not import browser passwords or automatically sign you in to Windows networks. Use a non-empty vault password when prompted."));
    description->setWordWrap(true);
    layout->addWidget(description);
    m_status = new QLabel;
    m_status->setObjectName("vaultStatus");
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_entries = new QTreeWidget;
    m_entries->setObjectName("credentials");
    m_entries->setHeaderLabels({tr("Internet or network address"), tr("User name")});
    m_entries->setRootIsDecorated(false);
    m_entries->setSelectionMode(QAbstractItemView::SingleSelection);
    m_entries->setColumnWidth(0, 390);
    layout->addWidget(m_entries, 1);
    auto *buttons = new QHBoxLayout;
    m_unlock = new QPushButton(tr("Unlock vault"));
    m_unlock->setObjectName("unlockVault");
    m_add = new QPushButton(tr("Add a generic credential"));
    m_add->setObjectName("addCredential");
    m_edit = new QPushButton(tr("Edit"));
    m_edit->setObjectName("editCredential");
    m_remove = new QPushButton(tr("Remove"));
    m_remove->setObjectName("removeCredential");
    for (auto *button : {m_unlock, m_add, m_edit, m_remove}) buttons->addWidget(button);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(m_backend, &VaultBackend::changed, this, &VaultWindow::refresh);
    connect(m_backend, &VaultBackend::error, this, &VaultWindow::report);
    connect(m_unlock, &QPushButton::clicked, this, [this] {
        if (m_backend->state() == VaultBackend::Unlocked) m_backend->lock();
        else m_backend->unlock(winId());
    });
    connect(m_add, &QPushButton::clicked, this, [this] { edit(true); });
    connect(m_edit, &QPushButton::clicked, this, [this] { edit(false); });
    connect(m_remove, &QPushButton::clicked, this, &VaultWindow::remove);
    connect(m_entries, &QTreeWidget::itemSelectionChanged, this, [this] {
        const bool enabled = m_backend->state() == VaultBackend::Unlocked && m_entries->currentItem();
        m_edit->setEnabled(enabled);
        m_remove->setEnabled(enabled);
    });
    refresh();
}

void VaultWindow::report(const QString &message) { m_status->setText(message); }

void VaultWindow::refresh()
{
    const bool unlocked = m_backend->state() == VaultBackend::Unlocked;
    const QString selected = m_entries->currentItem() ? m_entries->currentItem()->text(0) : QString();
    m_entries->clear();
    m_unlock->setEnabled(m_backend->state() != VaultBackend::Opening);
    m_unlock->setText(unlocked ? tr("Lock vault") : tr("Unlock vault"));
    m_add->setEnabled(unlocked);
    m_edit->setEnabled(false);
    m_remove->setEnabled(false);
    m_status->setText(unlocked ? tr("Your vault is unlocked. Passwords are not displayed in this list.")
        : m_backend->state() == VaultBackend::Opening ? tr("Waiting for the vault service or password prompt…")
        : tr("Your vault is locked. Unlock it to view or change credentials."));
    if (!unlocked) return;
    auto names = m_backend->entries();
    names.sort(Qt::CaseInsensitive);
    bool unreadable = false;
    for (const QString &name : names) {
        Credential value;
        if (!m_backend->read(name, value)) { unreadable = true; continue; }
        auto *item = new QTreeWidgetItem(m_entries, {name, value.user});
        if (name == selected) m_entries->setCurrentItem(item);
    }
    if (unreadable) report(tr("Some entries could not be read or use an unsupported format. They were not changed."));
}

void VaultWindow::edit(bool adding)
{
    if (m_backend->state() != VaultBackend::Unlocked || (!adding && !m_entries->currentItem())) return;
    const QString original = adding ? QString() : m_entries->currentItem()->text(0);
    Credential value;
    if (!adding && !m_backend->read(original, value)) {
        report(tr("This credential could not be read. It was not changed."));
        return;
    }
    if (original.size() > 512 || value.user.size() > 1024 || value.password.size() > 16384) {
        report(tr("This credential exceeds the editor's supported size. It was not changed."));
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName("credentialEditor");
    dialog.setWindowTitle(adding ? tr("Add a generic credential") : tr("Edit credential"));
    auto *form = new QFormLayout(&dialog);
    auto *target = new QLineEdit(original);
    target->setObjectName("target");
    target->setMaxLength(512);
    target->setReadOnly(!adding);
    auto *user = new QLineEdit(value.user);
    user->setObjectName("username");
    user->setMaxLength(1024);
    auto *password = new QLineEdit(value.password);
    password->setObjectName("password");
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(16384);
    value.password.clear();
    form->addRow(tr("Internet or network address:"), target);
    form->addRow(tr("User name:"), user);
    form->addRow(tr("Password:"), password);
    auto *showPassword = new QCheckBox(tr("Show password"));
    showPassword->setObjectName("showPassword");
    form->addRow(QString(), showPassword);
    connect(showPassword, &QCheckBox::toggled, password, [password](bool checked) {
        password->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    const auto validate = [target, buttons] {
        buttons->button(QDialogButtonBox::Save)->setEnabled(!target->text().trimmed().isEmpty());
    };
    connect(target, &QLineEdit::textChanged, &dialog, validate);
    validate();
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    // Never retain an editor containing a password after an external lock.
    connect(m_backend, &VaultBackend::changed, &dialog, [&] {
        if (m_backend->state() != VaultBackend::Unlocked) dialog.reject();
    });
    if (dialog.exec() != QDialog::Accepted) { password->clear(); return; }
    if (m_backend->state() != VaultBackend::Unlocked) { password->clear(); return; }
    const QString name = adding ? target->text().trimmed() : original;
    if (name.isEmpty()) { password->clear(); return; }
    if (adding && m_backend->entries().contains(name)) {
        password->clear();
        report(tr("That address already exists. Select it and choose Edit; nothing was overwritten."));
        return;
    }
    const bool saved = m_backend->write(name, {user->text(), password->text()});
    password->clear();
    if (!saved) report(tr("The credential could not be saved. Check the vault connection and try again."));
}

void VaultWindow::remove()
{
    if (m_backend->state() != VaultBackend::Unlocked || !m_entries->currentItem()) return;
    const QString target = m_entries->currentItem()->text(0);
    QMessageBox confirm(QMessageBox::Question, tr("Remove credential"),
        tr("Remove the saved credential for %1? This cannot be undone.").arg(target),
        QMessageBox::Yes | QMessageBox::No, this);
    confirm.setTextFormat(Qt::PlainText);
    confirm.setDefaultButton(QMessageBox::No);
    if (confirm.exec() != QMessageBox::Yes) return;
    if (!m_backend->remove(target)) report(tr("The credential could not be removed. Check the vault connection and try again."));
}
