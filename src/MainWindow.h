#pragma once
#include <QMainWindow>

class DesktopWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private:
    DesktopWidget* desktop;
};