#pragma once
#include <QWidget>

#include "core/Timeline.h"

// Dibuja la línea de tiempo (regla, marcadores, pistas con botones, clips). Clic en un clip lo
// selecciona; clic en cualquier punto mueve el cursor. Rueda = desplazar; Ctrl+rueda = zoom.
// No modifica el modelo: la ventana principal aplica los cambios por History.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    void setTimeline(const nexo::Timeline& tl);
    QSize minimumSizeHint() const override { return {400, 200}; }
    nexo::ClipId selectedClip() const { return selected_; }
    nexo::Frame playhead() const { return playhead_; }
    // Pista del clip seleccionado (-1 si no hay selección) o de la última pista pulsada.
    int currentTrack() const { return currentTrack_; }
    void setSnap(bool on) { snap_ = on; }
    bool snap() const { return snap_; }
    void zoomIn() { zoomBy(1.5); }
    void zoomOut() { zoomBy(1.0 / 1.5); }
    void zoomFit();

signals:
    void selectionChanged(nexo::ClipId id);   // 0 = ninguno
    void playheadChanged(nexo::Frame frame);
    // Peticiones de edición por arrastre; la ventana las aplica con History (deshacer incluido).
    void moveRequested(nexo::ClipId id, int toTrack, nexo::Frame newStart);
    void trimStartRequested(nexo::ClipId id, nexo::Frame delta);
    void trimEndRequested(nexo::ClipId id, nexo::Frame delta);
    void clipActivated(nexo::ClipId id);                       // doble clic
    void contextRequested(nexo::ClipId id, int track, const QPoint& globalPos);
    void trackFlagsRequested(int track, bool muted, bool hidden, bool locked);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    enum class Drag { None, Move, TrimStart, TrimEnd };
    int xOf(nexo::Frame f) const;
    nexo::Frame frameAt(double x) const;
    int trackRow(int y) const;                    // -1 si no cae en una pista
    int trackTop(int row) const;
    nexo::ClipId clipAt(int row, nexo::Frame f) const;
    nexo::Frame snapped(nexo::Frame f, nexo::ClipId ignore) const;
    void zoomBy(double factor);
    void clampOrigin();
    Drag drag_ = Drag::None;
    int dragTrack_ = 0;            // pista original del clip
    int dragRow_ = 0;              // pista bajo el ratón
    double dragStartX_ = 0;
    nexo::Frame dragDelta_ = 0;
    bool dragMoved_ = false;
    nexo::Timeline timeline_;
    nexo::ClipId selected_ = 0;
    nexo::Frame playhead_ = 0;
    int currentTrack_ = -1;
    double ppf_ = 4.0;             // píxeles por fotograma
    nexo::Frame origin_ = 0;       // primer fotograma visible
    bool userZoom_ = false;
    bool snap_ = true;
};
