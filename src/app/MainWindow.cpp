#include "MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QListWidget>
#include <QStatusBar>
#include <QToolBar>

#include "BuildInfo.h"
#include "Theme.h"
#include "TimelineWidget.h"

namespace {
nexo::Timeline demoTimeline() {
    nexo::Timeline tl({30, 1});
    const auto v1 = tl.addTrack("Video 1", nexo::TrackKind::Video);
    const auto a1 = tl.addTrack("Audio 1", nexo::TrackKind::Audio);
    nexo::Clip c;
    c.mediaPath = "demo.mp4";   // marcador: todavía no hay importación real
    c.duration = 150;
    tl.addClip(v1, c);
    c.timelineStart = 180;
    c.duration = 90;
    tl.addClip(v1, c);
    c.timelineStart = 0;
    c.duration = 270;
    tl.addClip(a1, c);
    return tl;
}
}  // namespace

MainWindow::MainWindow() : history_(demoTimeline()) {
    setWindowTitle(QString("NEXO VIDEO AI %1").arg(NEXO_VERSION));
    resize(1280, 760);
    setStyleSheet(QString(
        "QMainWindow, QDockWidget, QStatusBar, QToolBar { background:%1; color:%2; }"
        "QListWidget, QLabel { background:%3; color:%2; }"
        "QToolButton { color:%2; padding:4px 10px; }"
        "QToolButton:hover { border:1px solid %4; }")
        .arg(theme::bg.name(), theme::text.name(), theme::panel.name(), theme::accent.name()));

    auto* tb = addToolBar("Principal");
    tb->setMovable(false);
    auto* split = tb->addAction("Dividir (demo)");
    auto* undo = tb->addAction("Deshacer");
    auto* redo = tb->addAction("Rehacer");

    auto* library = new QDockWidget("Biblioteca", this);
    library->setWidget(new QListWidget);   // la importación llega en la Fase 3
    addDockWidget(Qt::LeftDockWidgetArea, library);

    auto* viewer = new QLabel("Visor: no disponible todavía (Fase 3)");
    viewer->setAlignment(Qt::AlignCenter);
    setCentralWidget(viewer);

    auto* tlDock = new QDockWidget("Línea de tiempo", this);
    timeline_ = new TimelineWidget;
    tlDock->setWidget(timeline_);
    addDockWidget(Qt::BottomDockWidgetArea, tlDock);

    connect(split, &QAction::triggered, this, [this] {
        const auto& clips = history_.timeline().track(0).clips;
        if (clips.empty()) return;
        const nexo::Clip first = clips.front();
        const nexo::Frame at = first.timelineStart + first.duration / 2;
        const auto r = history_.apply("Dividir", [&](nexo::Timeline& t) {
            return t.splitClip(first.id, at);
        });
        statusBar()->showMessage(r.ok ? "Clip dividido" : QString::fromStdString(r.error), 4000);
        refresh();
    });
    connect(undo, &QAction::triggered, this, [this] { history_.undo(); refresh(); });
    connect(redo, &QAction::triggered, this, [this] { history_.redo(); refresh(); });

    statusBar()->showMessage(QString("Compilación %1 — editor en construcción (Fase 2)")
                                 .arg(NEXO_GIT_COMMIT));
    refresh();
}

void MainWindow::refresh() {
    timeline_->setTimeline(history_.timeline());
}
