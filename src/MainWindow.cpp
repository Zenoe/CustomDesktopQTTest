#include "MainWindow.h"
#include "DesktopWidget.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(900, 600);
    setWindowTitle("Custom Desktop Box");

    // Menu
    QMenu* fileMenu = menuBar()->addMenu("File");
    QAction* exitAction = fileMenu->addAction("Exit");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    desktop = new DesktopWidget(this);
    setCentralWidget(desktop);
}