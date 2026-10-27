#include "TimelineWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>

#include "Theme.h"

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
}

void TimelineWidget::setTimeline(const nexo::Timeline& tl) {
    timeline_ = tl;
    bool found = false;
    for (std::size_t t = 0; t < tl.trackCount() && !found; ++t)
        for (const nexo::Clip& cl : tl.track(t).clips) if (cl.id == selected_) found = true;
    if (!found && selected_ != 0) { selected_ = 0; emit selectionChanged(0); }
    playhead_ = std::min(playhead_, tl.duration());
    update();
}

namespace { constexpr int kHeaderW = 90; constexpr int kRowH = 36; }

double TimelineWidget::pxPerFrame() const {
    return double(width() - kHeaderW - 12) / double(std::max<nexo::Frame>(timeline_.duration(), 1));
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) {
    const double ppf = pxPerFrame();
    if (e->position().x() < kHeaderW || ppf <= 0) return;
    const nexo::Frame f = std::clamp<nexo::Frame>(nexo::Frame((e->position().x() - kHeaderW) / ppf), 0, timeline_.duration());
    nexo::ClipId hit = 0;
    const int row = (int(e->position().y()) - 8) / (kRowH + 4);
    if (row >= 0 && std::size_t(row) < timeline_.trackCount() && (int(e->position().y()) - 8) % (kRowH + 4) < kRowH)
        for (const nexo::Clip& cl : timeline_.track(std::size_t(row)).clips)
            if (f >= cl.timelineStart && f < cl.timelineEnd()) hit = cl.id;
    playhead_ = f;
    emit playheadChanged(f);
    if (hit != selected_) { selected_ = hit; emit selectionChanged(hit); }
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
            p.setPen(c.id == selected_ ? QPen(Qt::white, 2) : QPen(theme::accent));
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
    }
    const int px = headerW + int(playhead_ * pxPerFrame);
    p.setPen(QPen(theme::accent, 2));
    p.drawLine(px, 0, px, height());
}
