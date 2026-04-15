#include "WireGuardServiceHost.h"   // MUST come before QApplication
#include <windows.h>

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include "MainWindow.h"

#include "util/logger.h"
#include "util/ConfigManager.h"
// void loadSettings(){
// ConfigManager::instance().init("config/xyboxconfig.json");

//   // Read values
//   int width = ConfigManager::instance().get("window.width", 800);
//   int height = ConfigManager::instance().get("window.height", 600);

//   // Write values
//   ConfigManager::instance().set("window.width", 1280);
//   ConfigManager::instance().set("window.height", 720);
//   ConfigManager::instance().set("log.file", std::string("app.log"));

//   // Force save (optional)
//   ConfigManager::instance().saveNow();

//   ConfigManager::instance().shutdown();
// }

void loadSettings()
{
    // ── Config file in the correct user config folder ────────────────────────
    // Example location (Windows): %APPDATA%\XY\XYBox\xyboxconfig.json
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configDir);
    QString configPath = configDir + "/xyboxconfig.json";

    ConfigManager::instance().init(configPath.toStdString());

    // ── Log file: separate folder per build type (debug vs release) ──────────
#ifdef QT_DEBUG
    QString buildType = "debug";
#else
    QString buildType = "release";
#endif

    QString logsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                    + "/logs/" + buildType;
    QDir().mkpath(logsDir);

    QString defaultLogPath = logsDir + "/xybox.log";

    // Only write defaults if the keys don't exist yet (preserves user changes)
      ConfigManager::instance().set("window.width", 1280);
      ConfigManager::instance().set("window.height", 720);
      ConfigManager::instance().set("log.file", defaultLogPath.toStdString());
      ConfigManager::instance().set("log.max_size_mb", 10);     // 10 MB per file
      ConfigManager::instance().set("log.max_files", 5);
      ConfigManager::instance().saveNow();
    // NO shutdown() – singleton must stay alive
}

int main(int argc, char* argv[])
{
    // fixme. comment servicemode so that the ui can be shown
    // ── Service mode (must stay first) ───────────────────────────────────────
    //if (handleServiceMode(argc, argv))
        //return 0;

    // ── GUI mode ─────────────────────────────────────────────────────────────
    // Set app identity EARLY so QStandardPaths returns correct user folders
    QCoreApplication::setOrganizationName("XY");
    QCoreApplication::setApplicationName("XYBox");
    QCoreApplication::setApplicationVersion("1.0.0");

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // Config first → logging can read the correct (full-path) log.file value
    loadSettings();
    setup_logging();

    QApplication app(argc, argv);

    // AppData folder (still useful for other things your app may create)
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    app.setQuitOnLastWindowClosed(false);

    MainWindow w;
    w.show();

    return app.exec();
}

// int main(int argc, char* argv[])
// {
//     // ── Service mode: SCM launched us as WireGuardTunnel$<name> ──────────────
//     // Must be checked before any Qt infrastructure is initialised.
//     if (handleServiceMode(argc, argv))
//         return 0;

//     // ── Normal GUI mode ───────────────────────────────────────────────────────
//     // High-DPI support (Qt6: enabled by default, but be explicit)
//     QApplication::setHighDpiScaleFactorRoundingPolicy(
//         Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

//    setup_logging();
//    loadSettings();
//     QApplication app(argc, argv);
//     app.setApplicationName("CustomDesktopQt");
//     app.setOrganizationName("YourOrg");
//     app.setApplicationVersion("1.0.0");

//     // Create AppData dirs
//     QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

//     // Prevent quitting when the main window is hidden to tray
//     app.setQuitOnLastWindowClosed(false);

//     MainWindow w;
//     w.show();

//     return app.exec();
// }

//#include <QApplication>
//#include "MainWindow.h"
//
//#include "util/logger.h"
//#include "util/ConfigManager.h"
//int main(int argc, char *argv[])
//{
//    QApplication app(argc, argv);
//
//    setup_logging();
//    loadSettings();
//
//    spdlog::info("main started....");
//    MainWindow w;
//    w.show();
//
//    return app.exec();
//}
