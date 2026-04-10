#include "MainWindow.h"
#include "DesktopWidget.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>

#include <QStandardPaths>

static const QString BOX_FILE =
QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
+ "/desktop.box";

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
    desktop->loadFromBoxFile(BOX_FILE);
}
MainWindow::~MainWindow()
{
    desktop->saveToBoxFile(BOX_FILE);
}