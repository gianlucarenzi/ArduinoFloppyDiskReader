#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QString>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    int id = QFontDatabase::addApplicationFont("fonts/TopazPlus_a500_v1.0.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont amigaTopaz(family);
    MainWindow w;
    w.setFont(amigaTopaz);
    w.show();
    return a.exec();
}
