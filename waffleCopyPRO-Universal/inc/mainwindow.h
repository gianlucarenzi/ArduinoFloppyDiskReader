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
#include "SerialIO.h"
#include "socketserver.h"

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
    QtDrawBridge *amigaBridge;
    SocketServer *socketServer;
    QFileSystemWatcher *watcher;
    QStringList fileList; // This file list is shared with the QThread for READING/WRITING from Waffle
    QString stext;
    bool readInProgress;
    bool writeInProgress;
    QCursor cursor;
    bool preComp;
    bool eraseBeforeWrite;
    bool tracks82;
    bool diskDriveHDensityMode;
    bool doRefresh;
    bool skipReadError;
    bool skipWriteError;
    void startWrite(void);
    void startRead(void);
    void showSetupError(QString err);
    QString m_track;
    QString m_side;
    QString m_status;
    QString m_folder;
    QString m_error;
    QSettings settings;
    int serialPort;
    bool readyReadSHM;
    QThread *m_thread;
    void progressChange(QString s, int value);
    SerialIO m_serialIO;

private slots:
    void checkStartWrite(void);
    void checkStartRead(void);
    void on_fileReadADF_clicked();
    void on_fileSaveADF_clicked();
    void doneWork();
    void doScroll();
    void stopClicked(void);
    void done(void);
    void togglePreComp(void);
    void toggleEraseBeforeWrite(void);
    void toggleNumTracks(void);
    void toggleDiskDensityMode(void);
    void toggleSkipReadError(void);
    void toggleSkipWriteError(void);
    void manageError(void);
    void wSysWatcher(void);
    void manageSerialPort(const QString &p);
    // Disk error management
    void errorDialog_RetryClicked(void);
    void errorDialog_SkipClicked(void);
    void errorDialog_CancelClicked(void);
    void manageQtDrawBridgeSignal(int rval);
    void drTrackChange(int rval);
    void drSideChange(int rval);
    void drStatusChange(int rval);
    void drErrorChange(int rval);
};
#endif // MAINWINDOW_H
