#pragma once

#include "GadgetTypes.h"

#include <QDialog>
#include <QJsonObject>
#include <QMap>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QNetworkAccessManager;

class GadgetOptionsDialog final : public QDialog
{
    Q_OBJECT

public:
    GadgetOptionsDialog(const GadgetDefinition &definition, const QJsonObject &settings, QWidget *parent = nullptr,
                        QNetworkAccessManager *lookupNetwork = nullptr);
    QJsonObject settings() const;
    bool newPuzzleRequested() const { return m_newPuzzleRequested; }

private:
    GadgetDefinition m_definition;
    QJsonObject m_original;
    bool m_newPuzzleRequested = false;
    QNetworkAccessManager *m_lookupNetwork = nullptr;
    quint64 m_locationRevision = 0;
    QMap<QString, QLineEdit *> m_lines;
    QMap<QString, QComboBox *> m_combos;
    QMap<QString, QSpinBox *> m_spins;
    QMap<QString, QDoubleSpinBox *> m_doubles;
    QMap<QString, QCheckBox *> m_checks;
};
