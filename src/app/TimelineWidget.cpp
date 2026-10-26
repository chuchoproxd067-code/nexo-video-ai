#include "TimelineWidget.h"

#include <QPainter>
#include <algorithm>

#include "Theme.h"

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
}

void TimelineWidget::setTimeline(const nexo::Timeline& tl) {
    timeline_ = tl;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), theme::panel);

    const int headerW = 90;
    const int rowH = 36;
    const nexo::Frame total = std::max<nexo::Frame>(timeline_.duration(), 1);
    const double pxPerFrame = double(width() - headerW - 12) / double(total);

    for (std::size_t t = 0; t < timeline_.trackCount(); ++t) {
        const nexo::Track& track = timeline_.track(t);
        const int y = int(t) * (rowH + 4) + 8;
        p.fillRect(QRect(0, y, headerW, rowH), theme::panelHi);
        p.setPen(theme::text);
        p.drawText(QRect(8, y, headerW - 8, rowH), Qt::AlignVCenter,
                   QString::fromStdString(track.name));
        for (const nexo::Clip& c : track.clips) {
            const QRect r(headerW + int(c.timelineStart * pxPerFrame), y,
                          std::max(2, int(c.duration * pxPerFrame)), rowH);
            p.fillRect(r, track.kind == nexo::TrackKind::Video ? theme::clipVideo
                                                               : theme::clipAudio);
            p.setPen(theme::accent);
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
    }
}
