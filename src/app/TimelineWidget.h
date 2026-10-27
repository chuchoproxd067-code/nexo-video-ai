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

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    double pxPerFrame() const;
    nexo::Timeline timeline_;
    nexo::ClipId selected_ = 0;
    nexo::Frame playhead_ = 0;
};
