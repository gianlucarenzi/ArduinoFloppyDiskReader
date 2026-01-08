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
#include "diagnosticthread.h"
#include "waffleconfig.h"
#include <QCursor>
#include <QSettings>
#include <QSerialPortInfo>
#include <QSerialPort>
#include "socketserver.h"
#include "mikmodplayer.h"
#include "vumeterwidget.h"
#include <QLineEdit>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
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
    QTimer *serialPortRefreshTimer;
    QtDrawBridge *amigaBridge;
    DiagnosticThread *diagnosticThread;
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
    QString m_sDefaultPath;
    bool isDiagnosticVisible;
    QSettings settings;
    bool readyReadSHM;
    QThread *m_thread;
    void progressChange(QString s, int value);
    MikModPlayer *player;
    VuMeterWidget *vuMeter;
    QTimer *m_vuMeterTimer;
    void playMusic(void);
    bool playMOD;
    bool m_musicLoaded;
    bool m_musicPaused;
    QPoint m_diagnosticMousePressPos;
    QTimer *diagnosticTimeoutTimer;


private slots:
    void onDiagnosticTimeout();
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
    void onDiagnosticButtonClicked(void);
    void hideDiagnosticView(void);
    void onDiagnosticMessage(QString message);
    void onDiagnosticComplete(bool success);
    void drTrackChange(int rval);
    void drSideChange(int rval);
    void drStatusChange(int rval);
    void drErrorChange(int rval);
    void refreshSerialPorts();
    // MOD Player
    void handleSongFinished();
    void updateVuMeter();
    void toggleMusic();

protected:
    void resizeEvent(QResizeEvent *event);
    bool eventFilter(QObject *obj, QEvent *event) override;
};
#endif // MAINWINDOW_H
