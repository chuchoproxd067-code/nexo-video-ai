#pragma once
#include <QMainWindow>

#include <QString>

#include "core/History.h"
#include "core/Project.h"

class TimelineWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private:
    void refresh();
    void save(bool askPath);
    void open();

    nexo::Project project_;
    QString currentPath_;
    nexo::History history_;
    TimelineWidget* timeline_ = nullptr;
};
