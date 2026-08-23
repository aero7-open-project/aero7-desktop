#pragma once

#include "GadgetPainter.h"
#include "GadgetTypes.h"

#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

class GadgetManager;
class QScreen;

namespace LayerShellQt { class Window; }

class GadgetWindow final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal controlsOpacity READ controlsOpacity WRITE setControlsOpacity)

public:
    GadgetWindow(const GadgetDefinition &definition, GadgetState state, GadgetManager *manager);
    ~GadgetWindow() override = default;

    QString instanceId() const { return m_state.instance; }
    QSize bodySize() const;
    QRect bodyGeometry() const;
    GadgetState stateForSave() const;
    void placeOnDesktopLayer(QScreen *screen, const QPoint &globalPosition);
    void setDesktopPosition(const QPoint &globalPosition);
    void notifyGeometryChanged();

    qreal controlsOpacity() const { return m_controlsOpacity; }
    void setControlsOpacity(qreal opacity);

signals:
    void stateChanged();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void updateNetworkData(bool force = false);
    void nextSlide();
    void previousSlide();

private:
    enum class Control { None, Close, Size, Options, Drag };

    QRect bodyRect() const;
    QRect controlRect(Control control) const;
    Control controlAt(const QPoint &position) const;
    void applyWindowLayer();
    void updateWindowSize();
    void showOptions();
    void showContextMenu(const QPoint &globalPosition);
    void setSizeMode(const QString &mode);
    void setOpacityPercent(int percent);
    void setAlwaysOnTop(bool enabled);
    void initializeRuntime();
    void initializePuzzle();
    void movePuzzleTile(const QPoint &position);
    void loadSlides();
    void showSlideAt(int index);
    void handleCalendarClick(const QPoint &position);
    void handleCurrencyClick(const QPoint &position);
    bool handleSlideClick(const QPoint &position);
    void openFeedItem(const QPoint &position);
    void handleMediaClick(const QPoint &position);
    void paintControls(QPainter &painter);

    GadgetDefinition m_definition;
    GadgetState m_state;
    GadgetManager *m_manager = nullptr;
    GadgetRenderData m_data;
    bool m_dragging = false;
    QPoint m_dragOffset;
    QPoint m_dragStartPosition;
    Control m_pressedControl = Control::None;
    qreal m_controlsOpacity = 0.0;
    QPropertyAnimation m_controlsAnimation;
    QTimer m_repaintTimer;
    QTimer m_networkTimer;
    QTimer m_slideTimer;
    QVariantAnimation m_slideAnimation;
    QStringList m_slideFiles;
    int m_slideIndex = 0;
    bool m_slidePaused = false;
    LayerShellQt::Window *m_layerShell = nullptr;
    QPointer<QScreen> m_desktopScreen;
    QPoint m_desktopPosition;
};
