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
        if (arg.startsWith("--scale="))
        {
            QString scaleValue = arg.mid(8); // Remove "--scale="
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
    QApplication a(argc, argv);
#ifdef __APPLE__
    // On macOS, when running from a bundle, the working directory is the user's home dir.
    // We need to change it to the Resources directory inside the bundle, where the assets are.
    QDir::setCurrent(QCoreApplication::applicationDirPath() + "/../Resources");
#endif
    
    // Load and set Topaz font BEFORE creating MainWindow
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
        amigaTopaz.setPointSize(9); // Base size for Amiga Topaz font
        
        // Calculate scale factor for highDPI screens
        QScreen *screen = QGuiApplication::primaryScreen();
        qreal scaleFactor = 1.0;
        
        // Check if scale factor was forced via command line
        if (forceScaleFactor > 0) {
            scaleFactor = forceScaleFactor;
            printf("Using forced scale factor: %.2f\n", scaleFactor);
        } else if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            int screenWidth = screenGeometry.width();
            int screenHeight = screenGeometry.height();
            
            const int baseWidth = 1024;
            const int baseHeight = 610;
            
            qreal scale50Width = (screenWidth * 0.5) / baseWidth;
            qreal scale50Height = (screenHeight * 0.5) / baseHeight;
            qreal scale50 = qMin(scale50Width, scale50Height);
            
            qreal scale75Width = (screenWidth * 0.75) / baseWidth;
            qreal scale75Height = (screenHeight * 0.75) / baseHeight;
            qreal scale75 = qMin(scale75Width, scale75Height);
            
            if (screenWidth >= 3840 || screenHeight >= 2160) {
                scaleFactor = scale75;
            } else if (screenWidth >= 2560 || screenHeight >= 1440) {
                scaleFactor = (scale50 + scale75) / 2.0;
            } else if (screenWidth >= 1920 || screenHeight >= 1080) {
                scaleFactor = scale50 * 1.3;
            } else {
                scaleFactor = qMin(1.0, scale50);
            }
            
            scaleFactor = qMax(1.0, scaleFactor);
            printf("Auto-detected scale factor: %.2f (Screen: %dx%d)\n", scaleFactor, screenWidth, screenHeight);
        }
        
        // Store scale factor in QApplication property so MainWindow can access it
        a.setProperty("forceScaleFactor", scaleFactor);
        
        // Apply scaled font size
        int scaledFontSize = static_cast<int>(9 * scaleFactor);
        amigaTopaz.setPointSize(scaledFontSize);
        
        a.setFont(amigaTopaz); // Set font globally for the entire application
    }
    
    MainWindow w;
    w.show();
    return a.exec();
}
