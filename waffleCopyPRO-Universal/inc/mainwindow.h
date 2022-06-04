#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <clicklabel.h>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontDatabase>
#include "qtdrawbridge.h"
#include "waffleconfig.h"
#include <QCursor>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    int track;
    int side;
    int status;
    QLabel *upperTrack[82];
    QLabel *lowerTrack[82];
    void prepareTracks(void);
    void prepareTracksPosition(void);
    QTimer *scrollTimer;
    QtDrawBridge *amigaBridge;
    QFileSystemWatcher *watcher;
    QStringList fileList; // This file list is shared with the QThread for READING/WRITING from Waffle
    QString stext;
    bool readInProgress;
    bool writeInProgress;
    QCursor cursor;
    bool preComp;
    bool doRefresh;

private:
    void startWrite(void);
    void startRead(void);
    void showError(QString err);
    void prepareFileSet(void);

private slots:
    void checkStartWrite(void);
    void checkStartRead(void);
    void progressChange(QString s);
    void on_fileReadADF_clicked();
    void on_fileSaveADF_clicked();
    void doneWork();
    void doScroll();
    void stopClicked(void);
    void done(void);
    void togglePreComp(void);
};
#endif // MAINWINDOW_H
