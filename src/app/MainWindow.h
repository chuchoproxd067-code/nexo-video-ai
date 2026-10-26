#pragma once
#include <QMainWindow>

#include "core/History.h"

class TimelineWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private:
    void refresh();

    nexo::History history_;
    TimelineWidget* timeline_ = nullptr;
};
