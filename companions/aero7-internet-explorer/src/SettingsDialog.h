#pragma once

#include <aero7compat/InternetExplorer.h>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum class Reason { Settings, FirstLaunch, BackendMissing };

    SettingsDialog(Aero7::Compat::InternetExplorer &internetExplorer,
                   Reason reason, QString missingDesktopId = {},
                   QWidget *parent = nullptr);

    [[nodiscard]] QString selectedDesktopId() const;

protected:
    void accept() override;

private:
    Aero7::Compat::InternetExplorer &m_internetExplorer;
    QComboBox *m_browser = nullptr;
    QCheckBox *m_makeDefault = nullptr;
    QLabel *m_status = nullptr;
};

int showNoBrowserDialog(QWidget *parent = nullptr);
