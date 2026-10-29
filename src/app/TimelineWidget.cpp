#include "TimelineWidget.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "Theme.h"

namespace {
constexpr int kHeaderW = 120;
constexpr int kRowH = 38;
constexpr int kGap = 4;
constexpr int kRuler = 22;
constexpr int kBtn = 14;
constexpr int kSnapPx = 8;

QRect flagRect(int top, int i) { return QRect(kHeaderW - 3 * (kBtn + 2) + i * (kBtn + 2) - 2, top + 3, kBtn, kBtn); }
}  // namespace

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setMouseTracking(false);
}

int TimelineWidget::xOf(nexo::Frame f) const { return kHeaderW + int(std::lround((double(f) - double(origin_)) * ppf_)); }
nexo::Frame TimelineWidget::frameAt(double x) const { return origin_ + nexo::Frame(std::floor((x - kHeaderW) / ppf_)); }
int TimelineWidget::trackTop(int row) const { return kRuler + 4 + row * (kRowH + kGap); }

int TimelineWidget::trackRow(int y) const {
    const int r = (y - kRuler - 4) / (kRowH + kGap);
    if (y < kRuler + 4 || r < 0 || std::size_t(r) >= timeline_.trackCount()) return -1;
    return (y - kRuler - 4) % (kRowH + kGap) < kRowH ? r : -1;
}

nexo::ClipId TimelineWidget::clipAt(int row, nexo::Frame f) const {
    if (row < 0) return 0;
    for (const nexo::Clip& cl : timeline_.track(std::size_t(row)).clips)
        if (f >= cl.timelineStart && f < cl.timelineEnd()) return cl.id;
    return 0;
}

void TimelineWidget::clampOrigin() {
    const nexo::Frame total = std::max<nexo::Frame>(timeline_.duration(), 1);
    const nexo::Frame visible = nexo::Frame((width() - kHeaderW - 12) / ppf_);
    origin_ = std::clamp<nexo::Frame>(origin_, 0, std::max<nexo::Frame>(0, total + 30 - visible));
}

void TimelineWidget::zoomFit() {
    const nexo::Frame total = std::max<nexo::Frame>(timeline_.duration(), 1);
    ppf_ = std::clamp(double(width() - kHeaderW - 12) / double(total), 0.2, 60.0);
    origin_ = 0;
    userZoom_ = false;
    update();
}

void TimelineWidget::zoomBy(double factor) {
    const nexo::Frame anchor = playhead_;
    const double ax = xOf(anchor);
    ppf_ = std::clamp(ppf_ * factor, 0.2, 60.0);
    origin_ = anchor - nexo::Frame((ax - kHeaderW) / ppf_);
    userZoom_ = true;
    clampOrigin();
    update();
}

void TimelineWidget::resizeEvent(QResizeEvent*) {
    if (!userZoom_) zoomFit();
    else clampOrigin();
}

void TimelineWidget::setTimeline(const nexo::Timeline& tl) {
    timeline_ = tl;
    bool found = false;
    for (std::size_t t = 0; t < tl.trackCount() && !found; ++t)
        for (const nexo::Clip& cl : tl.track(t).clips) if (cl.id == selected_) { found = true; currentTrack_ = int(t); }
    if (!found && selected_ != 0) { selected_ = 0; emit selectionChanged(0); }
    if (currentTrack_ >= int(tl.trackCount())) currentTrack_ = -1;
    playhead_ = std::min(playhead_, tl.duration());
    if (!userZoom_) zoomFit(); else clampOrigin();
    update();
}

nexo::Frame TimelineWidget::snapped(nexo::Frame f, nexo::ClipId ignore) const {
    if (!snap_) return f;
    nexo::Frame best = f;
    double bestD = kSnapPx / ppf_ + 1e-9;
    auto consider = [&](nexo::Frame t) {
        const double d = std::abs(double(t - f));
        if (d < bestD) { bestD = d; best = t; }
    };
    consider(playhead_);
    consider(0);
    for (const nexo::Marker& m : timeline_.markers()) consider(m.frame);
    for (std::size_t t = 0; t < timeline_.trackCount(); ++t)
        for (const nexo::Clip& cl : timeline_.track(t).clips) {
            if (cl.id == ignore) continue;
            consider(cl.timelineStart);
            consider(cl.timelineEnd());
        }
    return best;
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (e->button() != Qt::LeftButton) return;
    // Botones de pista (silenciar / ocultar / bloquear).
    const int hrow = trackRow(pos.y());
    if (pos.x() < kHeaderW) {
        if (hrow >= 0) {
            currentTrack_ = hrow;
            const nexo::Track& tr = timeline_.track(std::size_t(hrow));
            const int top = trackTop(hrow);
            bool m = tr.muted, h = tr.hidden, l = tr.locked;
            if (flagRect(top, 0).contains(pos)) { m = !m; emit trackFlagsRequested(hrow, m, h, l); }
            else if (flagRect(top, 1).contains(pos) && tr.kind == nexo::TrackKind::Video) { h = !h; emit trackFlagsRequested(hrow, m, h, l); }
            else if (flagRect(top, 2).contains(pos)) { l = !l; emit trackFlagsRequested(hrow, m, h, l); }
            update();
        }
        return;
    }
    const nexo::Frame f = std::clamp<nexo::Frame>(frameAt(e->position().x()), 0, std::max<nexo::Frame>(timeline_.duration(), 0));
    const int row = hrow;
    const nexo::ClipId hit = clipAt(row, f);
    if (row >= 0) currentTrack_ = row;
    playhead_ = f;
    emit playheadChanged(playhead_);
    if (hit != selected_) { selected_ = hit; emit selectionChanged(hit); }
    drag_ = Drag::None;
    if (hit != 0 && row >= 0) {
        for (const nexo::Clip& cl : timeline_.track(std::size_t(row)).clips) {
            if (cl.id != hit) continue;
            const double left = xOf(cl.timelineStart), right = xOf(cl.timelineEnd());
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

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    const nexo::Frame f = frameAt(e->position().x());
    const nexo::ClipId hit = clipAt(trackRow(int(e->position().y())), f);
    if (hit != 0 && e->position().x() >= kHeaderW) emit clipActivated(hit);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    if (drag_ == Drag::None) return;
    if (ppf_ <= 0) return;
    nexo::Frame delta = nexo::Frame(std::llround((e->position().x() - dragStartX_) / ppf_));
    // Imán: el borde que se mueve se pega a cursor, marcadores y bordes de otros clips.
    for (const nexo::Clip& cl : timeline_.track(std::size_t(dragTrack_)).clips) {
        if (cl.id != selected_) continue;
        if (drag_ == Drag::Move) {
            const nexo::Frame s0 = cl.timelineStart + delta, e0 = cl.timelineEnd() + delta;
            const nexo::Frame ss = snapped(s0, cl.id), se = snapped(e0, cl.id);
            if (ss != s0) delta += ss - s0;
            else if (se != e0) delta += se - e0;
        } else if (drag_ == Drag::TrimStart) {
            delta += snapped(cl.timelineStart + delta, cl.id) - (cl.timelineStart + delta);
        } else {
            delta += snapped(cl.timelineEnd() + delta, cl.id) - (cl.timelineEnd() + delta);
        }
    }
    dragDelta_ = delta;
    if (dragDelta_ != 0) dragMoved_ = true;
    const int r = trackRow(int(e->position().y()));
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

void TimelineWidget::wheelEvent(QWheelEvent* e) {
    const int dy = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
    if (e->modifiers() & Qt::ControlModifier) {
        if (dy > 0) zoomIn(); else if (dy < 0) zoomOut();
    } else {
        origin_ -= nexo::Frame(std::lround(dy / 120.0 * 60.0 / ppf_));
        userZoom_ = true;
        clampOrigin();
        update();
    }
    e->accept();
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* e) {
    const int row = trackRow(e->pos().y());
    const nexo::ClipId hit = e->pos().x() >= kHeaderW ? clipAt(row, frameAt(e->pos().x())) : 0;
    if (hit != 0 && hit != selected_) { selected_ = hit; emit selectionChanged(hit); update(); }
    if (row >= 0) currentTrack_ = row;
    emit contextRequested(hit, row, e->globalPos());
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), theme::panel);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Regla de tiempo.
    const nexo::FrameRate fr = timeline_.frameRate();
    const double fps = fr.den > 0 ? double(fr.num) / double(fr.den) : 30.0;
    p.fillRect(QRect(kHeaderW, 0, width() - kHeaderW, kRuler), theme::panelHi);
    double stepSec = 1;
    for (double s : {0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0, 300.0, 600.0}) { stepSec = s; if (s * fps * ppf_ >= 70) break; }
    p.setPen(theme::textDim);
    const nexo::Frame f0 = std::max<nexo::Frame>(0, frameAt(kHeaderW));
    const nexo::Frame step = std::max<nexo::Frame>(1, nexo::Frame(std::llround(stepSec * fps)));
    for (nexo::Frame f = (f0 / step) * step; xOf(f) < width(); f += step) {
        const int x = xOf(f);
        if (x < kHeaderW) continue;
        p.drawLine(x, kRuler - 7, x, kRuler);
        const int secs = int(std::llround(double(f) / fps));
        p.drawText(x + 3, 13, QString("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0')));
    }

    for (std::size_t t = 0; t < timeline_.trackCount(); ++t) {
        const nexo::Track& track = timeline_.track(t);
        const int y = trackTop(int(t));
        p.fillRect(QRect(kHeaderW, y, width() - kHeaderW, kRowH), theme::bg);
        p.fillRect(QRect(0, y, kHeaderW, kRowH), int(t) == currentTrack_ ? theme::clipVideo.darker(150) : theme::panelHi);
        p.setPen(theme::text);
        p.drawText(QRect(8, y + 2, kHeaderW - 60, kRowH - 4), Qt::AlignVCenter | Qt::TextWordWrap, QString::fromStdString(track.name));
        const char* labels[3] = {"M", "O", "B"};
        const bool on[3] = {track.muted, track.hidden, track.locked};
        for (int i = 0; i < 3; ++i) {
            if (i == 1 && track.kind == nexo::TrackKind::Audio) continue;
            const QRect br = flagRect(y, i);
            p.fillRect(br, on[i] ? theme::accent : theme::bg);
            p.setPen(on[i] ? theme::bg : theme::textDim);
            p.drawText(br, Qt::AlignCenter, labels[i]);
        }
        p.setClipRect(QRect(kHeaderW, 0, width() - kHeaderW, height()));
        for (const nexo::Clip& c : track.clips) {
            nexo::Frame start = c.timelineStart, len = c.duration;
            int yy = y;
            if (drag_ != Drag::None && c.id == selected_) {
                if (drag_ == Drag::Move) { start = std::max<nexo::Frame>(0, start + dragDelta_); yy = trackTop(dragRow_); }
                else if (drag_ == Drag::TrimStart) { start += dragDelta_; len -= dragDelta_; }
                else len += dragDelta_;
                len = std::max<nexo::Frame>(1, len);
            }
            const int x0 = xOf(start), w = std::max(2, int(std::lround(len * ppf_)));
            const QRect r(x0, yy, w, kRowH);
            QColor col = c.kind == nexo::ClipKind::Text ? QColor(0xb5, 0x3f, 0x8a)
                       : track.kind == nexo::TrackKind::Video ? theme::clipVideo : theme::clipAudio;
            if (track.locked) col = col.darker(170);
            p.fillRect(r, col);
            // Fundidos y fundido cruzado.
            p.setPen(QPen(QColor(255, 255, 255, 140), 1));
            if (c.fadeIn > 0) p.drawLine(x0, yy + kRowH - 1, x0 + int(c.fadeIn * ppf_), yy + 1);
            if (c.fadeOut > 0) p.drawLine(x0 + w - int(c.fadeOut * ppf_), yy + 1, x0 + w, yy + kRowH - 1);
            if (c.transitionIn > 0) p.fillRect(QRect(x0, yy, std::max(2, int(c.transitionIn * ppf_)), kRowH), QColor(255, 255, 255, 70));
            // Etiqueta.
            QString label = c.kind == nexo::ClipKind::Text ? "T  " + QString::fromStdString(c.text.text).replace('\n', ' ')
                                                            : QFileInfo(QString::fromStdString(c.mediaPath)).fileName();
            const nexo::ClipProps& pr = c.props;
            if (pr.freeze) label += "  ❄";
            else if (pr.speed != 1.0 || pr.reverse) label += QString("  %1%2×").arg(pr.reverse ? "◀ " : "").arg(pr.speed);
            if (pr.hasTransform() || !pr.effects.empty() || pr.chroma.enabled || pr.brightness != 0 || pr.contrast != 1 || pr.saturation != 1)
                label += "  ✦";
            if (pr.muted) label += "  🔇";
            p.setPen(Qt::white);
            p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(label, Qt::ElideRight, w - 8));
            p.setPen(c.id == selected_ ? QPen(Qt::white, 2) : QPen(theme::accent));
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
        p.setClipping(false);
    }

    // Marcadores.
    p.setClipRect(QRect(kHeaderW, 0, width() - kHeaderW, height()));
    for (const nexo::Marker& m : timeline_.markers()) {
        const int x = xOf(m.frame);
        p.setPen(QPen(QColor(0xff, 0xc8, 0x3d), 1, Qt::DashLine));
        p.drawLine(x, kRuler, x, height());
        p.setBrush(QColor(0xff, 0xc8, 0x3d));
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygon({QPoint(x - 5, 0), QPoint(x + 5, 0), QPoint(x, 9)}));
        if (!m.name.empty()) { p.setPen(QColor(0xff, 0xc8, 0x3d)); p.drawText(x + 6, 20, QString::fromStdString(m.name)); }
    }
    // Cursor.
    const int px = xOf(playhead_);
    p.setPen(QPen(theme::accent, 2));
    p.drawLine(px, 0, px, height());
}
