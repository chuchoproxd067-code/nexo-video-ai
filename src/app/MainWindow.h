#pragma once
#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "core/History.h"
#include "core/MediaProbe.h"
#include "core/Project.h"

class Exporter;
class MediaImporter;
class QListWidget;
class QListWidgetItem;
class QProgressDialog;
class TimelineWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private:
    void refresh();
    void save(bool askPath);
    void open();
    void importMedia();
    void addToTimeline(QListWidgetItem* item);
    void exportVideo();
    void rebuildLibrary();
    void onProbed(const QString& path, const nexo::MediaInfo& info);
    void onProbeFailed(const QString& path, const QString& message);
    void addLibraryItem(const QString& path, const QString& text);
    static QString describe(const nexo::MediaInfo& info);

    nexo::Project project_;
    QString currentPath_;
    nexo::History history_;
    TimelineWidget* timeline_ = nullptr;
    QListWidget* library_ = nullptr;
    MediaImporter* importer_ = nullptr;
    Exporter* exporter_ = nullptr;
    QProgressDialog* progress_ = nullptr;
    QHash<QString, nexo::MediaInfo> info_;
    QStringList importErrors_;
    QString verifyPath_;            // archivo exportado pendiente de verificar
    nexo::Frame verifyFrames_ = 0;
};
