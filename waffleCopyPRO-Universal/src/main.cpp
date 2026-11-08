#include "mainwindow.h"
#include "inc/debugmsg.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QScreen>
#include <QGuiApplication>

int main(int argc, char *argv[])
{
    bool useVNC = false;
    bool useDebug = false;
    bool useVerboseDebug = false;
    qreal forceScaleFactor = -1.0; // -1 means auto-detect
    
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
        if (arg == "-vdebug")
        {
            useDebug = true;
            useVerboseDebug = true;
        }
        if (arg.startsWith("-scale="))
        {
            QString scaleValue = arg.mid(7); // Remove "-scale="
            bool ok;
            qreal scale = scaleValue.toDouble(&ok);
            if (ok && scale > 0.1 && scale <= 5.0) {
                forceScaleFactor = scale;
                printf("Forcing scale factor to: %.2f\n", forceScaleFactor);
            } else {
                printf("Invalid scale factor: %s (must be between 0.1 and 5.0)\n", scaleValue.toStdString().c_str());
            }
        }
    }

    DebugMsg::init(useDebug);
    DebugMsg::initVerbose(useVerboseDebug);

    // Start an external Xorg based viewer
    if (useVNC)
    {
        int i = system("./runvnc.sh");
        if (i < 0)
        {
            fprintf(stderr, "*** System Call to runvnc.sh returns %d ERROR ***\n", i);
        }
    }

    QCoreApplication::setOrganizationName("RetroGiovedi");
    QCoreApplication::setApplicationName("waffleCopyPRO-Universal");
    QSettings::setDefaultFormat(QSettings::IniFormat);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 always enables high-DPI scaling (e.g. from Xft.dpi on X11), which
    // stacks on top of our own scale factor. Disable it to behave like Qt5,
    // unless the user explicitly sets QT_ENABLE_HIGHDPI_SCALING.
    if (!qEnvironmentVariableIsSet("QT_ENABLE_HIGHDPI_SCALING"))
    {
        qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    }
#endif
    QApplication a(argc, argv);
#ifdef __APPLE__
    // On macOS, when running from a bundle, the working directory is the user's home dir.
    // We need to change it to the Resources directory inside the bundle, where the assets are.
    QDir::setCurrent(QCoreApplication::applicationDirPath() + "/../Resources");
#endif
    
    // Load and set Topaz font BEFORE creating MainWindow
    int id = -1; // Initialize id outside the ifdef
#ifdef __APPLE__
    QString fontPath = QCoreApplication::applicationDirPath() + "/../Resources/fonts/TopazPlus_a500_v1.0.ttf";
    id = QFontDatabase::addApplicationFont(fontPath);
#else
    // For other platforms, use the original relative path
    id = QFontDatabase::addApplicationFont("fonts/TopazPlus_a500_v1.0.ttf");
#endif
    
    // Calculate scale factor for highDPI screens
    QScreen *screen = QGuiApplication::primaryScreen();
    qreal scaleFactor = 1.0;
    
    // Check if scale factor was forced via command line
    if (forceScaleFactor > 0) {
        scaleFactor = forceScaleFactor;
        printf("Using forced scale factor: %.2f\n", scaleFactor);
    } else if (screen) {
        // Same auto-detection used by MainWindow (single implementation)
        scaleFactor = MainWindow::calculateScaleFactor();
        printf("Auto-detected scale factor: %.2f (Screen: %dx%d)\n", scaleFactor,
               screen->geometry().width(), screen->geometry().height());
    }
    
    // Store scale factor in QApplication property so MainWindow can access it
    a.setProperty("forceScaleFactor", scaleFactor);
    
    // Use the Topaz font if available, otherwise the platform default font:
    // in both cases the size is set in pixels and scaled, so text never
    // stays tiny on high-DPI screens (on macOS 1pt == 1px).
    QFont appFont = QApplication::font();
    if (id < 0)
    {
        DebugMsg::print(__func__, "Missing font!");
    }
    else
    {
        appFont = QFont(QFontDatabase::applicationFontFamilies(id).at(0));
    }

    const int basePixelSize = 12; // A 9-point font is typically ~12 pixels high
    appFont.setPixelSize(qMax(1, static_cast<int>(basePixelSize * scaleFactor)));
    a.setFont(appFont); // Set font globally for the entire application
    
    MainWindow w;
    w.show();
    return a.exec();
}
