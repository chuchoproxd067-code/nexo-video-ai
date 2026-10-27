#pragma once
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "core/MltXml.h"

#include "core/History.h"
#include "core/MediaProbe.h"
#include "core/Project.h"

class Exporter;
class MediaImporter;
class Previewer;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QProgressDialog;
class TimelineWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    void refresh();
    void requestPreview();
    void showFrame();
    void save(bool askPath);
    void open();
    void importMedia();
    void addToTimeline(QListWidgetItem* item);
    void exportVideo();
    void playExternal();
    void editClipFx();
    void editTransition();
    bool beginRender(const QString& title, const nexo::ExportSettings& st, const QString& out);
    void rebuildLibrary();
    void fillResources();
    void addResource(QListWidgetItem* item);
    void onProbed(const QString& path, const nexo::MediaInfo& info);
    void onProbeFailed(const QString& path, const QString& message);
    void addLibraryItem(const QString& path, const QString& text);
    static QString describe(const nexo::MediaInfo& info);

    nexo::Project project_;
    QString currentPath_;
    nexo::History history_;
    TimelineWidget* timeline_ = nullptr;
    QListWidget* library_ = nullptr;
    QListWidget* resources_ = nullptr;
    QStringList addWhenReady_;      // recursos pedidos con doble clic: se añaden al terminar de leerlos
    MediaImporter* importer_ = nullptr;
    Exporter* exporter_ = nullptr;
    Previewer* previewer_ = nullptr;
    QLabel* viewer_ = nullptr;
    QImage frame_;
    QProgressDialog* progress_ = nullptr;
    QHash<QString, nexo::MediaInfo> info_;
    QStringList importErrors_;
    bool playPending_ = false;      // el render en curso es una vista previa para el reproductor del sistema
    QString verifyPath_;            // archivo exportado pendiente de verificar
    nexo::Frame verifyFrames_ = 0;
};
