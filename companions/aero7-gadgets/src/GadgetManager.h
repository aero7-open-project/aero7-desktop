#pragma once

#include "GadgetTypes.h"

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QTimer>

class GadgetGallery;
class GadgetWindow;
class QScreen;

class GadgetManager final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.aero7.GadgetManager")

public:
    explicit GadgetManager(QObject *parent = nullptr);
    ~GadgetManager() override;

    static QList<GadgetDefinition> builtinDefinitions();
    const QList<GadgetDefinition> &definitions() const { return m_definitions; }
    const GadgetDefinition *definition(const QString &id) const;
    QString addGadgetAt(const QString &id, const QPoint &globalPosition);

    void restoreSession();
    void scheduleSave();
    QPoint snappedPosition(const GadgetWindow *moving, const QPoint &requested) const;
    QScreen *screenForName(const QString &name) const;

public slots:
    Q_SCRIPTABLE void ShowGallery();
    Q_SCRIPTABLE QString AddGadget(const QString &id);
    Q_SCRIPTABLE void RemoveGadget(const QString &instance);
    Q_SCRIPTABLE void ResetLayout();

signals:
    Q_SCRIPTABLE void LayoutChanged();

private slots:
    void saveSession();
    void handleScreensChanged();

private:
    GadgetState defaultState(const GadgetDefinition &definition, int sequence) const;
    QJsonObject defaultSettings(const QString &id) const;
    GadgetWindow *createWindow(GadgetState state);
    QString layoutPath() const;
    QString uniqueInstance(const QString &id) const;

    QList<GadgetDefinition> m_definitions;
    QList<GadgetWindow *> m_windows;
    GadgetGallery *m_gallery = nullptr;
    QTimer m_saveTimer;
    bool m_restoring = false;
};
