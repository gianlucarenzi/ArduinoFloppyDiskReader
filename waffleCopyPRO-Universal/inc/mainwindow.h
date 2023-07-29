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
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

#define MAX_TRACKS 84
private:
    Ui::MainWindow *ui;
    int track;
    int side;
    int status;
    int err;
    QLabel *upperTrack[MAX_TRACKS];
    QLabel *lowerTrack[MAX_TRACKS];
    void prepareTracks(void);
    void prepareTracksPosition(void);
    QTimer *scrollTimer;
    QTimer *wSysTimer;
    QtDrawBridge *amigaBridge;
    QFileSystemWatcher *watcher;
    QStringList fileList; // This file list is shared with the QThread for READING/WRITING from Waffle
    QString stext;
    bool readInProgress;
    bool writeInProgress;
    QCursor cursor;
    bool preComp;
    bool eraseBeforeWrite;
    bool tracks82;
    bool doRefresh;
    void startWrite(void);
    void startRead(void);
    void showSetupError(QString err);
    void prepareFileSet(void);
    void resetFileCounters(void);
    QString m_track;
    QString m_side;
    QString m_status;
    QString m_folder;
    QString m_error;
    QSettings settings;
    int serialPort;
    bool readyReadSHM;

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
    void toggleEraseBeforeWrite(void);
    void toggleNumTracks(void);
    void manageError(void);
    void wSysWatcher(void);
    void manageSerialPort(int p);
    // Disk error management
    void errorDialog_RetryClicked(void);
    void errorDialog_SkipClicked(void);
    void errorDialog_CancelClicked(void);
    void manageQtDrawBridgeSignal(int rval);
};
#endif // MAINWINDOW_H
