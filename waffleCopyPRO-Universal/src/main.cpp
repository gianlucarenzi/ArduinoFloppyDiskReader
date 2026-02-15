#include "mainwindow.h"
#include "inc/debugmsg.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QString>

int main(int argc, char *argv[])
{
    bool useVNC = false;
    bool useDebug = false;
    for (int i=0; i < argc; i++)
    {
        QString arg = argv[i];
        if (arg.contains("vnc"))
        {
            useVNC = true;
        }
        if (arg == "-debug")
        {
            useDebug = true;
        }
    }

    DebugMsg::init(useDebug);

    // Start an external Xorg based viewer
    if (useVNC)
    {
        int i = system("./runvnc.sh");
        if (i < 0)
        {
            fprintf(stderr, "*** System Call to runvnc.sh returns %d ERROR ***\n", i);
        }
    }

    // Enable high DPI scaling - must be set before QApplication is created
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QCoreApplication::setOrganizationName("RetroGiovedi");
    QCoreApplication::setApplicationName("waffleCopyPRO-Universal");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    
    QApplication a(argc, argv);
#ifdef __APPLE__
    // On macOS, when running from a bundle, the working directory is the user's home dir.
    // We need to change it to the Resources directory inside the bundle, where the assets are.
    QDir::setCurrent(QCoreApplication::applicationDirPath() + "/../Resources");
#endif
    MainWindow w;
    int id = -1; // Initialize id outside the ifdef
#ifdef __APPLE__
    // On macOS, when running from a bundle, the working directory is the user's home dir.
    // We need to change it to the Resources directory inside the bundle, where the assets are.
    QDir::setCurrent(QCoreApplication::applicationDirPath() + "/../Resources");
    QString fontPath = QCoreApplication::applicationDirPath() + "/../Resources/fonts/TopazPlus_a500_v1.0.ttf";
    id = QFontDatabase::addApplicationFont(fontPath);
#else
    // For other platforms, use the original relative path
    id = QFontDatabase::addApplicationFont("fonts/TopazPlus_a500_v1.0.ttf");
#endif
    if (id < 0)
    {
        DebugMsg::print(__func__, "Missing font!");
    }
    else
    {
        QString family = QFontDatabase::applicationFontFamilies(id).at(0);
        QFont amigaTopaz(family);
        amigaTopaz.setPointSize(9); // Smaller size for Amiga Topaz font
        a.setFont(amigaTopaz); // Set font globally for the entire application
        w.setFont(amigaTopaz);
    }
    w.show();
    return a.exec();
}
