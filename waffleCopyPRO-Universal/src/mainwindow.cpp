#include <QDebug>
#include <clicklabel.h>
#include <QFileDialog>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdio.h>
#include <QString>
#include <QFont>
#include "qtdrawbridge.h"
#include <QCursor>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QSettings>
#include "socketserver.h"

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
  ui(new Ui::MainWindow),
  track(-1),
  side(-1),
  status(-1),
  err(-1),
  readInProgress(false),
  writeInProgress(false),
  preComp(true),
  eraseBeforeWrite(false),
  tracks82(false),
  doRefresh(true),
  serialPort(1)
{
    ui->setupUi(this);
    cursor = QCursor(QPixmap("WaffleUI/cursor.png"), 0, 0);
    this->setCursor(cursor);
    // Connection when click actions
    connect(ui->startWrite, SIGNAL(emitClick()), SLOT(checkStartWrite()));
    connect(ui->startRead, SIGNAL(emitClick()), SLOT(checkStartRead()));
    // Prepare all squares for tracks
    prepareTracks();
    prepareTracksPosition();
#ifdef _WIN32
    // In Windows the first available COM port is 1.
    ui->serial->setMinimum(1);
    ui->portSelection->setText("WAFFLE DRIVE COM PORT");
#endif
    // This add a QFileSystemWatcher on files
    //prepareFileSet();
    amigaBridge = new QtDrawBridge();
    connect(amigaBridge, SIGNAL(finished()), SLOT(doneWork()));
    connect(amigaBridge, SIGNAL(QtDrawBridgeSignal(int)), this, SLOT(manageQtDrawBridgeSignal(int)));
    ui->scrollText->setStyleSheet("color: rgb(255,255,255)");
    QString empty = "                                                ";
    stext += empty;
    stext += empty;
    stext += empty;
    QString sctext;
    // The following QString should be localized
    sctext = tr(" --- WAFFLE COPY PROFESSIONAL --- The essential USB floppy drive solution for the real Amiga user."
    "  It allows you to write from ADF files, to read floppy disks as ADF, IPF and SCP format and, thanks to a specific version Amiga Emulator, like WinUAE (by Toni Wilen) or AmiBerry (by MiDWaN), "
    "it works like a real Amiga disk drive allowing you to directly read and write your floppies through an emulator! "
    "Sometime you may need a special USB cable (Y-Type) with the possibility of "
    "double powering if the USB port of the PC is not powerful enough. Original Concept by Rob Smith (C), modified version by Gianluca Renzi, Waffle is it a product by RetroBit Lab and RetroGiovedi.");
    stext += sctext;
    stext += empty;
    ui->scrollText->setText(stext);
    scrollTimer = new QTimer();
    scrollTimer->setInterval(60);
    scrollTimer->setSingleShot(false);
    scrollTimer->start();
    connect(scrollTimer, SIGNAL(timeout()), this, SLOT(doScroll()));
    ui->stopButton->hide(); // The stop button will be visible only when running disk copy
    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(stopClicked()));
    connect(ui->copyCompleted, SIGNAL(clicked()), this, SLOT(done()));
    connect(ui->copyError, SIGNAL(clicked()), this, SLOT(done()));
    connect(ui->preCompSelection, SIGNAL(clicked()), this, SLOT(togglePreComp()));
    connect(ui->eraseBeforeWrite, SIGNAL(clicked()), this, SLOT(toggleEraseBeforeWrite()));
    connect(ui->numTracks, SIGNAL(clicked()), this, SLOT(toggleNumTracks()));
    connect(ui->showError, SIGNAL(clicked()), this, SLOT(manageError()));
    connect(ui->serial, SIGNAL(valueChanged(int)), this, SLOT(manageSerialPort(int)));
    ui->copyCompleted->hide();
    ui->copyError->hide();
    // Busy background is invisible now
    ui->busy->raise();
    ui->busy->hide();
    ui->stopButton->raise();
    ui->stopButton->hide();
    ui->showError->raise();
    ui->showError->hide();
    ui->version->setText(WAFFLE_VERSION);
    ui->version->show();
    // Settings
    // If not exists default is true
    qDebug() << "BEFORE PRECOMP" << preComp;
    qDebug() << "BEFORE ERASEBEFOREWRITE" << eraseBeforeWrite;
    qDebug() << "BEFORE TRACKS82" << tracks82;
    qDebug() << "BEFORE COM PORT" << serialPort;

    preComp = settings.value("PRECOMP", true).toBool();
    eraseBeforeWrite = settings.value("ERASEBEFOREWRITE", false).toBool();
    tracks82 = settings.value("TRACKS82", false).toBool();
    serialPort = settings.value("SERIALPORT", 1).toInt();

    // Adapt the correct value to the ui
    ui->preCompSelection->setChecked(preComp);
    ui->eraseBeforeWrite->setChecked(eraseBeforeWrite);
    ui->numTracks->setChecked(tracks82);
    ui->serial->setValue(serialPort);

    qDebug() << "AFTER PRECOMP" << preComp;
    qDebug() << "AFTER ERASEBEFOREWRITE" << eraseBeforeWrite;
    qDebug() << "AFTER TRACKS82" << tracks82;
    qDebug() << "AFTER COM PORT" << serialPort;

    ui->errorDialog->hide();
    ui->errorDialog->raise();
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(errorDialog_CancelClicked()));
    connect(ui->skipButton, SIGNAL(clicked()), this, SLOT(errorDialog_SkipClicked()));
    connect(ui->retryButton, SIGNAL(clicked()), this, SLOT(errorDialog_RetryClicked()));
    readyReadSHM = false;

    qDebug() << "Creating new socketServer";
    socketServer = new SocketServer();
    m_thread = new QThread();
    socketServer->moveToThread(m_thread);
    m_thread->start();
    connect(m_thread, SIGNAL(started()), socketServer, SLOT(process()));
    qDebug() << "New socketServer is running";

    // Connect all thread from socketServer
    connect(socketServer, SIGNAL(drTrack(int)), this, SLOT(drTrackChange(int)));
    connect(socketServer, SIGNAL(drSide(int)), this, SLOT(drSideChange(int)));
    connect(socketServer, SIGNAL(drStatus(int)), this, SLOT(drStatusChange(int)));
    connect(socketServer, SIGNAL(drError(int)), this, SLOT(drErrorChange(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::manageSerialPort(int p)
{
    qDebug() << "WRITE SETTINGS SERIAL" << p;
    settings.setValue("SERIALPORT", p);
    settings.sync();
}

void MainWindow::togglePreComp(void)
{
    preComp = !preComp;
    qDebug() << "WRITE SETTINGS PRECOMP" << preComp;
    ui->preCompSelection->setChecked(preComp);
    // Store preComp into settings
    settings.setValue("PRECOMP", preComp);
    settings.sync();
}

void MainWindow::toggleEraseBeforeWrite(void)
{
    eraseBeforeWrite = !eraseBeforeWrite;
    qDebug() << "WRITE SETTINGS ERASE BEFORE WRITE" << eraseBeforeWrite;
    ui->eraseBeforeWrite->setChecked(eraseBeforeWrite);
    settings.setValue("ERASEBEFOREWRITE", eraseBeforeWrite);
    settings.sync();
}

void MainWindow::toggleNumTracks(void)
{
    tracks82 = !tracks82;
    if (tracks82)
        qDebug() << "WRITE SETTINGS 82 TRACKS";
    else
        qDebug() << "WRITE SETTINGS 80 Tracks";
    ui->numTracks->setChecked(tracks82);
    settings.setValue("TRACKS82", tracks82);
    settings.sync();
}

void MainWindow::doneWork(void)
{
    qDebug() << "doneWork" << "status:" << status;
    if (status != 0) {
        ui->copyError->show();
        ui->copyError->raise();
    } else {
        ui->copyCompleted->show();
        ui->copyCompleted->raise();
    }
    readyReadSHM = false;
}

void MainWindow::done(void)
{
    ui->copyCompleted->hide();
    ui->copyError->hide();
    ui->busy->hide();
    ui->stopButton->hide();
    //prepareTracksPosition();
    qDebug() << "DONE";
    readyReadSHM = false;
}

void MainWindow::stopClicked(void)
{
    ui->copyCompleted->setText(tr("OPERATION TERMINATED BY USER"));
    ui->stopButton->hide();
    amigaBridge->terminate();
}

void MainWindow::doScroll(void)
{
#define ArraySize(a) (sizeof(a) / sizeof(a[0]))

    int sinetab[] = { 0, -1, -3, -5, -7, -9, -11, -13, -12, -10, -8, -6, -4, -2, 0 };
    static int p = 0;
    static int ypos = ui->scrollText->pos().y();
    static unsigned int slt = 0;
    int i = stext.length();
    QString textToScroll;
    textToScroll = stext.mid(p); // From p to the end of string
    ui->scrollText->setText(textToScroll);
    p++;
    if (p >= i)
        p = 0;
    int y = ypos + sinetab[ slt ];
    //qDebug() << "Y: " << y;
    ui->scrollText->setGeometry(ui->scrollText->pos().x(), y,
                                ui->scrollText->width(), ui->scrollText->height());
    slt++;
    if (slt >= ArraySize(sinetab))
        slt = 0;
    // One time - setFont() attribute on the widgets
    if (doRefresh) {
        ui->preCompSelection->setFont(this->font());
        ui->eraseBeforeWrite->setFont(this->font());
        ui->numTracks->setFont(this->font());
        ui->startRead->setFont(this->font());
        ui->startWrite->setFont(this->font());
        ui->stopButton->setFont(this->font());
        ui->portSelection->setFont(this->font());
        ui->version->setFont(this->font());
        ui->showError->setFont(this->font());
        ui->copyCompleted->setFont(this->font());
        ui->copyError->setFont(this->font());
        ui->scrollText->setFont(this->font());
        ui->errorDialog->setFont(this->font());
        ui->retryButton->setFont(this->font());
        ui->cancelButton->setFont(this->font());
        ui->skipButton->setFont(this->font());
        ui->errorMessage->setFont(this->font());
        doRefresh = ! doRefresh;
    }
}

void MainWindow::prepareTracks(void)
{
    for (int j = 0; j < MAX_TRACKS; j++)
    {
       upperTrack[j] = new QLabel(this);
       lowerTrack[j] = new QLabel(this);
    }
}

void MainWindow::prepareTracksPosition(void)
{
    int counter = 0;
    int j;
    // lut upper side track
    int ut[MAX_TRACKS][2] = {
        /* 0 */{394,240},{422,240},{450,240},{478,240},{505,240},{533,240},{561,240},{589,240},{617,240},{644,240},
        /* 1 */{394,267},{422,267},{450,267},{478,267},{505,267},{533,267},{561,267},{589,267},{617,267},{644,267},
        /* 2 */{394,295},{422,295},{450,295},{478,295},{505,295},{533,295},{561,295},{589,295},{617,295},{644,295},
        /* 3 */{394,323},{422,323},{450,323},{478,323},{505,323},{533,323},{561,323},{589,323},{617,323},{644,323},
        /* 4 */{394,350},{422,350},{450,350},{478,350},{505,350},{533,350},{561,350},{589,350},{617,350},{644,350},
        /* 5 */{394,379},{422,379},{450,379},{478,379},{505,379},{533,379},{561,379},{589,379},{617,379},{644,379},
        /* 6 */{394,406},{422,406},{450,406},{478,406},{505,406},{533,406},{561,406},{589,406},{617,406},{644,406},
        /* 7 */{394,434},{422,434},{450,434},{478,434},{505,434},{533,434},{561,434},{589,434},{617,434},{644,434},
        /* 8 */{394,461},{422,461},{450,461},{478,461},
    };
    for (j = 0; j < MAX_TRACKS; j++)
    {
       upperTrack[counter]->hide();
       upperTrack[counter]->setGeometry(ut[j][0], ut[j][1], 16, 16);
       // All blacks
       upperTrack[counter]->setStyleSheet("background-color: rgb(0,0,0)");
       counter++;
    }
    // lut lower side track
    int lt[MAX_TRACKS][2] = {
        /* 0 */{723,240},{751,240},{779,240},{807,240},{834,240},{862,240},{890,240},{918,240},{945,240},{973,240},
        /* 1 */{723,267},{751,267},{779,267},{807,267},{834,267},{862,267},{890,267},{918,267},{945,267},{973,267},
        /* 2 */{723,295},{751,295},{779,295},{807,295},{834,295},{862,295},{890,295},{918,295},{945,295},{973,295},
        /* 3 */{723,323},{751,323},{779,323},{807,323},{834,323},{862,323},{890,323},{918,323},{945,323},{973,323},
        /* 4 */{723,350},{751,350},{779,350},{807,350},{834,350},{862,350},{890,350},{918,350},{945,350},{973,350},
        /* 5 */{723,379},{751,379},{779,379},{807,379},{834,379},{862,379},{890,379},{918,379},{945,379},{973,379},
        /* 6 */{723,406},{751,406},{779,406},{807,406},{834,406},{862,406},{890,406},{918,406},{945,406},{973,406},
        /* 7 */{723,434},{751,434},{779,434},{807,434},{834,434},{862,434},{890,434},{918,434},{945,434},{973,434},
        /* 8 */{723,461},{751,461},{779,461},{807,461},
    };
    counter = 0;
    for (j = 0; j < MAX_TRACKS; j++)
    {
       lowerTrack[counter]->hide();
       lowerTrack[counter]->setGeometry(lt[j][0], lt[j][1], 16, 16);
       // All blacks
       lowerTrack[counter]->setStyleSheet("background-color: rgb(0,0,0)");
       counter++;
    }
    ui->stopButton->hide();
}

void MainWindow::checkStartWrite(void)
{
    qDebug() << "CHECK FOR WRITE";
    if (ui->getADFFileName->text().isEmpty())
    {
        qDebug() << "NEED ADF FILENAME First to write to floppy";
        showSetupError(tr("NEED ADF FILENAME FIRST TO WRITE TO FLOPPY"));
        return;
    }

    // To have the correct text on the copyCompleted.
    ui->copyCompleted->setText(tr("AMIGA DISK COPY COMPLETED"));
    QString port =
#ifdef _WIN32
    "COM";
#else
    "/dev/ttyUSB";
#endif

    int value = ui->serial->value();
    port += QString::number(value);

    QString filename = ui->getADFFileName->text();
    QStringList command;
    command.append("WRITE");
    command.append("VERIFY");

    if (preComp)
        command.append("PRECOMP");

    if (eraseBeforeWrite)
        command.append("ERASEBEFOREWRITE");

    qDebug() << "PORT: " << port;
    qDebug() << "FILENAME: " << filename;
    qDebug() << "COMMAND: " << command;

    qDebug() << "Track: " << m_track;
    qDebug() << "Side: " << m_side;
    qDebug() << "Status: " << m_status << "NEED TO WRITE";

    prepareTracksPosition();
    amigaBridge->setup(port, filename, command);
    startWrite();
    // Now we can read the shared memory coming from the Thread
    readyReadSHM = true;
}

void MainWindow::showSetupError(QString err)
{
    ui->showError->setText(err);
    ui->showError->show();
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000)
    {
        qApp->processEvents();
    }
    ui->showError->hide();
}

void MainWindow::manageError(void)
{
    // Simply hide error window
    ui->showError->hide();
}

void MainWindow::checkStartRead(void)
{
    qDebug() << "CHECK FOR READ";
    qDebug() << "START READ DISK";
    // This should start the reading from WAFFLE and write to ADF File
    if (ui->setADFFileName->text().isEmpty())
    {
        qDebug() << "NEED ADF FILENAME First to write to disk from floppy";
        showSetupError("NEED ADF FILENAME FIRST TO WRITE TO DISK FROM FLOPPY");
        return;
    }

    // To have the correct text on the copyCompleted.
    ui->copyCompleted->setText(tr("AMIGA DISK COPY COMPLETED"));
    QString port =
#ifdef _WIN32
    "COM";
#else
    "/dev/ttyUSB";
#endif
    int value = ui->serial->value();
    port += QString::number(value);
    qDebug() << "WRITE SETTINGS SERIALPORT " << value;
    settings.setValue("SERIALPORT", value);

    QString filename = ui->setADFFileName->text();
    QStringList command;
    command.append("READ");
    if (tracks82)
        command.append("TRACKSNUM82");

    qDebug() << "PORT: " << port;
    qDebug() << "FILENAME: " << filename;
    qDebug() << "COMMAND: " << command;

    qDebug() << "Track: " << m_track;
    qDebug() << "Side: " << m_side;
    qDebug() << "Status: " << m_status << "NEED TO READ";

    prepareTracksPosition();
    amigaBridge->setup(port, filename, command);
    startRead();
    // Now we can read the shared memory coming from the Thread
    readyReadSHM = true;
}

void MainWindow::startWrite(void)
{
    QApplication::processEvents();
    amigaBridge->start();
    ui->busy->show();
    ui->stopButton->show();
}

void MainWindow::startRead(void)
{
    QApplication::processEvents();
    amigaBridge->start();
    ui->busy->show();
    ui->stopButton->show();
}

void MainWindow::wSysWatcher(void)
{
    //qDebug() << "TIMER FIRE!";
    progressChange("Timer", 0);
}

void MainWindow::progressChange(QString s, int value)
{
    bool toShow = false;

    if (readyReadSHM)
    {
        //qDebug() << __PRETTY_FUNCTION__ << "called with" << s << "Value" << value;
        if (s == "Track")   track = value;

        if (s == "Side" )   side = value;

        if (s == "Status" ) status = value;

        if (s == "Error" )
        {
            err = value;

            // Now we have the error!
            if (err != 0)
                ui->errorDialog->show();
            else
                ui->errorDialog->hide();

            //qDebug() << __PRETTY_FUNCTION__ << "ERRORCODE" << err;
            if (err != 0)
            {
                // The only way to setup a 'RED' square, is when err != 0, the
                // square will be printed in RED
                status = 1;
                //qDebug() << __PRETTY_FUNCTION__ << "ERRORCODE CHANGE TO RED";
            }
        }

        if (track < 0 || side < 0 || status < 0) toShow = false; else toShow = true;

        //qDebug() << __PRETTY_FUNCTION__ << "TRACK: " << track << "SIDE: " << side << "STATUS: " << status << "ERROR: " << err << "toShow" << toShow;
        if (toShow)
        {
            // Error = red squares. good green or yellow if verify
            switch(side)
            {
            case 0: // Lower Side
                if (status != 0)
                    lowerTrack[track]->setStyleSheet("background-color: rgb(255,0,0)");
                else
                    lowerTrack[track]->setStyleSheet("background-color: rgb(0,255,0)");
                lowerTrack[track]->show();
                break;
            case 1:
                if (status != 0)
                    upperTrack[track]->setStyleSheet("background-color: rgb(255,0,0)");
                else
                    upperTrack[track]->setStyleSheet("background-color: rgb(0,255,0)");
                upperTrack[track]->show();
                break;
            }
        } else {
            //qDebug() << "Nothing to show...";
        }
    } else {
        //qDebug() << "Thread NOT READY YET";
    }
}

void MainWindow::drStatusChange(int val)
{
    //qDebug() << __PRETTY_FUNCTION__ << "SIGNAL drSTATUS" << val;
    progressChange("Status", val);
}

void MainWindow::drSideChange(int val)
{
    //qDebug() << __PRETTY_FUNCTION__ << "SIGNAL drSIDE" << val;
    progressChange("Side", val);
}

void MainWindow::drTrackChange(int val)
{
    //qDebug() << __PRETTY_FUNCTION__ << "SIGNAL drTrack" << val;
    progressChange("Track", val);
}

void MainWindow::drErrorChange(int val)
{
    //qDebug() << __PRETTY_FUNCTION__ << "SIGNAL drError" << val;
    progressChange("Error", val);
}


void MainWindow::on_fileReadADF_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select ADF FileName To be written on Waffle"),
                                                    ui->getADFFileName->text(),
                                                    tr("ADF Files(*.adf)"));
    if (!fileName.isEmpty())
        ui->getADFFileName->setText(fileName);

}

void MainWindow::on_fileSaveADF_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Write ADF FileName to be written on hard disk"),
                                                    ui->setADFFileName->text(),
                                                    tr("ADF Files(*.adf)"));
    if (!fileName.isEmpty())
        ui->setADFFileName->setText(fileName);

}

extern void set_user_input(char data);

void MainWindow::errorDialog_SkipClicked(void)
{
    //qDebug() << __FUNCTION__ << "SKIP Clicked";
    set_user_input('S'); // Skip
    ui->errorDialog->hide();
}

void MainWindow::errorDialog_CancelClicked(void)
{
    //qDebug() << __FUNCTION__ << "ABORT Clicked";
    set_user_input('A'); // Abort
    ui->errorDialog->hide();
    stopClicked();
}

void MainWindow::errorDialog_RetryClicked(void)
{
    //qDebug() << __FUNCTION__ << "RETRY Clicked";
    set_user_input('R'); // Retry
    ui->errorDialog->hide();
}

void MainWindow::manageQtDrawBridgeSignal(int sig)
{
    bool toShow = true;
    //qDebug() << __FUNCTION__ << "Signal RECEIVED FROM QtDrawBridge: " << sig;
    status = sig;
    switch (sig)
    {
    case 0: toShow = false; break; // No error
        // Responses from openPort
    case 1: ui->copyError->setText("Port In Use"); break;
    case 2: ui->copyError->setText("Port Not Found"); break;
    case 3: ui->copyError->setText("Port Error"); break;
    case 4: ui->copyError->setText("Access Denied"); break;
    case 5: ui->copyError->setText("Comport Config Error"); break;
    case 6: ui->copyError->setText("BaudRate Not Supported"); break;
    case 7: ui->copyError->setText("Error Reading Version"); break;
    case 8: ui->copyError->setText("Error Malformed Version"); break;
    case 9: ui->copyError->setText("Old Firmware"); break;
        // Responses from commands
    case 10: ui->copyError->setText("Send Failed"); break;
    case 11: ui->copyError->setText("Send Parameter Failed"); break;
    case 12: ui->copyError->setText("Read Response Failed"); break;
    case 13: ui->copyError->setText("Write Timeout"); break;
    case 14: ui->copyError->setText("Serial Overrun"); break;
    case 15: ui->copyError->setText("Framing Error"); break;
    case 16: ui->copyError->setText("Error"); break;

        // Response from selectTrack
    case 17: ui->copyError->setText("Track Range Error"); break;
    case 18: ui->copyError->setText("Select Track Error"); break;
    case 19: ui->copyError->setText("Write Protected"); break;
    case 20: ui->copyError->setText("Status Error"); break;
    case 21: ui->copyError->setText("Send Data Failed"); break;
    case 22: ui->copyError->setText("Track Write Response Error"); break;

        // Returned if there is no disk in the drive
    case 23: ui->copyError->setText("No Disk In Drive"); break;

    case 24: ui->copyError->setText("Diagnostic Not Available"); break;
    case 25: ui->copyError->setText("USB Serial Bad"); break;
    case 26: ui->copyError->setText("CTS Failure"); break;
    case 27: ui->copyError->setText("Rewind Failure"); break;
    case 28: ui->copyError->setText("Media Type Mismatch"); break;
    default: ui->copyError->setText("Unknown Error"); break;
    }

    if (toShow) {
        ui->copyError->show();
        ui->copyError->raise();
        // When it's halted for an error, sometimes the LED (motor) is spinning
        // due to the last issued command.
        // How to invoke: ./drawbridge /dev/comport SETTINGS ?
        // Without issuing a complete thread?

    }

    readyReadSHM = false;
}
