#include "MainWindow.h"

#include <algorithm>
#include <functional>

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QPair>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QDockWidget>
#include <QStandardPaths>
#include <QUrl>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QFile>
#include <QTextStream>
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
#include "ClipDialogs.h"
#include "core/Subtitles.h"
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
    auto* openA = new QAction("Abrir…", this);
    auto* saveA = new QAction("Guardar", this);
    auto* saveAsA = new QAction("Guardar como…", this);
    auto* importA = new QAction("Importar medios…", this);
    auto* addA = new QAction("Añadir a la línea de tiempo", this);
    auto* playA = new QAction("Reproducir", this);
    auto* exportA = new QAction("Exportar…", this);
    auto* split = new QAction("Dividir en el cursor", this);
    auto* propsA = new QAction("Propiedades del clip…", this);
    auto* fxA = new QAction("Volumen y fundidos…", this);
    auto* trA = new QAction("Fundido cruzado…", this);
    auto* textA = new QAction("Añadir texto…", this);
    auto* editTextA = new QAction("Editar texto…", this);
    auto* detachA = new QAction("Separar audio", this);
    auto* del = new QAction("Eliminar clip", this);
    auto* delRipple = new QAction("Eliminar y cerrar el hueco", this);
    auto* undo = new QAction("Deshacer", this);
    auto* redo = new QAction("Rehacer", this);
    auto* markA = new QAction("Marcador en el cursor", this);
    auto* unmarkA = new QAction("Quitar marcador más cercano", this);
    auto* addVA = new QAction("Añadir pista de video", this);
    auto* addAA = new QAction("Añadir pista de audio", this);
    auto* remTA = new QAction("Quitar pista (vacía)", this);
    auto* renTA = new QAction("Renombrar pista…", this);
    auto* srtIn = new QAction("Importar subtítulos (.srt)…", this);
    auto* srtOut = new QAction("Exportar subtítulos (.srt)…", this);
    auto* zin = new QAction("Acercar", this);
    auto* zout = new QAction("Alejar", this);
    auto* zfit = new QAction("Ajustar a la ventana", this);
    auto* snapA = new QAction("Imán (pegar a bordes y marcadores)", this);
    snapA->setCheckable(true);
    snapA->setChecked(true);
    del->setShortcut(QKeySequence::Delete);
    delRipple->setShortcut(Qt::SHIFT | Qt::Key_Delete);
    split->setShortcut(Qt::Key_S);
    propsA->setShortcut(Qt::CTRL | Qt::Key_E);
    textA->setShortcut(Qt::Key_T);
    markA->setShortcut(Qt::Key_M);
    undo->setShortcut(QKeySequence::Undo);
    redo->setShortcut(QKeySequence::Redo);
    zin->setShortcut(Qt::CTRL | Qt::Key_Plus);
    zout->setShortcut(Qt::CTRL | Qt::Key_Minus);
    openA->setShortcut(QKeySequence::Open);
    saveA->setShortcut(QKeySequence::Save);
    for (QAction* a : {openA, saveA, importA, addA, playA, exportA}) tb->addAction(a);
    tb->addSeparator();
    for (QAction* a : {split, propsA, textA, del}) tb->addAction(a);
    tb->addSeparator();
    for (QAction* a : {undo, redo, zin, zout, zfit}) tb->addAction(a);

    auto* mFile = menuBar()->addMenu("Archivo");
    for (QAction* a : {openA, saveA, saveAsA}) mFile->addAction(a);
    mFile->addSeparator();
    for (QAction* a : {importA, exportA, playA}) mFile->addAction(a);
    auto* mEdit = menuBar()->addMenu("Edición");
    for (QAction* a : {undo, redo}) mEdit->addAction(a);
    mEdit->addSeparator();
    for (QAction* a : {split, del, delRipple}) mEdit->addAction(a);
    mEdit->addSeparator();
    for (QAction* a : {markA, unmarkA}) mEdit->addAction(a);
    auto* mClip = menuBar()->addMenu("Clip");
    for (QAction* a : {addA, propsA, fxA, trA, textA, editTextA, detachA}) mClip->addAction(a);
    auto* mTrack = menuBar()->addMenu("Pista");
    for (QAction* a : {addVA, addAA, remTA, renTA}) mTrack->addAction(a);
    auto* mSub = menuBar()->addMenu("Subtítulos");
    for (QAction* a : {srtIn, srtOut}) mSub->addAction(a);
    auto* mView = menuBar()->addMenu("Ver");
    for (QAction* a : {zin, zout, zfit}) mView->addAction(a);
    mView->addAction(snapA);

    auto* libraryDock = new QDockWidget("Biblioteca", this);
    library_ = new QListWidget;
    libraryDock->setWidget(library_);
    addDockWidget(Qt::LeftDockWidgetArea, libraryDock);

    auto* resDock = new QDockWidget("Recursos (sonidos y música libres)", this);
    resources_ = new QListWidget;
    resDock->setWidget(resources_);
    addDockWidget(Qt::LeftDockWidgetArea, resDock);
    tabifyDockWidget(libraryDock, resDock);
    libraryDock->raise();
    fillResources();

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
    connect(resources_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) { addResource(it); });
    connect(library_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) { addToTimeline(it); });
    connect(exportA, &QAction::triggered, this, [this] { exportVideo(); });
    connect(playA, &QAction::triggered, this, [this] { playExternal(); });
    connect(split, &QAction::triggered, this, [this] {
        const nexo::ClipId id = timeline_->selectedClip();
        if (id == 0) { statusBar()->showMessage("Selecciona un clip (clic) y coloca el cursor donde dividir.", 5000); return; }
        const nexo::Frame at = timeline_->playhead();
        applyEdit("Dividir", [&](nexo::Timeline& t) { return t.splitClip(id, at); });
    });
    connect(trA, &QAction::triggered, this, [this] { editTransition(); });
    connect(fxA, &QAction::triggered, this, [this] { editClipFx(); });
    connect(propsA, &QAction::triggered, this, [this] { editProps(); });
    connect(textA, &QAction::triggered, this, [this] { addText(); });
    connect(editTextA, &QAction::triggered, this, [this] { editText(); });
    connect(detachA, &QAction::triggered, this, [this] { detachAudio(); });
    connect(delRipple, &QAction::triggered, this, [this] { removeRipple(); });
    connect(markA, &QAction::triggered, this, [this] { addMarkerHere(); });
    connect(unmarkA, &QAction::triggered, this, [this] { removeNearestMarker(); });
    connect(addVA, &QAction::triggered, this, [this] { addTrack(nexo::TrackKind::Video); });
    connect(addAA, &QAction::triggered, this, [this] { addTrack(nexo::TrackKind::Audio); });
    connect(remTA, &QAction::triggered, this, [this] { removeTrack(); });
    connect(renTA, &QAction::triggered, this, [this] { renameTrack(); });
    connect(srtIn, &QAction::triggered, this, [this] { importSrt(); });
    connect(srtOut, &QAction::triggered, this, [this] { exportSrt(); });
    connect(zin, &QAction::triggered, this, [this] { timeline_->zoomIn(); });
    connect(zout, &QAction::triggered, this, [this] { timeline_->zoomOut(); });
    connect(zfit, &QAction::triggered, this, [this] { timeline_->zoomFit(); });
    connect(snapA, &QAction::toggled, this, [this](bool on) { timeline_->setSnap(on); });
    connect(del, &QAction::triggered, this, [this] {
        const nexo::ClipId id = timeline_->selectedClip();
        if (id == 0) { statusBar()->showMessage("Selecciona un clip para eliminarlo.", 4000); return; }
        applyEdit("Eliminar clip", [&](nexo::Timeline& t) { return t.removeClip(id); });
    });
    connect(undo, &QAction::triggered, this, [this] { history_.undo(); refresh(); });
    connect(redo, &QAction::triggered, this, [this] { history_.redo(); refresh(); });
    connect(timeline_, &TimelineWidget::clipActivated, this, [this](nexo::ClipId) { editProps(); });
    connect(timeline_, &TimelineWidget::trackFlagsRequested, this, [this](int tr, bool m, bool h, bool l) {
        applyEdit("Pista", [=](nexo::Timeline& t) { return t.setTrackFlags(std::size_t(tr), m, h, l); });
    });
    connect(timeline_, &TimelineWidget::contextRequested, this, [=](nexo::ClipId id, int, const QPoint& pos) {
        QMenu menu(this);
        if (id != 0) {
            const nexo::Clip* c = clipById(id);
            menu.addAction(propsA);
            if (c && c->kind == nexo::ClipKind::Text) menu.addAction(editTextA);
            menu.addAction(fxA); menu.addAction(trA); menu.addAction(split); menu.addAction(detachA);
            menu.addSeparator();
            menu.addAction(del); menu.addAction(delRipple);
        } else {
            menu.addAction(textA); menu.addAction(markA); menu.addAction(unmarkA);
            menu.addSeparator();
            menu.addAction(addVA); menu.addAction(addAA); menu.addAction(renTA); menu.addAction(remTA);
        }
        menu.exec(pos);
    });

    connect(timeline_, &TimelineWidget::moveRequested, this, [this](nexo::ClipId id, int tr, nexo::Frame s) {
        applyEdit("Mover clip", [=](nexo::Timeline& t) { return t.moveClip(id, std::size_t(tr), s); });
    });
    connect(timeline_, &TimelineWidget::trimStartRequested, this, [this](nexo::ClipId id, nexo::Frame d) {
        applyEdit("Recortar inicio", [=](nexo::Timeline& t) { return t.trimStart(id, d); });
    });
    connect(timeline_, &TimelineWidget::trimEndRequested, this, [this](nexo::ClipId id, nexo::Frame d) {
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
            playPending_ = false;
            QMessageBox::warning(this, "Exportación", message);
            return;
        }
        if (playPending_) {
            playPending_ = false;
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(message)))
                QMessageBox::warning(this, "Reproducir", "No se pudo abrir el reproductor del sistema. El archivo está en:\n" + message);
            else statusBar()->showMessage("Reproduciendo en el reproductor del sistema", 5000);
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
        if ((verifyAudioOnly_ ? info.hasAudio : info.hasVideo) && diff <= 2) {
            QMessageBox::information(this, "Exportación terminada",
                QString("Archivo exportado y verificado:\n%1\n\n%2 · %3").arg(path, describe(info), sizeText));
            statusBar()->showMessage("Exportado y verificado: " + path, 8000);
        } else {
            QMessageBox::warning(this, "Exportación",
                QString("El archivo se creó pero no coincide con lo esperado:\n%1\nDuración leída: %2 fotogramas (esperados %3).")
                    .arg(path).arg(info.frames).arg(verifyFrames_));
        }
        return;
    }
    info_.insert(path, info);
    syncMediaFrames();
    project_.addMedia(path.toStdString());
    addLibraryItem(path, QFileInfo(path).fileName() + "  —  " + describe(info));
    statusBar()->showMessage("Medio listo: " + QFileInfo(path).fileName(), 4000);
    if (addWhenReady_.removeAll(path) > 0) {
        for (int i = 0; i < library_->count(); ++i)
            if (library_->item(i)->data(Qt::UserRole).toString() == path) { addToTimeline(library_->item(i)); break; }
    }
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

    nexo::ExportSettings st;
    QString out;
    if (!dialogs::chooseExport(this, st, out, QDir::homePath())) return;
    verifyFrames_ = tl.duration();
    verifyAudioOnly_ = nexo::formatIsAudioOnly(st.format);
    beginRender("Exportando", st, out);
}

bool MainWindow::beginRender(const QString& title, const nexo::ExportSettings& st, const QString& out) {
    QString err;
    if (!exporter_->start(history_.timeline(), st, out, err)) {
        playPending_ = false;
        QMessageBox::warning(this, title, err);
        return false;
    }
    progress_ = new QProgressDialog(title + "…", "Cancelar", 0, 100, this);
    progress_->setWindowTitle(title);
    progress_->setWindowModality(Qt::WindowModal);
    progress_->setMinimumDuration(0);
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    progress_->setValue(0);
    connect(progress_, &QProgressDialog::canceled, exporter_, &Exporter::cancel);
    return true;
}

void MainWindow::playExternal() {
    if (!exporter_->available()) {
        QMessageBox::warning(this, "Reproducir", "Esta compilación no incluye melt.");
        return;
    }
    if (exporter_->running()) {
        QMessageBox::information(this, "Reproducir", "Ya hay un render en curso.");
        return;
    }
    if (history_.timeline().duration() <= 0) {
        QMessageBox::information(this, "Reproducir", "La línea de tiempo está vacía.");
        return;
    }
    project_.timeline = history_.timeline();
    if (!project_.missingMedia().empty()) {
        QMessageBox::warning(this, "Reproducir", "Faltan archivos de medios en el disco.");
        return;
    }
    nexo::ExportSettings st;
    st.width = 854;
    st.height = 480;
    const QString out = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/nexo-vista-previa.mp4";
    playPending_ = true;
    beginRender("Preparando vista previa", st, out);
}

void MainWindow::editClipFx() {
    const nexo::ClipId id = timeline_->selectedClip();
    if (id == 0) { statusBar()->showMessage("Selecciona un clip (clic) para cambiar su volumen y fundidos.", 5000); return; }
    const nexo::Timeline& tl = history_.timeline();
    const nexo::Clip* clip = nullptr;
    for (std::size_t t = 0; t < tl.trackCount() && !clip; ++t)
        for (const nexo::Clip& c : tl.track(t).clips) if (c.id == id) clip = &c;
    if (!clip) return;
    const double fps = double(tl.frameRate().num) / double(tl.frameRate().den);
    const double maxSec = double(clip->duration) / fps;

    QDialog dlg(this);
    dlg.setWindowTitle("Volumen y fundidos");
    auto* form = new QFormLayout(&dlg);
    auto* vol = new QDoubleSpinBox; vol->setRange(0, 400); vol->setSuffix(" %"); vol->setDecimals(0); vol->setValue(clip->volume * 100.0);
    auto* fin = new QDoubleSpinBox; fin->setRange(0, maxSec); fin->setSuffix(" s"); fin->setDecimals(2); fin->setSingleStep(0.25); fin->setValue(double(clip->fadeIn) / fps);
    auto* fout = new QDoubleSpinBox; fout->setRange(0, maxSec); fout->setSuffix(" s"); fout->setDecimals(2); fout->setSingleStep(0.25); fout->setValue(double(clip->fadeOut) / fps);
    form->addRow("Volumen", vol);
    form->addRow("Fundido de entrada", fin);
    form->addRow("Fundido de salida", fout);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    const double v = vol->value() / 100.0;
    const nexo::Frame fi = nexo::secondsToFrames(fin->value(), tl.frameRate());
    const nexo::Frame fo = nexo::secondsToFrames(fout->value(), tl.frameRate());
    const auto r = history_.apply("Volumen y fundidos", [&](nexo::Timeline& t) { return t.setClipFx(id, v, fi, fo); });
    statusBar()->showMessage(r.ok ? "Volumen y fundidos aplicados" : QString::fromStdString(r.error), 6000);
    refresh();
}

void MainWindow::editTransition() {
    const nexo::ClipId id = timeline_->selectedClip();
    if (id == 0) { statusBar()->showMessage("Selecciona el segundo clip de la pareja (el que viene después) para poner un fundido cruzado.", 6000); return; }
    const nexo::Timeline& tl = history_.timeline();
    const nexo::Clip* clip = nullptr;
    for (std::size_t t = 0; t < tl.trackCount() && !clip; ++t)
        for (const nexo::Clip& c : tl.track(t).clips) if (c.id == id) clip = &c;
    if (!clip) return;
    const double fps = double(tl.frameRate().num) / double(tl.frameRate().den);
    bool ok = false;
    const double sec = QInputDialog::getDouble(this, "Fundido cruzado",
        "Duración en segundos (0 = quitar). Los dos clips se solapan esa duración y el vídeo total se acorta lo mismo:",
        double(clip->transitionIn) / fps, 0.0, 30.0, 2, &ok, Qt::WindowFlags(), 0.25);
    if (!ok) return;
    const nexo::Frame n = nexo::secondsToFrames(sec, tl.frameRate());
    const auto r = history_.apply("Fundido cruzado", [&](nexo::Timeline& t) { return t.setTransition(id, n); });
    statusBar()->showMessage(r.ok ? "Fundido cruzado aplicado" : QString::fromStdString(r.error), 7000);
    refresh();
}

void MainWindow::fillResources() {
    // Junto al ejecutable: <paquete>/bin/nexo-video-ai.exe y <paquete>/assets/{sfx,music}.
    const QDir root(QCoreApplication::applicationDirPath() + "/../assets");
    const QList<QPair<QString, QString>> groups{{"sfx", "Efecto"}, {"music", "Música"}};
    for (const auto& g : groups) {
        const QDir dir(root.filePath(g.first));
        for (const QFileInfo& fi : dir.entryInfoList({"*.ogg", "*.wav", "*.mp3"}, QDir::Files, QDir::Name)) {
            auto* it = new QListWidgetItem(g.second + " · " + fi.completeBaseName().replace('-', ' '));
            it->setData(Qt::UserRole, fi.absoluteFilePath());
            it->setToolTip(fi.absoluteFilePath() + "\nCC0 / dominio público (ver assets/LICENSE-ASSETS.md)");
            resources_->addItem(it);
        }
    }
    if (resources_->count() == 0) {
        auto* it = new QListWidgetItem("(esta compilación no incluye la carpeta de recursos)");
        it->setFlags(Qt::NoItemFlags);
        resources_->addItem(it);
    }
}

void MainWindow::addResource(QListWidgetItem* item) {
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (!importer_->available()) { statusBar()->showMessage("Esta compilación no incluye melt.", 5000); return; }
    if (info_.contains(path)) {
        for (int i = 0; i < library_->count(); ++i)
            if (library_->item(i)->data(Qt::UserRole).toString() == path) { addToTimeline(library_->item(i)); return; }
    }
    addWhenReady_ << path;
    addLibraryItem(path, QFileInfo(path).fileName() + "  (leyendo…)");
    importer_->enqueue({path});
}

void MainWindow::rebuildLibrary() {
    library_->clear();
    info_.clear();
    syncMediaFrames();
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

// ---------------------------------------------------------------------------------------------
// Edición avanzada (todas las operaciones pasan por History: deshacer/rehacer sirve para todo)

void MainWindow::applyEdit(const char* label, const std::function<nexo::Result(nexo::Timeline&)>& op) {
    const auto r = history_.apply(label, op);
    statusBar()->showMessage(r.ok ? QString::fromUtf8(label) : QString::fromStdString(r.error), 6000);
    if (!r.ok) QApplication::beep();
    refresh();
}

void MainWindow::syncMediaFrames() {
    std::map<std::string, nexo::Frame> m;
    for (auto it = info_.constBegin(); it != info_.constEnd(); ++it) m[it.key().toStdString()] = it.value().frames;
    exporter_->setMediaFrames(m);
    previewer_->setMediaFrames(m);
}

const nexo::Clip* MainWindow::clipById(nexo::ClipId id) const {
    const nexo::Timeline& tl = history_.timeline();
    for (std::size_t t = 0; t < tl.trackCount(); ++t)
        for (const nexo::Clip& c : tl.track(t).clips) if (c.id == id) return &c;
    return nullptr;
}

int MainWindow::trackOfClip(nexo::ClipId id) const {
    const nexo::Timeline& tl = history_.timeline();
    for (std::size_t t = 0; t < tl.trackCount(); ++t)
        for (const nexo::Clip& c : tl.track(t).clips) if (c.id == id) return int(t);
    return -1;
}

void MainWindow::editProps() {
    const nexo::ClipId id = timeline_->selectedClip();
    const nexo::Clip* clip = clipById(id);
    if (!clip) { statusBar()->showMessage("Selecciona un clip (clic) para editar sus propiedades.", 5000); return; }
    const int tr = trackOfClip(id);
    nexo::ClipProps np;
    const nexo::Clip copy = *clip;
    if (!dialogs::editProps(this, copy, history_.timeline().track(std::size_t(tr)).kind, history_.timeline().frameRate(), np)) return;
    applyEdit("Propiedades del clip", [&](nexo::Timeline& t) { return t.setClipProps(id, np); });
}

void MainWindow::addText() {
    nexo::TextProps t;
    t.text = "Título";
    if (!dialogs::editText(this, "Añadir texto", t)) return;
    int track = timeline_->currentTrack();
    const nexo::Timeline& tl = history_.timeline();
    if (track < 0 || tl.track(std::size_t(track)).kind != nexo::TrackKind::Video) track = findTrack(tl, nexo::TrackKind::Video);
    if (track < 0) { statusBar()->showMessage("No hay pista de video para el texto.", 5000); return; }
    const nexo::Frame start = timeline_->playhead();
    const nexo::Frame dur = nexo::secondsToFrames(3.0, tl.frameRate());
    applyEdit("Añadir texto", [&](nexo::Timeline& t2) { return t2.addTextClip(std::size_t(track), t, start, dur); });
}

void MainWindow::editText() {
    const nexo::Clip* clip = clipById(timeline_->selectedClip());
    if (!clip || clip->kind != nexo::ClipKind::Text) { statusBar()->showMessage("Selecciona un clip de texto.", 5000); return; }
    nexo::TextProps t = clip->text;
    const nexo::ClipId id = clip->id;
    if (!dialogs::editText(this, "Editar texto", t)) return;
    applyEdit("Editar texto", [&](nexo::Timeline& t2) { return t2.setText(id, t); });
}

void MainWindow::addTrack(nexo::TrackKind kind) {
    const nexo::Timeline& tl = history_.timeline();
    int n = 1;
    for (std::size_t i = 0; i < tl.trackCount(); ++i) if (tl.track(i).kind == kind) ++n;
    const std::string name = std::string(kind == nexo::TrackKind::Video ? "Video " : "Audio ") + std::to_string(n);
    applyEdit("Añadir pista", [&](nexo::Timeline& t) { t.addTrack(name, kind); return nexo::Result::success(); });
}

void MainWindow::removeTrack() {
    const int tr = timeline_->currentTrack();
    if (tr < 0) { statusBar()->showMessage("Pulsa el nombre de la pista que quieres quitar.", 5000); return; }
    applyEdit("Quitar pista", [&](nexo::Timeline& t) { return t.removeTrack(std::size_t(tr)); });
}

void MainWindow::renameTrack() {
    const int tr = timeline_->currentTrack();
    if (tr < 0) { statusBar()->showMessage("Pulsa el nombre de la pista que quieres renombrar.", 5000); return; }
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Renombrar pista", "Nombre:", QLineEdit::Normal,
                                               QString::fromStdString(history_.timeline().track(std::size_t(tr)).name), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    applyEdit("Renombrar pista", [&](nexo::Timeline& t) { return t.renameTrack(std::size_t(tr), name.trimmed().toStdString()); });
}

void MainWindow::addMarkerHere() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Marcador", "Nombre (opcional):", QLineEdit::Normal, QString(), &ok);
    if (!ok) return;
    const nexo::Frame f = timeline_->playhead();
    applyEdit("Añadir marcador", [&](nexo::Timeline& t) { return t.addMarker(f, name.toStdString()); });
}

void MainWindow::removeNearestMarker() {
    const auto& ms = history_.timeline().markers();
    if (ms.empty()) { statusBar()->showMessage("No hay marcadores.", 4000); return; }
    const nexo::Frame f = timeline_->playhead();
    std::size_t best = 0;
    for (std::size_t i = 1; i < ms.size(); ++i)
        if (std::llabs(ms[i].frame - f) < std::llabs(ms[best].frame - f)) best = i;
    applyEdit("Quitar marcador", [&](nexo::Timeline& t) { return t.removeMarker(best); });
}

void MainWindow::detachAudio() {
    const nexo::ClipId id = timeline_->selectedClip();
    if (id == 0) { statusBar()->showMessage("Selecciona un clip de video.", 5000); return; }
    const int at = findTrack(history_.timeline(), nexo::TrackKind::Audio);
    if (at < 0) { statusBar()->showMessage("Añade primero una pista de audio.", 5000); return; }
    applyEdit("Separar audio", [&](nexo::Timeline& t) { return t.detachAudio(id, std::size_t(at)); });
}

void MainWindow::removeRipple() {
    const nexo::ClipId id = timeline_->selectedClip();
    if (id == 0) { statusBar()->showMessage("Selecciona un clip.", 4000); return; }
    applyEdit("Eliminar y cerrar el hueco", [&](nexo::Timeline& t) { return t.removeClipRipple(id); });
}

void MainWindow::importSrt() {
    const QString path = QFileDialog::getOpenFileName(this, "Importar subtítulos", QString(), "Subtítulos SRT (*.srt)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Subtítulos", "No se pudo abrir el archivo."); return; }
    const std::string data = f.readAll().toStdString();
    std::vector<nexo::SubtitleCue> cues;
    std::string err;
    if (!nexo::parseSrt(data, history_.timeline().frameRate(), cues, err)) {
        QMessageBox::warning(this, "Subtítulos", QString::fromStdString(err));
        return;
    }
    if (cues.empty()) { QMessageBox::information(this, "Subtítulos", "El archivo no tiene subtítulos."); return; }
    nexo::TextProps style;
    style.size = 6; style.posX = 50; style.posY = 90; style.bold = true;
    applyEdit("Importar subtítulos", [&](nexo::Timeline& t) {
        const std::size_t track = t.addTrack("Subtítulos", nexo::TrackKind::Video);
        for (const nexo::SubtitleCue& c : cues) {
            nexo::TextProps tp = style;
            tp.text = c.text;
            const nexo::Result r = t.addTextClip(track, tp, c.start, c.end - c.start);
            if (!r.ok) return r;
        }
        return nexo::Result::success();
    });
}

void MainWindow::exportSrt() {
    const nexo::Timeline& tl = history_.timeline();
    int tr = timeline_->currentTrack();
    auto hasText = [&](int i) {
        if (i < 0) return false;
        for (const nexo::Clip& c : tl.track(std::size_t(i)).clips) if (c.kind == nexo::ClipKind::Text) return true;
        return false;
    };
    if (!hasText(tr)) {
        tr = -1;
        for (std::size_t i = 0; i < tl.trackCount(); ++i) if (hasText(int(i))) { tr = int(i); break; }
    }
    if (tr < 0) { QMessageBox::information(this, "Subtítulos", "Ninguna pista tiene clips de texto."); return; }
    const QString path = QFileDialog::getSaveFileName(this, "Exportar subtítulos", QDir::homePath() + "/subtitulos.srt", "Subtítulos SRT (*.srt)");
    if (path.isEmpty()) return;
    const std::string srt = nexo::formatSrt(nexo::cuesFromTrack(tl, std::size_t(tr)), tl.frameRate());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(srt.data(), qint64(srt.size())) != qint64(srt.size())) {
        QMessageBox::warning(this, "Subtítulos", "No se pudo escribir el archivo.");
        return;
    }
    statusBar()->showMessage("Subtítulos exportados: " + path, 6000);
}
