#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QDebug>

int main(int argc, char *argv[])
{
    bool useVNC = false;
    for (int i=0; i < argc; i++)
    {
        QString arg = argv[i];
        if (arg.contains("vnc"))
        {
            useVNC = true;
        }
    }

    // Start an external Xorg based viewer
    if (useVNC)
    {
        int i = system("./runvnc.sh");
        if (i < 0)
        {
            fprintf(stderr, "*** System Call to runvnc.sh returns %d ERROR ***\n", i);
        }
    }

    QApplication a(argc, argv);
    MainWindow w;
    int id = QFontDatabase::addApplicationFont("fonts/TopazPlus_a500_v1.0.ttf");
    if (id < 0)
    {
        qDebug() << __PRETTY_FUNCTION__ << "Missing font!";
    }
    else
    {
        QString family = QFontDatabase::applicationFontFamilies(id).at(0);
        QFont amigaTopaz(family);
        w.setFont(amigaTopaz);
    }
    w.show();
    return a.exec();
}
