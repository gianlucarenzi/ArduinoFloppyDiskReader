#include "adfwritermanager.h"
#include "inc/debugmsg.h"
#include <clicklabel.h>
#include <QFileDialog>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QFileInfo>
#include <QEvent>
#include <QMouseEvent>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdio.h>
#include <QString>
#include <QFont>
#include "qtdrawbridge.h"
#include <QCursor>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include "socketserver.h"
#include <QSerialPortInfo>
#include <QScrollBar>
#include "mikmodplayer.h"
#include "vumeterwidget.h"

extern void set_user_input(char data);

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
  ui(new Ui::MainWindow),
  track(-1),
  side(-1),
  status(-1),
  err(-1),
  diagnosticThread(nullptr),
  readInProgress(false),
  writeInProgress(false),
  preComp(true),
  eraseBeforeWrite(false),
  tracks82(false),
  diskDriveHDensityMode(false),
  doRefresh(true),
  skipReadError(false),
  skipWriteError(false),
  isDiagnosticVisible(false),
  player(new MikModPlayer(this, 20, 50)),
  m_musicLoaded(false),
  m_musicPaused(false)
{
    ui->setupUi(this);

    // Platform-specific font size for diagnostic widget
    // macOS: keep default (11pt), Windows/Linux: smaller (10pt)
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    QFont diagnosticFont = ui->diagnosticTest->font();
    diagnosticFont.setPointSize(10);
    ui->diagnosticTest->setFont(diagnosticFont);
#endif

    // Create WaffleFolder if not exists
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir homeDir(homePath);
    if (!homeDir.exists("WaffleFolder")) {
        homeDir.mkdir("WaffleFolder");
    }
    m_sDefaultPath = homeDir.filePath("WaffleFolder");

    cursor = QCursor(QPixmap("WaffleUI/cursor.png"), 0, 0);
    this->setCursor(cursor);
    // Connection when click actions
    connect(ui->startWrite, SIGNAL(emitClick()), SLOT(checkStartWrite()));
    connect(ui->startRead, SIGNAL(emitClick()), SLOT(checkStartRead()));
    // Prepare all squares for tracks
    prepareTracks();
    prepareTracksPosition();
    refreshSerialPorts();

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
    "  It allows you to write from ADF, SCP and IPF files, and read floppy disks as ADF or SCP format and, thanks to "
    "a specific version of an Amiga Emulator Software, like WinUAE (by Toni Wilen) or AmiBerry (by MiDWaN), it works "
    "like a real Amiga disk drive allowing you to directly read and write your floppies through an emulator! "
    "Sometime you may need a special USB cable (Y-Type) with the possibility of double powering if the USB port of the "
    "PC is not powerful enough. Original Concept by Rob Smith, modified version by Gianluca Renzi, "
    "Waffle is a product by RetroBit Lab and RetroGiovedi.");
    stext += sctext;
    stext += empty;
    ui->scrollText->setText(stext);
    scrollTimer = new QTimer();
    scrollTimer->setInterval(60);
    scrollTimer->setSingleShot(false);
    scrollTimer->start();
    connect(scrollTimer, SIGNAL(timeout()), this, SLOT(doScroll()));

    serialPortRefreshTimer = new QTimer(this);
    serialPortRefreshTimer->setInterval(3000); // Refresh every 3 seconds
    connect(serialPortRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshSerialPorts);
    serialPortRefreshTimer->start();
    ui->stopButton->hide(); // The stop button will be visible only when running disk copy
    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(stopClicked()));
    connect(ui->copyCompleted, SIGNAL(clicked()), this, SLOT(done()));
    connect(ui->copyError, SIGNAL(clicked()), this, SLOT(done()));
    connect(ui->preCompSelection, SIGNAL(clicked()), this, SLOT(togglePreComp()));
    connect(ui->eraseBeforeWrite, SIGNAL(clicked()), this, SLOT(toggleEraseBeforeWrite()));
    connect(ui->numTracks, SIGNAL(clicked()), this, SLOT(toggleNumTracks()));
    connect(ui->showError, SIGNAL(clicked()), this, SLOT(manageError()));
    connect(ui->serialPortComboBox, SIGNAL(currentTextChanged(const QString &)), this, SLOT(manageSerialPort(const QString &)));
    connect(ui->hdModeSelection, SIGNAL(clicked()), this, SLOT(toggleDiskDensityMode()));
    connect(ui->diagnosticButton, SIGNAL(clicked()), this, SLOT(onDiagnosticButtonClicked()));
    connect(ui->skipReadError, SIGNAL(clicked()), this, SLOT(toggleSkipReadError()));
    connect(ui->skipWriteError, SIGNAL(clicked()), this, SLOT(toggleSkipWriteError()));

    ui->busy->hide();

    ui->copyCompleted->hide();
    ui->copyError->hide();
    ui->diagnosticTest->hide();
    // Install event filter on diagnosticTest to capture clicks
    ui->diagnosticTest->viewport()->installEventFilter(this);
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
    DebugMsg::print(__func__, "BEFORE PRECOMP" + QString::number(preComp));
    DebugMsg::print(__func__, "BEFORE ERASEBEFOREWRITE" + QString::number(eraseBeforeWrite));
    DebugMsg::print(__func__, "BEFORE TRACKS82" + QString::number(tracks82));
    DebugMsg::print(__func__, "BEFORE SKIPREADERROR" + QString::number(skipReadError));
    DebugMsg::print(__func__, "BEFORE SKIPWRITEERROR" + QString::number(skipWriteError));

    preComp = settings.value("PRECOMP", true).toBool();
    eraseBeforeWrite = settings.value("ERASEBEFOREWRITE", false).toBool();
    tracks82 = settings.value("TRACKS82", false).toBool();
    skipReadError = settings.value("SKIPREADERROR", false).toBool();
    skipWriteError = settings.value("SKIPWRITEERROR", false).toBool();
    diskDriveHDensityMode = settings.value("HD", false).toBool();

    // Adapt the correct value to the ui
    ui->preCompSelection->setChecked(preComp);
    ui->eraseBeforeWrite->setChecked(eraseBeforeWrite);
    ui->numTracks->setChecked(tracks82);
    ui->skipReadError->setChecked(skipReadError);
    ui->skipWriteError->setChecked(skipWriteError);
    QString savedPortName = settings.value("SERIALPORTNAME", "").toString();
    if (!savedPortName.isEmpty()) {
        int index = ui->serialPortComboBox->findText(savedPortName);
        if (index != -1) {
            ui->serialPortComboBox->setCurrentIndex(index);
        }
    } else {
        if (ui->serialPortComboBox->count() > 0) {
            QString currentPort = ui->serialPortComboBox->currentText();
            if (!currentPort.isEmpty()) {
                DebugMsg::print(__func__, "WRITE INITIAL SETTINGS SERIALPORTNAME" + currentPort);
                settings.setValue("SERIALPORTNAME", currentPort);
                settings.sync();
            }
        }
    }

    DebugMsg::print(__func__, "AFTER PRECOMP" + QString::number(preComp));
    DebugMsg::print(__func__, "AFTER ERASEBEFOREWRITE" + QString::number(eraseBeforeWrite));
    DebugMsg::print(__func__, "AFTER TRACKS82" + QString::number(tracks82));
    DebugMsg::print(__func__, "AFTER SKIPREADERROR" + QString::number(skipReadError));
    DebugMsg::print(__func__, "AFTER SKIPWRITEERROR" + QString::number(skipWriteError));

    ui->errorDialog->hide();
    ui->errorDialog->raise();
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(errorDialog_CancelClicked()));
    connect(ui->skipButton, SIGNAL(clicked()), this, SLOT(errorDialog_SkipClicked()));
    connect(ui->retryButton, SIGNAL(clicked()), this, SLOT(errorDialog_RetryClicked()));
    readyReadSHM = false;

    DebugMsg::print(__func__, "Creating new socketServer");
    socketServer = new SocketServer();
    m_thread = new QThread();
    socketServer->moveToThread(m_thread);
    m_thread->start();
    connect(m_thread, SIGNAL(started()), socketServer, SLOT(process()));
    DebugMsg::print(__func__, "New socketServer is running");

    // Connect all thread from socketServer
    connect(socketServer, SIGNAL(drTrack(int)), this, SLOT(drTrackChange(int)));
    connect(socketServer, SIGNAL(drSide(int)), this, SLOT(drSideChange(int)));
    connect(socketServer, SIGNAL(drStatus(int)), this, SLOT(drStatusChange(int)));
    connect(socketServer, SIGNAL(drError(int)), this, SLOT(drErrorChange(int)));

    // VuMeter
    vuMeter = new VuMeterWidget(this); // Parent is MainWindow

    m_vuMeterTimer = new QTimer(this);
    connect(m_vuMeterTimer, &QTimer::timeout, this, &MainWindow::updateVuMeter);
    m_vuMeterTimer->setInterval(50);

    // Connect signals
    connect(player, &MikModPlayer::songFinished, this, &MainWindow::handleSongFinished);
    connect(ui->modPlayerButton, SIGNAL(emitClick()), this, SLOT(toggleMusic(void)));

    diagnosticTimeoutTimer = new QTimer(this);
    diagnosticTimeoutTimer->setSingleShot(true); // Ensures it fires only once
    connect(diagnosticTimeoutTimer, &QTimer::timeout, this, &MainWindow::onDiagnosticTimeout);
}

// Slot implementation for diagnostic timeout
void MainWindow::onDiagnosticTimeout()
{
    DebugMsg::print(__func__, "Diagnostic timeout occurred. Hiding diagnostic view.");
    hideDiagnosticView();
}

MainWindow::~MainWindow()
{
    // Stop and clean up diagnostic thread if running
    if (diagnosticThread) {
        if (diagnosticThread->isRunning()) {
            diagnosticThread->requestInterruption();
            if (!diagnosticThread->wait(3000)) {
                diagnosticThread->terminate();
                diagnosticThread->wait();
            }
        }
        delete diagnosticThread;
        diagnosticThread = nullptr;
    }

    m_vuMeterTimer->stop();
    player->stopPlayback();
    ArduinoFloppyReader::ADFWriterManager::getInstance().closeDevice(); // Ensure port is closed on exit
    delete ui;
}


void MainWindow::manageSerialPort(const QString &p)
{
    if (p.isEmpty())
        return;
    DebugMsg::print(__func__, "WRITE SETTINGS SERIALPORTNAME" + p);
    settings.setValue("SERIALPORTNAME", p);
    settings.sync();
}

void MainWindow::togglePreComp(void)
{
    preComp = !preComp;
    DebugMsg::print(__func__, "WRITE SETTINGS PRECOMP" + QString::number(preComp));
    ui->preCompSelection->setChecked(preComp);
    // Store preComp into settings
    settings.setValue("PRECOMP", preComp);
    settings.sync();
}

void MainWindow::toggleEraseBeforeWrite(void)
{
    eraseBeforeWrite = !eraseBeforeWrite;
    DebugMsg::print(__func__, "WRITE SETTINGS ERASE BEFORE WRITE" + QString::number(eraseBeforeWrite));
    ui->eraseBeforeWrite->setChecked(eraseBeforeWrite);
    settings.setValue("ERASEBEFOREWRITE", eraseBeforeWrite);
    settings.sync();
}

void MainWindow::toggleNumTracks(void)
{
    tracks82 = !tracks82;
    if (tracks82)
        DebugMsg::print(__func__, "WRITE SETTINGS 82 TRACKS");
    else
        DebugMsg::print(__func__, "WRITE SETTINGS 80 Tracks");
    ui->numTracks->setChecked(tracks82);
    settings.setValue("TRACKS82", tracks82);
    settings.sync();
}

void MainWindow::toggleDiskDensityMode(void)
{
    diskDriveHDensityMode = !diskDriveHDensityMode;
    if (diskDriveHDensityMode)
        DebugMsg::print(__func__, "USING HIGH DENSITY MODE");
    else
        DebugMsg::print(__func__, "USING DOUBLE DENSITY MODE (DEFAULT)");
    ui->hdModeSelection->setChecked(diskDriveHDensityMode);
    settings.setValue("HD", diskDriveHDensityMode);
    settings.sync();
}

void MainWindow::toggleSkipReadError(void)
{
    skipReadError = !skipReadError;
    if (skipReadError)
        DebugMsg::print(__func__, "SKIP READ ERROR ENABLED");
    else
        DebugMsg::print(__func__, "SKIP READ ERROR DISABLED");
    ui->skipReadError->setChecked(skipReadError);
    settings.setValue("SKIPREADERROR", skipReadError);
    settings.sync();
}

void MainWindow::toggleSkipWriteError(void)
{
    skipWriteError = !skipWriteError;
    if (skipWriteError)
        DebugMsg::print(__func__, "SKIP WRITE ERROR ENABLED");
    else
        DebugMsg::print(__func__, "SKIP WRITE ERROR DISABLED");
    ui->skipWriteError->setChecked(skipWriteError);
    settings.setValue("SKIPWRITEERROR", skipWriteError);
    settings.sync();
}

void MainWindow::doneWork(void)
{
    DebugMsg::print(__func__, "doneWork status:" + QString::number(status));
    if (status != 0) {
        ui->copyError->show();
        ui->copyError->raise();
    } else {
        ui->copyCompleted->show();
        ui->copyCompleted->raise();
    }
    readyReadSHM = false;
    serialPortRefreshTimer->start();
    ArduinoFloppyReader::ADFWriterManager::getInstance().closeDevice(); // Close port after operation
}

void MainWindow::done(void)
{
    ui->copyCompleted->hide();
    ui->copyError->hide();
    ui->busy->hide();
    ui->stopButton->hide();
    //prepareTracksPosition();
    DebugMsg::print(__func__, "DONE");
    readyReadSHM = false;
}

void MainWindow::stopClicked(void)
{
    ui->copyCompleted->setText(tr("OPERATION TERMINATED BY USER"));
    if (ui->errorDialog->isVisible())
        ui->errorDialog->hide();
    ui->stopButton->hide();
    amigaBridge->terminate();
    ArduinoFloppyReader::ADFWriterManager::getInstance().closeDevice(); // Close port on stop
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
    //DebugMsg::print(__func__, "Y: " + QString::number(y));
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
        ui->hdModeSelection->setFont(this->font());
        ui->skipReadError->setFont(this->font());
        ui->skipWriteError->setFont(this->font());
		QFont diagnosticFont = ui->diagnosticTest->font();
		diagnosticFont.setPointSize(diagnosticFont.pointSize() + 1);
		ui->diagnosticTest->setFont(diagnosticFont);
        ui->diagnosticButton->setFont(this->font());
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
    serialPortRefreshTimer->stop();
    DebugMsg::print(__func__, "CHECK FOR WRITE");
    DebugMsg::print(__func__, "START WRITE DISK");
    QString port = ui->serialPortComboBox->currentText();
    DebugMsg::print(__func__, "checkStartWrite: UI portName before modification =" + port);
#ifndef _WIN32
    port.prepend("/dev/");
#endif
    DebugMsg::print(__func__, "checkStartWrite: UI portName after modification =" + port);

    if (port.isEmpty()) {
        showSetupError(tr("ERROR: No serial port selected!\n\nPlease select a serial port from the dropdown menu."));
        return;
    }

    if (ui->getADFFileName->text().isEmpty())
    {
        DebugMsg::print(__func__, "NEED VALID IMAGE FILENAME First to write to floppy");
        showSetupError(tr("NEED VALID IMAGE FILENAME FIRST TO WRITE TO FLOPPY"));
        return;
    }



    QString filename = ui->getADFFileName->text();
    QFileInfo fileInfo(filename);
    if (!fileInfo.isAbsolute()) {
        filename = QDir(m_sDefaultPath).filePath(filename);
    }
    QStringList command;
    command.append("WRITE");
    command.append("VERIFY");

    if (preComp)
        command.append("PRECOMP");

    if (eraseBeforeWrite)
        command.append("ERASEBEFOREWRITE");

    if (diskDriveHDensityMode)
        command.append("HD");

    if (skipWriteError)
        command.append("SKIPWRITEERROR");

    DebugMsg::print(__func__, "PORT: " + port);
    DebugMsg::print(__func__, "FILENAME: " + filename);
    DebugMsg::print(__func__, "COMMAND: " + command.join(" "));

    DebugMsg::print(__func__, "Track: " + QString::number(track));
    DebugMsg::print(__func__, "Side: " + QString::number(side));
    DebugMsg::print(__func__, "Status: " + QString::number(status) + "NEED TO WRITE");

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
    serialPortRefreshTimer->stop();
    DebugMsg::print(__func__, "CHECK FOR READ");
    DebugMsg::print(__func__, "START READ DISK");
    // This should start the reading from WAFFLE and write to ADF File
    if (ui->setADFFileName->text().isEmpty())
    {
        DebugMsg::print(__func__, "NEED VALID IMAGE FILENAME First to write to disk from floppy");
        showSetupError("NEED VALID IMAGE FILENAME FIRST TO WRITE TO DISK FROM FLOPPY");
        return;
    }

    // To have the correct text on the copyCompleted.
    ui->copyCompleted->setText(tr("AMIGA DISK COPY COMPLETED"));
    QString port = ui->serialPortComboBox->currentText();
    DebugMsg::print(__func__, "checkStartRead: UI portName before modification =" + port);
#ifndef _WIN32
    port.prepend("/dev/");
#endif
    DebugMsg::print(__func__, "checkStartRead: UI portName after modification =" + port);

    if (port.isEmpty()) {
        showSetupError(tr("ERROR: No serial port selected!\n\nPlease select a serial port from the dropdown menu."));
        return;
    }



    QString filename = ui->setADFFileName->text();
    QFileInfo fileInfo(filename);
    if (!fileInfo.isAbsolute()) {
        filename = QDir(m_sDefaultPath).filePath(filename);
    }
    QStringList command;
    command.append("READ");
    if (tracks82)
        command.append("TRACKSNUM82");

    if (skipReadError)
        command.append("SKIPREADERROR");

    if (diskDriveHDensityMode)
        command.append("HD");

    DebugMsg::print(__func__, "PORT: " + port);
    DebugMsg::print(__func__, "FILENAME: " + filename);
    DebugMsg::print(__func__, "COMMAND: " + command.join(" "));

    DebugMsg::print(__func__, "Track: " + QString::number(track));
    DebugMsg::print(__func__, "Side: " + QString::number(side));
    DebugMsg::print(__func__, "Status: " + QString::number(status) + "NEED TO READ");

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
    ui->stopButton->raise();
    ui->stopButton->show();
}

void MainWindow::startRead(void)
{
    QApplication::processEvents();
    amigaBridge->start();
    ui->busy->show();
    ui->stopButton->raise();
    ui->stopButton->show();
}

void MainWindow::wSysWatcher(void)
{
    //DebugMsg::print(__func__, "TIMER FIRE!");
    progressChange("Timer", 0);
}

void MainWindow::progressChange(QString s, int value)
{
    bool toShow = false;

    if (readyReadSHM)
    {
        //DebugMsg::print(__func__, "called with" + s + "Value" + QString::number(value));
        if (s == "Track")   track = value;

        if (s == "Side" )   side = value;

        if (s == "Status" ) status = value;

        if (s == "Error" )
        {
            err = value;

            // Now we have the error!
            if (err != 0) {
                if (skipReadError) {
                    set_user_input('S'); // Simulate Skip
                    ui->errorDialog->hide();
                } else {
                    ui->errorDialog->show();
                }
            } else {
                ui->errorDialog->hide();
            }

            //DebugMsg::print(__func__, "ERRORCODE" + QString::number(err));
            if (err != 0)
            {
                // The only way to setup a 'RED' square, is when err != 0, the
                // square will be printed in RED
                status = 1;
                //DebugMsg::print(__func__, "ERRORCODE CHANGE TO RED");
            }
        }

        if (track < 0 || side < 0 || status < 0) toShow = false; else toShow = true;

        //DebugMsg::print(__func__, "TRACK: " + QString::number(track) + "SIDE: " + QString::number(side) + "STATUS: " + QString::number(status) + "ERROR: " + QString::number(err) + "toShow" + QString::number(toShow));
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
            //DebugMsg::print(__func__, "Nothing to show...");
        }
    } else {
        //DebugMsg::print(__func__, "Thread NOT READY YET");
    }
}

void MainWindow::drStatusChange(int val)
{
    //DebugMsg::print(__func__, "SIGNAL drSTATUS" + QString::number(val));
    progressChange("Status", val);
}

void MainWindow::drSideChange(int val)
{
    //DebugMsg::print(__func__, "SIGNAL drSIDE" + QString::number(val));
    progressChange("Side", val);
}

void MainWindow::drTrackChange(int val)
{
    //DebugMsg::print(__func__, "SIGNAL drTrack" + QString::number(val));
    progressChange("Track", val);
}

void MainWindow::drErrorChange(int val)
{
    //DebugMsg::print(__func__, "SIGNAL drError" + QString::number(val));
    progressChange("Error", val);
}


void MainWindow::on_fileReadADF_clicked()
{
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select Disk Image File"),
            m_sDefaultPath,
            tr("Disk Images (*.adf *.scp *.ima *.img *.st *.ipf);;ADF Files (*.adf);;SCP Files (*.scp);;IMA Files (*.ima);;IMG Files (*.img);;ST Files (*.st);;IPF Files (*.ipf)"));    if (!fileName.isEmpty())
        ui->getADFFileName->setText(fileName);

}

void MainWindow::on_fileSaveADF_clicked()
{
        QString fileName = QFileDialog::getSaveFileName(this, tr("Write Disk Image File to be written on hard disk"),
            m_sDefaultPath,
            tr("Disk Images (*.adf *.scp *.ima *.img *.st);;ADF Files (*.adf);;SCP Files (*.scp);;IMA Files (*.ima);;IMG Files (*.img);;ST Files (*.st)"));    if (!fileName.isEmpty())
        ui->setADFFileName->setText(fileName);

}

extern void set_user_input(char data);

void MainWindow::errorDialog_SkipClicked(void)
{
    //DebugMsg::print(__func__, "SKIP Clicked");
    set_user_input('S'); // Skip
    ui->errorDialog->hide();
    serialPortRefreshTimer->start();
}

void MainWindow::errorDialog_CancelClicked(void)
{
    //DebugMsg::print(__func__, "ABORT Clicked");
    set_user_input('A'); // Abort
    ui->errorDialog->hide();
    stopClicked();
    serialPortRefreshTimer->start();
}

void MainWindow::errorDialog_RetryClicked(void)
{
    //DebugMsg::print(__func__, "RETRY Clicked");
    set_user_input('R'); // Retry
    ui->errorDialog->hide();
}

void MainWindow::manageQtDrawBridgeSignal(int sig)
{
    bool toShow = true;
    //DebugMsg::print(__func__, "Signal RECEIVED FROM QtDrawBridge: " + QString::number(sig));
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
    case 12: ui->copyError->setText("Read Response Failed or No Disk in Drive"); break;
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
    case 28: ui->copyError->setText("Media Type Mismatch or No Disk in Drive"); break;
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

void MainWindow::hideDiagnosticView(void)
{
    if (isDiagnosticVisible && ui && ui->diagnosticTest) {
        // Set flag first to prevent any new signals from being processed
        isDiagnosticVisible = false;

        // Stop the diagnostic thread if it's still running
        if (diagnosticThread) {
            // Disconnect all signals immediately to prevent further widget updates
            disconnect(diagnosticThread, nullptr, this, nullptr);

            if (diagnosticThread->isRunning()) {
                DebugMsg::print(__func__, "Interrupting diagnostic thread...");
                diagnosticThread->requestInterruption();

                // Wait for thread to finish (max 5 seconds)
                if (!diagnosticThread->wait(5000)) {
                    DebugMsg::print(__func__, "Thread didn't finish in time, terminating forcefully");
                    diagnosticThread->terminate();
                    diagnosticThread->wait();
                }
                DebugMsg::print(__func__, "Diagnostic thread stopped");
            } else {
                DebugMsg::print(__func__, "Diagnostic thread already finished");
            }

            // Mark for deletion but don't delete immediately
            // The thread will be cleaned up when creating a new one or on app exit
            diagnosticThread->deleteLater();
            diagnosticThread = nullptr;
        } else {
            DebugMsg::print(__func__, "Diagnostic thread was null");
        }

        // Now safe to hide widgets
        if (ui->busy) {
            ui->busy->hide();
        }
        if (ui->diagnosticTest) {
            ui->diagnosticTest->hide();
        }
        diagnosticTimeoutTimer->stop(); // Stop the timer when hiding
        DebugMsg::print(__func__, "hideDiagnosticView completed");
    } else {
        DebugMsg::print(__func__, "hideDiagnosticView called but not visible or invalid UI");
    }
}

void MainWindow::onDiagnosticButtonClicked(void)
{
    DebugMsg::print(__func__, "DIAGNOSTIC BUTTON CLICKED");

    // Check if a diagnostic is already running
    if (diagnosticThread && diagnosticThread->isRunning()) {
        DebugMsg::print(__func__, "Diagnostic already running, ignoring request");
        return;
    }

    if (!isDiagnosticVisible) {
        // Ensure the serial port is closed before starting diagnostics
        ArduinoFloppyReader::ADFWriterManager::getInstance().closeDevice();

        // Get the selected serial port
        QString portName = ui->serialPortComboBox->currentText();
        DebugMsg::print(__func__, "onDiagnosticButtonClicked: UI portName before modification =" + portName);
#ifndef _WIN32
        portName.prepend("/dev/");
#endif
        DebugMsg::print(__func__, "onDiagnosticButtonClicked: UI portName after modification =" + portName);

        if (portName.isEmpty()) {
            // Show error if no port is selected
            ui->busy->show();
            ui->busy->raise();
            ui->diagnosticTest->clear();
            ui->diagnosticTest->append(tr("ERROR: No serial port selected!\n\nPlease select a serial port from the dropdown menu."));
            ui->diagnosticTest->show();
            ui->diagnosticTest->raise();
            isDiagnosticVisible = true;
            return;
        }

        // Clean up any existing thread that hasn't been deleted yet
        if (diagnosticThread) {
            DebugMsg::print(__func__, "Cleaning up old diagnostic thread");
            diagnosticThread->deleteLater();
            diagnosticThread = nullptr;
        }

        DebugMsg::print(__func__, "HIDING TRACKS");
        // Hide track indicators when diagnostic view is shown
        for (int i = 0; i < MAX_TRACKS; ++i) {
            upperTrack[i]->setStyleSheet("color: rgb(0, 0, 0");
            lowerTrack[i]->setStyleSheet("color: rgb(0, 0, 0");
            upperTrack[i]->hide();
            lowerTrack[i]->hide();
        }

#ifdef Q_OS_MAC
        ui->getADFFileName->setAttribute(Qt::WA_MacShowFocusRect, false);
        ui->setADFFileName->setAttribute(Qt::WA_MacShowFocusRect, false);
#endif

        // Show the diagnostic view
        ui->busy->show();
        ui->busy->raise();
        ui->diagnosticTest->clear();
        ui->diagnosticTest->append(tr("=== Waffle Copy Pro - Diagnostic Test ===\n"));
        ui->diagnosticTest->append(tr("Starting diagnostic on port: %1\n").arg(portName));
        ui->diagnosticTest->show();
        ui->diagnosticTest->raise();
        isDiagnosticVisible = true;

        diagnosticTimeoutTimer->start(30000); // 30 seconds timeout

        // Create and start the diagnostic thread
        diagnosticThread = new DiagnosticThread();
        connect(diagnosticThread, SIGNAL(diagnosticMessage(QString)), this, SLOT(onDiagnosticMessage(QString)));
        connect(diagnosticThread, SIGNAL(diagnosticComplete(bool)), this, SLOT(onDiagnosticComplete(bool)));
        // Do NOT auto-delete the thread - we manage it manually to avoid crashes

        diagnosticThread->setup(portName);
        diagnosticThread->start();
    } else {
        // Second click: hide
        hideDiagnosticView();
    }
}

void MainWindow::onDiagnosticMessage(QString message)
{
    // Send to debug output
    DebugMsg::print(__func__, "[DIAGNOSTIC]" + message);

    // Stop timeout timer when disk is detected - no more timeout once disk is present
    if (message.contains("Disk detected", Qt::CaseInsensitive)) {
        if (diagnosticTimeoutTimer && diagnosticTimeoutTimer->isActive()) {
            DebugMsg::print(__func__, "Disk detected - stopping diagnostic timeout timer");
            diagnosticTimeoutTimer->stop(); // Stop timer completely, diagnostic can now run without timeout
        }
    }

    // Use QMetaObject::invokeMethod to ensure we're in the right thread and timing
    if (isDiagnosticVisible && ui && ui->diagnosticTest) {
        QMetaObject::invokeMethod(ui->diagnosticTest, "append", Qt::QueuedConnection, Q_ARG(QString, message));

        // Auto-scroll to bottom (also queued)
        QMetaObject::invokeMethod(this, [this]() {
            if (isDiagnosticVisible && ui && ui->diagnosticTest) {
                QScrollBar *scrollBar = ui->diagnosticTest->verticalScrollBar();
                if (scrollBar) {
                    scrollBar->setValue(scrollBar->maximum());
                }
            }
        }, Qt::QueuedConnection);
    }
}

void MainWindow::onDiagnosticComplete(bool success)
{
    DebugMsg::print(__func__, "onDiagnosticComplete called, success:" + QString::number(success) + ", visible:" + QString::number(isDiagnosticVisible));

    if (isDiagnosticVisible && ui && ui->diagnosticTest) {
        // Use QMetaObject::invokeMethod to safely append messages
        QString finalMessage;
        if (success) {
            finalMessage = tr("\n=== ALL TESTS PASSED ===");
        } else {
            finalMessage = tr("\n=== SOME TESTS FAILED ===");
        }
        finalMessage += tr("\nClick anywhere on this window to close.");

        QMetaObject::invokeMethod(ui->diagnosticTest, "append", Qt::QueuedConnection, Q_ARG(QString, finalMessage));
        DebugMsg::print(__func__, "onDiagnosticComplete scheduled append");
    } else {
        DebugMsg::print(__func__, "Skipping append - widget not visible or invalid");
    }

    // Ensure the port is closed after diagnostics are finished
    ArduinoFloppyReader::ADFWriterManager::getInstance().closeDevice();
    diagnosticTimeoutTimer->stop(); // Stop the timer when diagnostics complete
    // Removed QTimer::singleShot to allow click-to-close behavior
    // QTimer::singleShot(2000, this, SLOT(hideDiagnosticView()));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    ui->busy->resize(event->size());
    QMainWindow::resizeEvent(event);
}

void MainWindow::refreshSerialPorts()
{
    ui->serialPortComboBox->blockSignals(true);
    QString currentPortName = ui->serialPortComboBox->currentText();
    ui->serialPortComboBox->clear();

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        // Check for FTDI Vendor ID and common Product IDs
        if (info.vendorIdentifier() == 0x0403 &&
            (info.productIdentifier() == 0x6001 ||
             info.productIdentifier() == 0x6010 ||
             info.productIdentifier() == 0x6014))
        {
            ui->serialPortComboBox->addItem(info.portName());
        }
    }

    // Restore previous selection if still available
    if (!currentPortName.isEmpty()) {
        int index = ui->serialPortComboBox->findText(currentPortName);
        if (index != -1) {
            ui->serialPortComboBox->setCurrentIndex(index);
        }
    }
    ui->serialPortComboBox->blockSignals(false);
}

void MainWindow::playMusic()
{
    // Directly load the module
    player->loadModule("WaffleUI/stardstm.mod");
    player->start();
    m_vuMeterTimer->start();
    m_musicLoaded = true;
    m_musicPaused = false;
}

void MainWindow::handleSongFinished()
{
    DebugMsg::print(__func__, "Song finished. Restart From beginning...");
    m_vuMeterTimer->stop();
    player->stopPlayback(); // Ensure playback is stopped
    vuMeter->startDecay();  // Start VU Meter decay
    playMusic();            // Restart from beginning
}

void MainWindow::updateVuMeter()
{
    if (player && player->isPlaying())
    {
        vuMeter->setAudioLevels(player->getCurrentLevels());
    }
}

void MainWindow::toggleMusic()
{
    DebugMsg::print(__func__, "toggleMusic called. m_musicLoaded:" + QString::number(m_musicLoaded) + "m_musicPaused:" + QString::number(m_musicPaused));

    // Calculate VU meter width based on channels
    int channels = player->getNumChannels();
    int vuMeterWidth = 0;
    if (channels > 0) {
        int barWidth = 24;
        int barSpacing = 2;
        vuMeterWidth = channels * barWidth + (channels + 1) * barSpacing;
    } else {
        vuMeterWidth = 100; // Fallback to default minimum width
    }

    // Calculate position
    int x = (this->width() - vuMeterWidth) / 2;
    int y = this->height() - vuMeter->height() - 5; // 5 pixels from the bottom

    if (!m_musicLoaded) {
        // State: Stopped -> Playing
        DebugMsg::print(__func__, "State: Stopped -> Playing");
        vuMeter->setGeometry(x, y, vuMeterWidth, vuMeter->height());
        vuMeter->show();
        playMusic();
    } else if (m_musicPaused) {
        // State: Paused -> Playing (Resume)
        DebugMsg::print(__func__, "State: Paused -> Playing (Resume)");
        vuMeter->setGeometry(x, y, vuMeterWidth, vuMeter->height());
        vuMeter->show();
        player->pausePlayback(); // Toggles pause off
        m_vuMeterTimer->start();
        m_musicPaused = false;
    } else {
        // State: Playing -> Paused
        DebugMsg::print(__func__, "State: Playing -> Paused");
        player->pausePlayback(); // Toggles pause on
        m_vuMeterTimer->stop();
        vuMeter->startDecay();
        // Hide the VU meter after it has decayed
        QTimer::singleShot(1000, this, [this]() {
            if (m_musicPaused) { // Check if it's still paused before hiding
                vuMeter->hide();
            }
        });
        m_musicPaused = true;
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->diagnosticTest->viewport() && isDiagnosticVisible) {
        // Only handle mouse events, let keyboard events pass through
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_diagnosticMousePressPos = mouseEvent->pos();
            }
            // Let the event propagate normally
            return QMainWindow::eventFilter(obj, event);
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // Calculate distance from press to release
                QPoint releasePos = mouseEvent->pos();
                int distance = (releasePos - m_diagnosticMousePressPos).manhattanLength();

                // Only close if it's a simple click (not a drag)
                // Allow tolerance of 5 pixels for accidental micro-movements
                if (distance <= 5) {
                    // Use QTimer to defer the close, avoiding potential issues
                    QTimer::singleShot(100, this, SLOT(hideDiagnosticView()));
                    return true; // Event handled
                }
            }
            // Let the event propagate normally
            return QMainWindow::eventFilter(obj, event);
        }
    }
    // Pass all other events to the parent class
    return QMainWindow::eventFilter(obj, event);
}

