#pragma once
#include <QWidget>

#include "core/Timeline.h"

// Dibuja la línea de tiempo. Clic en un clip lo selecciona; clic en cualquier punto mueve el cursor.
// No modifica el modelo: la ventana principal aplica los cambios por History.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    void setTimeline(const nexo::Timeline& tl);
    QSize minimumSizeHint() const override { return {400, 160}; }
    nexo::ClipId selectedClip() const { return selected_; }
    nexo::Frame playhead() const { return playhead_; }

signals:
    void selectionChanged(nexo::ClipId id);   // 0 = ninguno
    void playheadChanged(nexo::Frame frame);
    // Peticiones de edición por arrastre; la ventana las aplica con History (deshacer incluido).
    void moveRequested(nexo::ClipId id, int toTrack, nexo::Frame newStart);
    void trimStartRequested(nexo::ClipId id, nexo::Frame delta);
    void trimEndRequested(nexo::ClipId id, nexo::Frame delta);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    enum class Drag { None, Move, TrimStart, TrimEnd };
    double pxPerFrame() const;
    int rowAt(int y) const;
    Drag drag_ = Drag::None;
    int dragTrack_ = 0;            // pista original del clip
    int dragRow_ = 0;              // pista bajo el ratón
    double dragStartX_ = 0;
    nexo::Frame dragDelta_ = 0;
    bool dragMoved_ = false;
    nexo::Timeline timeline_;
    nexo::ClipId selected_ = 0;
    nexo::Frame playhead_ = 0;
};
