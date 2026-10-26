#pragma once
#include <QWidget>

#include "core/Timeline.h"

// Dibuja la línea de tiempo del modelo (solo lectura en esta fase: aún no hay
// interacción de arrastre). Sirve para comprobar el modelo y la ventana en CI.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    void setTimeline(const nexo::Timeline& tl);
    QSize minimumSizeHint() const override { return {400, 160}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    nexo::Timeline timeline_;
};
