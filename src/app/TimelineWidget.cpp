#include "TimelineWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

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
    drag_ = Drag::None;
    if (hit != 0 && row >= 0) {
        for (const nexo::Clip& cl : timeline_.track(std::size_t(row)).clips) {
            if (cl.id != hit) continue;
            const double left = kHeaderW + cl.timelineStart * ppf, right = kHeaderW + cl.timelineEnd() * ppf;
            const double x = e->position().x();
            drag_ = (x - left < 6 && right - left > 14) ? Drag::TrimStart
                  : (right - x < 6 && right - left > 14) ? Drag::TrimEnd : Drag::Move;
        }
        dragTrack_ = dragRow_ = row;
        dragStartX_ = e->position().x();
        dragDelta_ = 0;
        dragMoved_ = false;
    }
    update();
}

int TimelineWidget::rowAt(int y) const {
    const int r = (y - 8) / (kRowH + 4);
    return (y >= 8 && r >= 0 && std::size_t(r) < timeline_.trackCount()) ? r : -1;
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    if (drag_ == Drag::None) return;
    const double ppf = pxPerFrame();
    if (ppf <= 0) return;
    dragDelta_ = nexo::Frame(std::llround((e->position().x() - dragStartX_) / ppf));
    if (dragDelta_ != 0) dragMoved_ = true;
    const int r = rowAt(int(e->position().y()));
    if (drag_ == Drag::Move && r >= 0 && timeline_.track(std::size_t(r)).kind == timeline_.track(std::size_t(dragTrack_)).kind)
        dragRow_ = r;
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent*) {
    const Drag d = drag_;
    drag_ = Drag::None;
    const nexo::Frame delta = dragDelta_;
    const bool trackChanged = dragRow_ != dragTrack_;
    update();
    if (d == Drag::None || selected_ == 0 || (!dragMoved_ && !trackChanged)) return;
    if (d == Drag::Move) {
        for (const nexo::Clip& cl : timeline_.track(std::size_t(dragTrack_)).clips)
            if (cl.id == selected_) { emit moveRequested(selected_, dragRow_, std::max<nexo::Frame>(0, cl.timelineStart + delta)); break; }
    } else if (delta != 0) {
        if (d == Drag::TrimStart) emit trimStartRequested(selected_, delta);
        else emit trimEndRequested(selected_, delta);
    }
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
            nexo::Frame start = c.timelineStart, len = c.duration;
            int yy = y;
            if (drag_ != Drag::None && c.id == selected_) {
                if (drag_ == Drag::Move) { start = std::max<nexo::Frame>(0, start + dragDelta_); yy = dragRow_ * (rowH + 4) + 8; }
                else if (drag_ == Drag::TrimStart) { start += dragDelta_; len -= dragDelta_; }
                else len += dragDelta_;
                len = std::max<nexo::Frame>(1, len);
            }
            const QRect r(headerW + int(start * pxPerFrame), yy, std::max(2, int(len * pxPerFrame)), rowH);
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
