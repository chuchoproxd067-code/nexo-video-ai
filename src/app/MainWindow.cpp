#include "MainWindow.h"

#include <algorithm>
#include <functional>

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressDialog>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QStatusBar>
#include <QToolBar>

#include "BuildInfo.h"
#include "Exporter.h"
#include "MediaImporter.h"
#include "Previewer.h"
#include "Theme.h"
#include "TimelineWidget.h"

namespace {

nexo::Timeline emptyTimeline() {
    nexo::Timeline tl({30, 1});
    tl.addTrack("Video 1", nexo::TrackKind::Video);
    tl.addTrack("Audio 1", nexo::TrackKind::Audio);
    return tl;
}

// Primera pista del tipo pedido (-1 si no hay).
int findTrack(const nexo::Timeline& tl, nexo::TrackKind kind) {
    for (std::size_t i = 0; i < tl.trackCount(); ++i)
        if (tl.track(i).kind == kind) return int(i);
    return -1;
}

nexo::Frame trackEnd(const nexo::Timeline& tl, std::size_t track) {
    const auto& clips = tl.track(track).clips;
    return clips.empty() ? 0 : clips.back().timelineEnd();
}

}  // namespace

MainWindow::MainWindow() : history_(emptyTimeline()) {
    setWindowTitle(QString("NEXO VIDEO AI %1").arg(NEXO_VERSION));
    resize(1280, 760);
    setStyleSheet(QString(
        "QMainWindow, QDockWidget, QStatusBar, QToolBar { background:%1; color:%2; }"
        "QListWidget, QLabel { background:%3; color:%2; }"
        "QToolButton { color:%2; padding:4px 10px; }"
        "QToolButton:hover { border:1px solid %4; }")
        .arg(theme::bg.name(), theme::text.name(), theme::panel.name(), theme::accent.name()));

    importer_ = new MediaImporter(this);
    exporter_ = new Exporter(this);
    previewer_ = new Previewer(this);
    importer_->setProjectFps(history_.timeline().frameRate());

    auto* tb = addToolBar("Principal");
    tb->setMovable(false);
    auto* openA = tb->addAction("Abrir…");
    auto* saveA = tb->addAction("Guardar");
    auto* saveAsA = tb->addAction("Guardar como…");
    tb->addSeparator();
    auto* importA = tb->addAction("Importar medios…");
    auto* addA = tb->addAction("Añadir a la línea de tiempo");
    auto* exportA = tb->addAction("Exportar…");
    tb->addSeparator();
    auto* split = tb->addAction("Dividir en el cursor");
    auto* del = tb->addAction("Eliminar clip");
    del->setShortcut(QKeySequence::Delete);
    split->setShortcut(Qt::Key_S);
    auto* undo = tb->addAction("Deshacer");
    auto* redo = tb->addAction("Rehacer");

    auto* libraryDock = new QDockWidget("Biblioteca", this);
    library_ = new QListWidget;
    libraryDock->setWidget(library_);
    addDockWidget(Qt::LeftDockWidgetArea, libraryDock);

    viewer_ = new QLabel(importer_->available()
                                  ? "Importa medios, añádelos a la línea de tiempo (doble clic en la biblioteca) y exporta."
                                  : "Esta compilación no incluye melt: no se pueden leer medios ni exportar.");
    viewer_->setAlignment(Qt::AlignCenter);
    viewer_->setMinimumSize(320, 180);
    viewer_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setCentralWidget(viewer_);

    auto* tlDock = new QDockWidget("Línea de tiempo", this);
    timeline_ = new TimelineWidget;
    tlDock->setWidget(timeline_);
    addDockWidget(Qt::BottomDockWidgetArea, tlDock);

    connect(openA, &QAction::triggered, this, [this] { open(); });
    connect(saveA, &QAction::triggered, this, [this] { save(false); });
    connect(saveAsA, &QAction::triggered, this, [this] { save(true); });
    connect(importA, &QAction::triggered, this, [this] { importMedia(); });
    connect(addA, &QAction::triggered, this, [this] {
        if (library_->currentItem()) addToTimeline(library_->currentItem());
        else statusBar()->showMessage("Selecciona un medio de la biblioteca primero.", 4000);
    });
    connect(library_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) { addToTimeline(it); });
    connect(exportA, &QAction::triggered, this, [this] { exportVideo(); });
    connect(split, &QAction::triggered, this, [this] {
        const nexo::ClipId id = timeline_->selectedClip();
        if (id == 0) { statusBar()->showMessage("Selecciona un clip (clic) y coloca el cursor donde dividir.", 5000); return; }
        const nexo::Frame at = timeline_->playhead();
        const auto r = history_.apply("Dividir", [&](nexo::Timeline& t) { return t.splitClip(id, at); });
        statusBar()->showMessage(r.ok ? "Clip dividido" : QString::fromStdString(r.error), 5000);
        refresh();
    });
    connect(del, &QAction::triggered, this, [this] {
        const nexo::ClipId id = timeline_->selectedClip();
        if (id == 0) { statusBar()->showMessage("Selecciona un clip para eliminarlo.", 4000); return; }
        const auto r = history_.apply("Eliminar clip", [&](nexo::Timeline& t) { return t.removeClip(id); });
        statusBar()->showMessage(r.ok ? "Clip eliminado" : QString::fromStdString(r.error), 4000);
        refresh();
    });
    connect(undo, &QAction::triggered, this, [this] { history_.undo(); refresh(); });
    connect(redo, &QAction::triggered, this, [this] { history_.redo(); refresh(); });

    auto applyEdit = [this](const char* label, std::function<nexo::Result(nexo::Timeline&)> op) {
        const auto r = history_.apply(label, op);
        statusBar()->showMessage(r.ok ? QString::fromUtf8(label) : QString::fromStdString(r.error), 5000);
        refresh();
    };
    connect(timeline_, &TimelineWidget::moveRequested, this, [applyEdit](nexo::ClipId id, int tr, nexo::Frame s) {
        applyEdit("Mover clip", [=](nexo::Timeline& t) { return t.moveClip(id, std::size_t(tr), s); });
    });
    connect(timeline_, &TimelineWidget::trimStartRequested, this, [applyEdit](nexo::ClipId id, nexo::Frame d) {
        applyEdit("Recortar inicio", [=](nexo::Timeline& t) { return t.trimStart(id, d); });
    });
    connect(timeline_, &TimelineWidget::trimEndRequested, this, [applyEdit](nexo::ClipId id, nexo::Frame d) {
        applyEdit("Recortar final", [=](nexo::Timeline& t) { return t.trimEnd(id, d); });
    });
    connect(timeline_, &TimelineWidget::playheadChanged, this, [this](nexo::Frame) { requestPreview(); });
    connect(previewer_, &Previewer::ready, this, [this](const QImage& img, nexo::Frame) {
        frame_ = img;
        if (img.isNull()) viewer_->setText("Línea de tiempo vacía");
        else showFrame();
    });
    connect(previewer_, &Previewer::failed, this, [this](const QString& msg) {
        statusBar()->showMessage("Vista previa: " + msg.section('\n', 0, 0), 6000);
    });
    connect(importer_, &MediaImporter::probed, this, &MainWindow::onProbed);
    connect(importer_, &MediaImporter::failed, this, &MainWindow::onProbeFailed);
    connect(importer_, &MediaImporter::idle, this, [this] {
        if (importErrors_.isEmpty()) return;
        QMessageBox::warning(this, "No se pudieron importar algunos medios", importErrors_.join('\n'));
        importErrors_.clear();
    });

    connect(exporter_, &Exporter::progress, this, [this](int pct) { if (progress_) progress_->setValue(pct); });
    connect(exporter_, &Exporter::finished, this, [this](bool ok, const QString& message) {
        if (progress_) { progress_->close(); progress_->deleteLater(); progress_ = nullptr; }
        if (!ok) {
            QMessageBox::warning(this, "Exportación", message);
            return;
        }
        statusBar()->showMessage("Exportación terminada; verificando el archivo…");
        verifyPath_ = message;
        importer_->enqueue({message});   // se vuelve a leer con melt para comprobar duración y formato
    });

    statusBar()->showMessage(QString("Compilación %1").arg(NEXO_GIT_COMMIT));
    refresh();
}

QString MainWindow::describe(const nexo::MediaInfo& info) {
    QString s = QString::number(info.seconds, 'f', 1) + " s";
    if (info.width > 0) s += QString(" · %1×%2").arg(info.width).arg(info.height);
    s += info.kind == nexo::MediaKind::Audio ? " · audio" : info.kind == nexo::MediaKind::Image ? " · imagen" : "";
    return s;
}

void MainWindow::addLibraryItem(const QString& path, const QString& text) {
    for (int i = 0; i < library_->count(); ++i) {
        QListWidgetItem* it = library_->item(i);
        if (it->data(Qt::UserRole).toString() == path) { it->setText(text); return; }
    }
    auto* it = new QListWidgetItem(text);
    it->setData(Qt::UserRole, path);
    it->setToolTip(path);
    library_->addItem(it);
}

void MainWindow::importMedia() {
    if (!importer_->available()) {
        QMessageBox::warning(this, "Importar", "Esta compilación no incluye melt: no se pueden leer medios.");
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Importar medios", QString(),
        "Medios (*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.flac *.aac *.m4a *.ogg *.png *.jpg *.jpeg *.bmp);;Todos los archivos (*)");
    if (files.isEmpty()) return;
    for (const QString& f : files) addLibraryItem(f, QFileInfo(f).fileName() + "  (leyendo…)");
    importer_->enqueue(files);
    statusBar()->showMessage(QString("Leyendo %1 archivo(s)…").arg(files.size()));
}

void MainWindow::onProbed(const QString& path, const nexo::MediaInfo& info) {
    if (path == verifyPath_) {
        const nexo::Frame diff = info.frames > verifyFrames_ ? info.frames - verifyFrames_ : verifyFrames_ - info.frames;
        const QString sizeText = QString::number(QFileInfo(path).size() / 1024.0 / 1024.0, 'f', 1) + " MB";
        verifyPath_.clear();
        if (info.hasVideo && diff <= 2) {
            QMessageBox::information(this, "Exportación terminada",
                QString("Video exportado y verificado:\n%1\n\n%2 · %3").arg(path, describe(info), sizeText));
            statusBar()->showMessage("Exportado y verificado: " + path, 8000);
        } else {
            QMessageBox::warning(this, "Exportación",
                QString("El archivo se creó pero no coincide con lo esperado:\n%1\nDuración leída: %2 fotogramas (esperados %3).")
                    .arg(path).arg(info.frames).arg(verifyFrames_));
        }
        return;
    }
    info_.insert(path, info);
    project_.addMedia(path.toStdString());
    addLibraryItem(path, QFileInfo(path).fileName() + "  —  " + describe(info));
    statusBar()->showMessage("Medio listo: " + QFileInfo(path).fileName(), 4000);
}

void MainWindow::onProbeFailed(const QString& path, const QString& message) {
    if (path == verifyPath_) {
        verifyPath_.clear();
        QMessageBox::warning(this, "Exportación",
            QString("El archivo exportado no se pudo leer de vuelta:\n%1\n%2").arg(path, message));
        return;
    }
    addLibraryItem(path, QFileInfo(path).fileName() + "  —  NO SE PUDO LEER");
    importErrors_ << (QFileInfo(path).fileName() + ": " + message);
}

void MainWindow::addToTimeline(QListWidgetItem* item) {
    const QString path = item->data(Qt::UserRole).toString();
    const auto it = info_.constFind(path);
    if (it == info_.constEnd()) {
        statusBar()->showMessage("Este medio aún no se ha leído o no se pudo leer.", 5000);
        return;
    }
    const nexo::MediaInfo& mi = it.value();
    const int track = findTrack(history_.timeline(),
                                mi.kind == nexo::MediaKind::Audio ? nexo::TrackKind::Audio : nexo::TrackKind::Video);
    if (track < 0) {
        statusBar()->showMessage("No hay una pista adecuada para este medio.", 5000);
        return;
    }
    nexo::Clip c;
    c.mediaPath = path.toStdString();
    c.sourceIn = 0;
    c.duration = mi.frames;
    c.timelineStart = trackEnd(history_.timeline(), std::size_t(track));
    const auto r = history_.apply("Añadir clip", [&](nexo::Timeline& t) { return t.addClip(std::size_t(track), c); });
    statusBar()->showMessage(r.ok ? "Clip añadido: " + QFileInfo(path).fileName() : QString::fromStdString(r.error), 5000);
    refresh();
}

void MainWindow::exportVideo() {
    if (!exporter_->available()) {
        QMessageBox::warning(this, "Exportar", "Esta compilación no incluye melt: no se puede exportar.");
        return;
    }
    if (exporter_->running()) {
        QMessageBox::information(this, "Exportar", "Ya hay una exportación en curso.");
        return;
    }
    const nexo::Timeline tl = history_.timeline();
    if (tl.duration() <= 0) {
        QMessageBox::information(this, "Exportar", "La línea de tiempo está vacía: añade primero algún medio.");
        return;
    }
    project_.timeline = tl;
    const auto missing = project_.missingMedia();
    if (!missing.empty()) {
        QString list;
        for (const auto& m : missing) list += "\n• " + QString::fromStdString(m);
        QMessageBox::warning(this, "Exportar", "Faltan archivos de medios en el disco:" + list);
        return;
    }

    const QStringList presets{"720p (1280×720)", "1080p (1920×1080)", "480p (854×480)"};
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, "Exportar", "Resolución:", presets, 0, false, &ok);
    if (!ok) return;
    nexo::ExportSettings st;
    if (choice.startsWith("1080")) { st.width = 1920; st.height = 1080; }
    else if (choice.startsWith("480")) { st.width = 854; st.height = 480; }

    QString out = QFileDialog::getSaveFileName(this, "Exportar video", QDir::homePath() + "/nexo.mp4", "Video MP4 (*.mp4)");
    if (out.isEmpty()) return;
    if (!out.endsWith(".mp4", Qt::CaseInsensitive)) out += ".mp4";

    QString err;
    if (!exporter_->start(tl, st, out, err)) {
        QMessageBox::warning(this, "Exportar", err);
        return;
    }
    verifyFrames_ = tl.duration();
    progress_ = new QProgressDialog("Exportando…", "Cancelar", 0, 100, this);
    progress_->setWindowTitle("Exportando video");
    progress_->setWindowModality(Qt::WindowModal);
    progress_->setMinimumDuration(0);
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    progress_->setValue(0);
    connect(progress_, &QProgressDialog::canceled, exporter_, &Exporter::cancel);
}

void MainWindow::rebuildLibrary() {
    library_->clear();
    info_.clear();
    importErrors_.clear();
    const auto missing = project_.missingMedia();
    QStringList toRead;
    for (const std::string& m : project_.media) {
        const QString p = QString::fromStdString(m);
        const bool gone = std::find(missing.begin(), missing.end(), m) != missing.end();
        addLibraryItem(p, QFileInfo(p).fileName() + (gone ? "  —  FALTA EL ARCHIVO" : "  (leyendo…)"));
        if (!gone) toRead << p;
    }
    if (!toRead.isEmpty()) importer_->enqueue(toRead);
}

void MainWindow::save(bool askPath) {
    QString path = currentPath_;
    if (askPath || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Guardar proyecto", currentPath_,
                                            "Proyecto NEXO (*.nexo)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".nexo", Qt::CaseInsensitive)) path += ".nexo";
    }
    project_.timeline = history_.timeline();
    std::string err;
    if (!nexo::saveProject(project_, path.toStdString(), err)) {
        QMessageBox::warning(this, "No se pudo guardar", QString::fromStdString(err));
        return;
    }
    currentPath_ = path;
    statusBar()->showMessage("Proyecto guardado en " + path, 5000);
}

void MainWindow::open() {
    const QString path = QFileDialog::getOpenFileName(this, "Abrir proyecto", currentPath_,
                                                      "Proyecto NEXO (*.nexo)");
    if (path.isEmpty()) return;
    nexo::Project loaded;
    std::string err;
    if (!nexo::loadProject(path.toStdString(), loaded, err)) {
        QMessageBox::warning(this, "No se pudo abrir", QString::fromStdString(err));
        return;   // el proyecto actual no se toca
    }
    project_ = std::move(loaded);
    history_ = nexo::History(project_.timeline);
    importer_->setProjectFps(history_.timeline().frameRate());
    currentPath_ = path;
    refresh();
    rebuildLibrary();
    const auto missing = project_.missingMedia();
    statusBar()->showMessage(
        missing.empty() ? "Proyecto abierto"
                        : QString("Proyecto abierto. Faltan %1 archivo(s) de medios (p. ej. %2)")
                              .arg(missing.size())
                              .arg(QString::fromStdString(missing.front())));
}

void MainWindow::refresh() {
    timeline_->setTimeline(history_.timeline());
    requestPreview();
}

void MainWindow::requestPreview() {
    if (!previewer_->available()) return;
    if (history_.timeline().duration() <= 0) { frame_ = QImage(); viewer_->setPixmap(QPixmap()); viewer_->setText("Línea de tiempo vacía: importa medios y añádelos."); return; }
    previewer_->request(history_.timeline(), timeline_->playhead());
}

void MainWindow::showFrame() {
    if (frame_.isNull()) return;
    viewer_->setPixmap(QPixmap::fromImage(frame_).scaled(viewer_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    showFrame();
}
