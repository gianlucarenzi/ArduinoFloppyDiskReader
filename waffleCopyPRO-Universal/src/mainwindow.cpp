#include "adfwritermanager.h"
#include "inc/debugmsg.h"
#include "inc/waffleconfig.h"
#include <clicklabel.h>
#include <QFileDialog>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QFileInfo>
#include <QEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
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
#include "SerialIO.h"
#include <QScrollBar>
#include "mikmodplayer.h"
#include "vumeterwidget.h"
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QTranslator>
#include <QInputDialog>
#include <QLabel>
#include <QRandomGenerator>

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
  m_musicPaused(false),
  m_scaleFactor(1.0),
  m_simulationTimer(nullptr),
  m_simulationTrack(0),
  m_simulationSide(0),
  m_simulationMode(false),
  m_simulationSkipped(false),
  m_simulationIsWrite(false)
{
    ui->setupUi(this);

    // Check if scale factor was forced via QApplication property (from main.cpp)
    QVariant forcedScale = QApplication::instance()->property("forceScaleFactor");
    if (forcedScale.isValid()) {
        m_scaleFactor = forcedScale.toReal();
        DebugMsg::print(__func__, QString("Using forced scale factor from main: %1").arg(m_scaleFactor));
    } else {
        // Calculate scale factor based on screen resolution
        m_scaleFactor = calculateScaleFactor();
    }
    
    // Apply scaling to window size
    int baseWidth = 1024;
    int baseHeight = 615;
    int scaledWidth = static_cast<int>(baseWidth * m_scaleFactor);
    int scaledHeight = static_cast<int>(baseHeight * m_scaleFactor);
    DebugMsg::print(__func__, QString("W: %1 - H: %2").arg(scaledWidth).arg(scaledHeight));
    
    setFixedSize(scaledWidth, scaledHeight);
    setMinimumSize(scaledWidth, scaledHeight);
    setMaximumSize(scaledWidth, scaledHeight);

    // Scale all widgets recursively - must be done AFTER setFixedSize
    if (m_scaleFactor != 1.0) {
        scaleAllWidgets(ui->mainWindow);
    }

    // Manually scale fonts for specific labels that have hardcoded sizes in the UI file
    QFont sideFont = ui->upperSideDisk->font();
    const int basePixelSize = 20; // Original size is 15pt (~20px)
    sideFont.setPixelSize(qMax(1, static_cast<int>(basePixelSize * m_scaleFactor)));
    ui->upperSideDisk->setFont(sideFont);
    ui->lowerSideDisk->setFont(sideFont);

    // Load language from settings
    QString savedLanguage = settings.value("LANGUAGE", "en").toString();
    loadLanguage(savedLanguage);

    // Create WaffleFolder if not exists
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir homeDir(homePath);
    if (!homeDir.exists("WaffleFolder")) {
        homeDir.mkdir("WaffleFolder");
    }
    m_sDefaultPath = homeDir.filePath("WaffleFolder");

    // Load and scale cursor
    QPixmap cursorPixmap("WaffleUI/cursor.png");
    if (m_scaleFactor != 1.0 && !cursorPixmap.isNull()) {
        QSize scaledCursorSize(
            static_cast<int>(cursorPixmap.width() * m_scaleFactor),
            static_cast<int>(cursorPixmap.height() * m_scaleFactor)
        );
        cursorPixmap = cursorPixmap.scaled(scaledCursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    cursor = QCursor(cursorPixmap, 0, 0);
    this->setCursor(cursor);
    // Connection when click actions
    connect(ui->startWrite, SIGNAL(emitClick()), SLOT(checkStartWrite()));
    connect(ui->startRead, SIGNAL(emitClick()), SLOT(checkStartRead()));
    connect(ui->rightLogo, SIGNAL(emitClick()), SLOT(toggleScrollMode()));
    // Prepare all squares for tracks
    prepareTracks();
    prepareTracksPosition();
    refreshSerialPorts();

    // This add a QFileSystemWatcher on files
    //prepareFileSet();
    amigaBridge = new QtDrawBridge();

    connect(amigaBridge, SIGNAL(finished()), SLOT(doneWork()));
    connect(amigaBridge, SIGNAL(QtDrawBridgeSignal(int)), this, SLOT(manageQtDrawBridgeSignal(int)));

    ui->scrollText->setTextColor(QColor(255, 255, 255));
    QString empty = "                                                ";
    stext += empty;
    stext += empty;
    stext += empty;
    stext += empty;
    stext += empty;
    stext += empty;
    QString sctext;
    // The following QString should be localized
    sctext = tr(" --- WAFFLE COPY PROFESSIONAL v%1 --- The essential USB floppy drive solution for the real Amiga user."
    "  It allows you to write from ADF, SCP and IPF files, and read floppy disks as ADF or SCP format and, thanks to "
    "a specific version of an Amiga Emulator Software, like WinUAE (by Toni Wilen) or AmiBerry (by MiDWaN), it works "
    "like a real Amiga disk drive allowing you to directly read and write your floppies through an emulator! "
    "Sometime you may need a special USB cable (Y-Type) with the possibility of double powering if the USB port of the "
    "PC is not powerful enough. Original Concept by Rob Smith, modified version by Gianluca Renzi, "
    "Waffle is a product by RetroBit Lab and RetroGiovedi.").arg(WAFFLE_VERSION);
    stext += sctext;
    stext += empty;
    stext += tr(" --- CREDITS --- IPF support powered by CAPS image library. Copyright (C) Software Preservation Society. "
    "Music playback powered by libmikmod. Copyright (C) 1998-2004 Miodrag Vallat and others. "
    "Music: \"Stardust Memories\" by Jester/Sanity (C) 1992 Volker Tripp.");
    stext += empty;
    ui->scrollText->setText(stext);
    ui->scrollText->setInterval(60);
    ui->scrollText->startScroll();

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
    DebugMsg::print(__func__, "BEFORE PRECOMP: " + QString::number(preComp));
    DebugMsg::print(__func__, "BEFORE ERASEBEFOREWRITE: " + QString::number(eraseBeforeWrite));
    DebugMsg::print(__func__, "BEFORE TRACKS82: " + QString::number(tracks82));
    DebugMsg::print(__func__, "BEFORE SKIPREADERROR: " + QString::number(skipReadError));
    DebugMsg::print(__func__, "BEFORE SKIPWRITEERROR: " + QString::number(skipWriteError));

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
    ui->hdModeSelection->setChecked(diskDriveHDensityMode);
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
                DebugMsg::print(__func__, "WRITE INITIAL SETTINGS SERIALPORTNAME " + currentPort);
                settings.setValue("SERIALPORTNAME", currentPort);
                settings.sync();
            }
        }
    }

    DebugMsg::print(__func__, "AFTER PRECOMP: " + QString::number(preComp));
    DebugMsg::print(__func__, "AFTER ERASEBEFOREWRITE: " + QString::number(eraseBeforeWrite));
    DebugMsg::print(__func__, "AFTER TRACKS82: " + QString::number(tracks82));
    DebugMsg::print(__func__, "AFTER SKIPREADERROR: " + QString::number(skipReadError));
    DebugMsg::print(__func__, "AFTER SKIPWRITEERROR: " + QString::number(skipWriteError));

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

    // Apply Amiga font to all widgets
    applyAmigaFontToWidgets();

    // Create menu bar at the end, after all widgets are setup
    QMenuBar *menuBar = this->menuBar();
    menuBar->setFont(this->font());
    menuBar->setNativeMenuBar(false); // Force Qt menu bar instead of native (important for Linux/Windows)

    // Menu File
    QMenu *fileMenu = menuBar->addMenu(tr("File"));
    fileMenu->setFont(this->font());
    QAction *langAction = fileMenu->addAction(tr("Language Settings"));
    connect(langAction, &QAction::triggered, this, &MainWindow::onLanguageSettings);
    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Menu Help
    QMenu *helpMenu = menuBar->addMenu(tr("Help"));
    helpMenu->setFont(this->font());
    QAction *aboutAction = helpMenu->addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    
#ifdef QT_DEBUG
    // Menu Debug (for simulation) - only available in debug builds
    QMenu *debugMenu = menuBar->addMenu(tr("Debug"));
    debugMenu->setFont(this->font());
    QAction *simulateReadAction = debugMenu->addAction(tr("Simulate Read"));
    connect(simulateReadAction, &QAction::triggered, this, &MainWindow::startSimulation);
    QAction *simulateWriteAction = debugMenu->addAction(tr("Simulate Write"));
    connect(simulateWriteAction, &QAction::triggered, this, &MainWindow::startWriteSimulation);
#endif
}

qreal MainWindow::calculateScaleFactor()
{
    // Get primary screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return 1.0; // Fallback to no scaling
    }
    
    QRect screenGeometry = screen->availableGeometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();
    
    // Base dimensions
    const int baseWidth = 1024;
    const int baseHeight = 610;
    
    // Calculate scale factors for 50% and 75% of screen
    qreal scale50Width = (screenWidth * 0.5) / baseWidth;
    qreal scale50Height = (screenHeight * 0.5) / baseHeight;
    qreal scale50 = qMin(scale50Width, scale50Height);
    
    qreal scale75Width = (screenWidth * 0.75) / baseWidth;
    qreal scale75Height = (screenHeight * 0.75) / baseHeight;
    qreal scale75 = qMin(scale75Width, scale75Height);
    
    // Choose scale factor based on screen size
    qreal scaleFactor;
    if (screenWidth >= 3840 || screenHeight >= 2160) {
        // 4K or higher: use 75% of screen
        scaleFactor = scale75;
    } else if (screenWidth >= 2560 || screenHeight >= 1440) {
        // 2K or higher: use between 50-75% based on actual size
        scaleFactor = (scale50 + scale75) / 2.0;
    } else if (screenWidth >= 1920 || screenHeight >= 1080) {
        // Full HD: use 50-65% of screen
        scaleFactor = scale50 * 1.3;
    } else {
        // Lower resolutions: use original size or slight scaling
        scaleFactor = qMin(1.0, scale50);
    }
    
    // Ensure minimum scale factor of 1.0 (never downscale below original)
    scaleFactor = qMax(1.0, scaleFactor);
    
    DebugMsg::print(__func__, QString("Screen: %1x%2, Scale factor: %3")
                    .arg(screenWidth).arg(screenHeight).arg(scaleFactor));
    
    return scaleFactor;
}

void MainWindow::scaleWidgetGeometry(QWidget *widget)
{
    if (!widget) return;
    
    // Scale the widget's geometry
    QRect geom = widget->geometry();
    if (widget != this) { // Don't scale the main window geometry again
        widget->setGeometry(
            static_cast<int>(geom.x() * m_scaleFactor),
            static_cast<int>(geom.y() * m_scaleFactor),
            static_cast<int>(geom.width() * m_scaleFactor),
            static_cast<int>(geom.height() * m_scaleFactor)
        );
    }
    
    // Scale minimum and maximum size if set
    QSize minSize = widget->minimumSize();
    if (minSize.width() > 0 || minSize.height() > 0) {
        widget->setMinimumSize(
            static_cast<int>(minSize.width() * m_scaleFactor),
            static_cast<int>(minSize.height() * m_scaleFactor)
        );
    }
    
    QSize maxSize = widget->maximumSize();
    // Check if maxSize is not the default QWIDGETSIZE_MAX
    if (maxSize.width() < 16777215 || maxSize.height() < 16777215) {
        widget->setMaximumSize(
            static_cast<int>(maxSize.width() * m_scaleFactor),
            static_cast<int>(maxSize.height() * m_scaleFactor)
        );
    }
    
    // Recursively scale all child widgets
    QList<QWidget*> children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children) {
        scaleWidgetGeometry(child);
    }
}

void MainWindow::scaleAllWidgets(QWidget *widget)
{
    if (!widget) return;
    
    // Scale THIS widget's geometry (no skip condition)
    QRect geom = widget->geometry();
    widget->setGeometry(
        static_cast<int>(geom.x() * m_scaleFactor),
        static_cast<int>(geom.y() * m_scaleFactor),
        static_cast<int>(geom.width() * m_scaleFactor),
        static_cast<int>(geom.height() * m_scaleFactor)
    );
    
    // Scale minimum and maximum size if set
    QSize minSize = widget->minimumSize();
    if (minSize.width() > 0 || minSize.height() > 0) {
        widget->setMinimumSize(
            static_cast<int>(minSize.width() * m_scaleFactor),
            static_cast<int>(minSize.height() * m_scaleFactor)
        );
    }
    
    QSize maxSize = widget->maximumSize();
    // Check if maxSize is not the default QWIDGETSIZE_MAX
    if (maxSize.width() < 16777215 || maxSize.height() < 16777215) {
        widget->setMaximumSize(
            static_cast<int>(maxSize.width() * m_scaleFactor),
            static_cast<int>(maxSize.height() * m_scaleFactor)
        );
    }
    
    // Scale pixmaps in QLabel widgets
    QLabel *label = qobject_cast<QLabel*>(widget);
    if (label && label->pixmap() && !label->pixmap()->isNull()) {
        QPixmap originalPixmap = *label->pixmap();
        QSize scaledSize(
            static_cast<int>(originalPixmap.width() * m_scaleFactor),
            static_cast<int>(originalPixmap.height() * m_scaleFactor)
        );
        label->setPixmap(originalPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    // Recursively scale all child widgets
    QList<QWidget*> children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children) {
        scaleAllWidgets(child);
    }
}

void MainWindow::applyAmigaFontToWidgets()
{
    ui->preCompSelection->setFont(this->font());
    ui->eraseBeforeWrite->setFont(this->font());
    ui->numTracks->setFont(this->font());
    ui->startRead->setFont(this->font());
    ui->startWrite->setFont(this->font());
    ui->stopButton->setFont(this->font());
    ui->portSelection->setFont(this->font());
    // ui->version is set in UI file
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
    ui->diagnosticButton->setFont(this->font());
    
    // Platform-specific font size for diagnostic text
    // macOS: Amiga font + 3px, Windows/Linux: Amiga font + 2px
    QFont diagnosticFont = this->font();
#if defined(Q_OS_MACOS)
    diagnosticFont.setPixelSize(this->font().pixelSize() + 3);
#else
    diagnosticFont.setPixelSize(this->font().pixelSize() + 2);
#endif
    ui->diagnosticTest->setFont(diagnosticFont);
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
    DebugMsg::print(__func__, "WRITE SETTINGS SERIALPORTNAME: " + p);
    settings.setValue("SERIALPORTNAME", p);
    settings.sync();
}

void MainWindow::togglePreComp(void)
{
    preComp = !preComp;
    DebugMsg::print(__func__, "WRITE SETTINGS PRECOMP: " + QString::number(preComp));
    ui->preCompSelection->setChecked(preComp);
    // Store preComp into settings
    settings.setValue("PRECOMP", preComp);
    settings.sync();
}

void MainWindow::toggleEraseBeforeWrite(void)
{
    eraseBeforeWrite = !eraseBeforeWrite;
    DebugMsg::print(__func__, "WRITE SETTINGS ERASE BEFORE WRITE: " + QString::number(eraseBeforeWrite));
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
        DebugMsg::print(__func__, "WRITE SETTINGS 80 TRACKS");
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
    setOperationMode(false);
    DebugMsg::print(__func__, "DONE");
    readyReadSHM = false;
}

void MainWindow::stopClicked(void)
{
    ui->copyCompleted->setText(tr("OPERATION TERMINATED BY USER"));
    if (ui->errorDialog->isVisible())
        ui->errorDialog->hide();
    ui->stopButton->hide();
    if (m_simulationMode && m_simulationTimer) {
        m_simulationMode = false;
        m_simulationTimer->stop();
        setOperationMode(false);
    } else {
        amigaBridge->terminate();
        ArduinoFloppyReader::ADFWriterManager::getInstance().closeDevice(); // Close port on stop
    }
}

void MainWindow::toggleScrollMode(void)
{
    if (ui->scrollText->scrollMode() == SineScroller::CoarseMode) {
        ui->scrollText->setScrollMode(SineScroller::FineMode);
    } else {
        ui->scrollText->setScrollMode(SineScroller::CoarseMode);
    }
}

void MainWindow::prepareTracks(void)
{
    for (int j = 0; j < MAX_TRACKS; j++)
    {
       upperTrack[j] = new QLabel(ui->DiskTracksFrame);
       lowerTrack[j] = new QLabel(ui->DiskTracksFrame);
    }
    
    // Create the grid after creating track labels
    createTrackGrid();
}

void MainWindow::createTrackGrid(void)
{
    // Grid color (same as UPPER SIDE / LOWER SIDE text)
    const QColor gridColor(192, 191, 188);
    const QString gridStyle = QString("background-color: rgb(%1,%2,%3);").arg(gridColor.red()).arg(gridColor.green()).arg(gridColor.blue());
    const QString labelStyle = QString("color: rgb(%1,%2,%3); background-color: transparent;").arg(gridColor.red()).arg(gridColor.green()).arg(gridColor.blue());
    
    // Grid layout constants (base values, will be scaled)
    const int trackSize = 16;        // Track indicator size
    const int padding = 4;           // Increased padding around track (was 2)
    const int cellSize = trackSize + 2 * padding;  // 24x24 cell (was 20x20)
    const int cols = 10;
    const int rows = 9;
    const int labelSize = 16;        // Increased size for row/column labels (was 14)
    const int lineThickness = 2;     // Thickness for grid lines
    
    // Starting positions for each side (relative to DiskTracksFrame)
    const int upperStartX = 14;
    const int upperStartY = 60;
    const int lowerStartX = 343;
    const int lowerStartY = 60;
    
    // Create grids for both sides
    auto createGrid = [&](int startX, int startY, const QString &sidePrefix) {
        // Column numbers (0-9) at the top
        for (int col = 0; col < cols; col++) {
            QLabel *colLabel = new QLabel(ui->DiskTracksFrame);
            colLabel->setText(QString::number(col));
            colLabel->setStyleSheet(labelStyle);
            colLabel->setAlignment(Qt::AlignCenter);
            int x = static_cast<int>((startX + labelSize + col * cellSize) * m_scaleFactor);
            int y = static_cast<int>((startY - labelSize - 4) * m_scaleFactor);
            int w = static_cast<int>(cellSize * m_scaleFactor);
            int h = static_cast<int>(labelSize * m_scaleFactor);
            colLabel->setGeometry(x, y, w, h);
            colLabel->raise();
            colLabel->show();
        }
        
        // Row numbers (0-8) on the left
        for (int row = 0; row < rows; row++) {
            QLabel *rowLabel = new QLabel(ui->DiskTracksFrame);
            rowLabel->setText(QString::number(row));
            rowLabel->setStyleSheet(labelStyle);
            rowLabel->setAlignment(Qt::AlignCenter);
            int x = static_cast<int>((startX - 4) * m_scaleFactor);
            int y = static_cast<int>((startY + row * cellSize) * m_scaleFactor);
            int w = static_cast<int>(labelSize * m_scaleFactor);
            int h = static_cast<int>(cellSize * m_scaleFactor);
            rowLabel->setGeometry(x, y, w, h);
            rowLabel->raise();
            rowLabel->show();
        }
        
        // Horizontal lines (use QLabel instead of QFrame for better visibility)
        for (int row = 0; row <= rows; row++) {
            QLabel *line = new QLabel(ui->DiskTracksFrame);
            line->setStyleSheet(gridStyle);
            int x = static_cast<int>((startX + labelSize) * m_scaleFactor);
            int y = static_cast<int>((startY + row * cellSize) * m_scaleFactor);
            int w = static_cast<int>(cols * cellSize * m_scaleFactor);
            int h = static_cast<int>(lineThickness * m_scaleFactor);
            line->setGeometry(x, y, w, h);
            line->raise();
            line->show();
        }
        
        // Vertical lines (use QLabel instead of QFrame for better visibility)
        for (int col = 0; col <= cols; col++) {
            QLabel *line = new QLabel(ui->DiskTracksFrame);
            line->setStyleSheet(gridStyle);
            int x = static_cast<int>((startX + labelSize + col * cellSize) * m_scaleFactor);
            int y = static_cast<int>(startY * m_scaleFactor);
            int w = static_cast<int>(lineThickness * m_scaleFactor);
            int h = static_cast<int>(rows * cellSize * m_scaleFactor);
            line->setGeometry(x, y, w, h);
            line->raise();
            line->show();
        }
    };
    
    // Create upper side grid
    createGrid(upperStartX, upperStartY, "upper");
    
    // Create lower side grid
    createGrid(lowerStartX, lowerStartY, "lower");
}

void MainWindow::prepareTracksPosition(void)
{
    // Grid layout constants (base values, will be scaled) - must match createTrackGrid()
    const int trackSize = 16;        // Track indicator size
    const int padding = 4;           // Padding around track
    const int cellSize = 24;         // Cell size (explicit instead of calculated)
    const int cols = 10;
    const int labelSize = 16;        // Size for row/column labels
    const int lineThickness = 2;     // Line thickness
    
    // Starting positions for each side (relative to DiskTracksFrame)
    const int upperStartX = 14;
    const int upperStartY = 60;
    const int lowerStartX = 343;
    const int lowerStartY = 60;
    
    int scaledTrackSize = static_cast<int>(trackSize * m_scaleFactor);
    
    // Position upper tracks in grid (10 cols x 9 rows, only first 84)
    for (int j = 0; j < MAX_TRACKS; j++)
    {
        int row = j / cols;
        int col = j % cols;
        
        // Calculate position to center track in cell:
        // Cell starts at: startX + labelSize + lineThickness + (col * cellSize)
        // Track should be centered: + (cellSize - trackSize) / 2
        int baseX = upperStartX + labelSize + lineThickness + col * cellSize + (cellSize - trackSize) / 2;
        int baseY = upperStartY + lineThickness + row * cellSize + (cellSize - trackSize) / 2;
        
        int scaledX = static_cast<int>(baseX * m_scaleFactor);
        int scaledY = static_cast<int>(baseY * m_scaleFactor);
        
        upperTrack[j]->hide();
        upperTrack[j]->setGeometry(scaledX, scaledY, scaledTrackSize, scaledTrackSize);
        upperTrack[j]->setStyleSheet("background-color: rgb(0,0,0)");
    }
    
    // Position lower tracks in grid
    for (int j = 0; j < MAX_TRACKS; j++)
    {
        int row = j / cols;
        int col = j % cols;
        
        // Calculate position to center track in cell
        int baseX = lowerStartX + labelSize + lineThickness + col * cellSize + (cellSize - trackSize) / 2;
        int baseY = lowerStartY + lineThickness + row * cellSize + (cellSize - trackSize) / 2;
        
        int scaledX = static_cast<int>(baseX * m_scaleFactor);
        int scaledY = static_cast<int>(baseY * m_scaleFactor);
        
        lowerTrack[j]->hide();
        lowerTrack[j]->setGeometry(scaledX, scaledY, scaledTrackSize, scaledTrackSize);
        lowerTrack[j]->setStyleSheet("background-color: rgb(0,0,0)");
    }
    
    // Bring tracks to front (above grid lines)
    for (int j = 0; j < MAX_TRACKS; j++) {
        upperTrack[j]->raise();
        lowerTrack[j]->raise();
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
    if (!port.startsWith("/dev/"))
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
    if (!port.startsWith("/dev/"))
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

void MainWindow::setOperationMode(bool active)
{
    // Disable/enable all interactive controls during read/write operations
    ui->startWrite->setEnabled(!active);
    ui->startRead->setEnabled(!active);
    ui->serialPortComboBox->setEnabled(!active);
    ui->setADFFileName->setEnabled(!active);
    ui->fileSaveADF->setEnabled(!active);
    ui->getADFFileName->setEnabled(!active);
    ui->fileReadADF->setEnabled(!active);
    ui->preCompSelection->setEnabled(!active);
    ui->skipWriteError->setEnabled(!active);
    ui->eraseBeforeWrite->setEnabled(!active);
    ui->numTracks->setEnabled(!active);
    ui->skipReadError->setEnabled(!active);
    ui->hdModeSelection->setEnabled(!active);
    ui->diagnosticButton->setEnabled(!active);
    if (active)
        ui->DiskTracksFrame->raise();
}

void MainWindow::startWrite(void)
{
    QApplication::processEvents();
    amigaBridge->start();
    setOperationMode(true);
    ui->stopButton->raise();
    ui->stopButton->show();
}

void MainWindow::startRead(void)
{
    QApplication::processEvents();
    amigaBridge->start();
    setOperationMode(true);
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
                    ui->errorDialog->raise();
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
    set_user_input('S'); // Skip
    ui->errorDialog->hide();
    if (m_simulationMode && m_simulationTimer) {
        m_simulationSkipped = true; // advance without re-testing: square stays red
        m_simulationTimer->start(100);
    } else {
        serialPortRefreshTimer->start();
    }
}

void MainWindow::errorDialog_CancelClicked(void)
{
    //DebugMsg::print(__func__, "ABORT Clicked");
    set_user_input('A'); // Abort
    ui->errorDialog->hide();
    if (m_simulationMode && m_simulationTimer) {
        m_simulationMode = false;
        m_simulationTimer->stop();
        setOperationMode(false);
        ui->stopButton->setVisible(false);
    } else {
        stopClicked();
        serialPortRefreshTimer->start();
    }
}

void MainWindow::errorDialog_RetryClicked(void)
{
    //DebugMsg::print(__func__, "RETRY Clicked");
    set_user_input('R'); // Retry
    ui->errorDialog->hide();
    if (m_simulationMode && m_simulationTimer)
        m_simulationTimer->start(100);
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
    case 1: ui->copyError->setText(tr("Port In Use")); break;
    case 2: ui->copyError->setText(tr("Port Not Found")); break;
    case 3: ui->copyError->setText(tr("Port Error")); break;
    case 4: ui->copyError->setText(tr("Access Denied")); break;
    case 5: ui->copyError->setText(tr("Comport Config Error")); break;
    case 6: ui->copyError->setText(tr("BaudRate Not Supported")); break;
    case 7: ui->copyError->setText(tr("Error Reading Version")); break;
    case 8: ui->copyError->setText(tr("Error Malformed Version")); break;
    case 9: ui->copyError->setText(tr("Old Firmware")); break;
        // Responses from commands
    case 10: ui->copyError->setText(tr("Send Failed")); break;
    case 11: ui->copyError->setText(tr("Send Parameter Failed")); break;
    case 12: ui->copyError->setText(tr("Read Response Failed or No Disk in Drive")); break;
    case 13: ui->copyError->setText(tr("Write Timeout")); break;
    case 14: ui->copyError->setText(tr("Serial Overrun")); break;
    case 15: ui->copyError->setText(tr("Framing Error")); break;
    case 16: ui->copyError->setText(tr("Error")); break;

        // Response from selectTrack
    case 17: ui->copyError->setText(tr("Track Range Error")); break;
    case 18: ui->copyError->setText(tr("Select Track Error")); break;
    case 19: ui->copyError->setText(tr("Write Protected")); break;
    case 20: ui->copyError->setText(tr("Status Error")); break;
    case 21: ui->copyError->setText(tr("Send Data Failed")); break;
    case 22: ui->copyError->setText(tr("Track Write Response Error")); break;

        // Returned if there is no disk in the drive
    case 23: ui->copyError->setText(tr("No Disk In Drive")); break;

    case 24: ui->copyError->setText(tr("Diagnostic Not Available")); break;
    case 25: ui->copyError->setText(tr("USB Serial Bad")); break;
    case 26: ui->copyError->setText(tr("CTS Failure")); break;
    case 27: ui->copyError->setText(tr("Rewind Failure")); break;
    case 28: ui->copyError->setText(tr("Media Type Mismatch or No Disk in Drive")); break;
    default: ui->copyError->setText(tr("Unknown Error")); break;
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
        if (!portName.startsWith("/dev/"))
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
            upperTrack[i]->setStyleSheet("color: rgb(0, 0, 0);");
            lowerTrack[i]->setStyleSheet("color: rgb(0, 0, 0);");
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

    // Add all QSerialPortInfo ports (ONLY FTDI devices)
    QList<QString> addedPorts;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString portName = info.portName();
#ifndef _WIN32
        if (!portName.startsWith("/dev/"))
            portName.prepend("/dev/");
#endif
        // Accept only FTDI devices: VID 0x0403 and known FTDI PIDs, or product/manufacturer/description containing "ftdi"
        bool isFTDI = false;
        const quint16 FTDI_VID = 0x0403;
        static const QVector<quint16> FTDI_PIDS = { 0x6001, 0x6010, 0x6011, 0x6014 };
        if (info.hasVendorIdentifier() && info.hasProductIdentifier()) {
            if ((quint16)info.vendorIdentifier() == FTDI_VID && FTDI_PIDS.contains((quint16)info.productIdentifier())) isFTDI = true;
        }
        // Fallback by textual match
        if (!isFTDI) {
            QString desc = info.description().toLower();
            QString manuf = info.manufacturer().toLower();
            if (desc.contains("ftdi") || manuf.contains("ftdi") || info.portName().toLower().contains("ftdi")) isFTDI = true;
        }

        if (!isFTDI) continue;

        ui->serialPortComboBox->addItem(portName);
        addedPorts.append(portName);
    }

    // Add SerialIO ports if not already present
    std::vector<SerialIO::SerialPortInformation> serialPorts;
    SerialIO::enumSerialPorts(serialPorts);
    for (const auto &port : serialPorts) {
        std::string portNameA;
        quickw2a(port.portName, portNameA);
        QString portName = QString::fromStdString(portNameA);

        if (addedPorts.contains(portName)) continue;

        // Only add FTDI devices from SerialIO: require VID 0x0403 with known PIDs, or product name containing "ftdi"
        bool isFTDI = false;
        const unsigned int FTDI_VID = 0x0403;
        const unsigned int FTDI_PIDS[] = { 0x6001, 0x6010, 0x6011, 0x6014 };
        if (port.vid == FTDI_VID) {
            for (unsigned int p : FTDI_PIDS) if (port.pid == p) { isFTDI = true; break; }
        }
        if (!isFTDI && !port.productName.empty()) {
            std::string prodA;
            quickw2a(port.productName, prodA);
            std::transform(prodA.begin(), prodA.end(), prodA.begin(), ::tolower);
            if (prodA.find("ftdi") != std::string::npos) isFTDI = true;
        }

        if (!isFTDI) continue;

        ui->serialPortComboBox->addItem(portName);
        addedPorts.append(portName);
    }

#ifdef _WIN32
    // If no FTDI devices found, on Windows fall back to adding all serial ports (non-FTDI)
    if (addedPorts.isEmpty()) {
        DebugMsg::print(__func__, QString("No FTDI devices found on Windows; falling back to listing all serial ports"));
        // Add QSerialPortInfo ports without FTDI filtering
        for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
            QString portName = info.portName();
#ifndef _WIN32
            if (!portName.startsWith("/dev/"))
                portName.prepend("/dev/");
#endif
            if (addedPorts.contains(portName)) continue;
            ui->serialPortComboBox->addItem(portName);
            addedPorts.append(portName);
        }
        // Add SerialIO ports without FTDI filtering
        for (const auto &port : serialPorts) {
            std::string portNameA;
            quickw2a(port.portName, portNameA);
            QString portName = QString::fromStdString(portNameA);
            if (addedPorts.contains(portName)) continue;
            ui->serialPortComboBox->addItem(portName);
            addedPorts.append(portName);
        }
    }
#endif

    // Remove duplicate entries (normalize names) - ensure unique list
    {
        QSet<QString> seen;
        bool removedAny = false;
        for (int i = ui->serialPortComboBox->count()-1; i>=0; --i) {
            QString name = ui->serialPortComboBox->itemText(i);
#ifdef _WIN32
            QString key = name.toLower();
#else
            QString key = name;
            if (!key.startsWith("/dev/")) key.prepend("/dev/");
#endif
            if (seen.contains(key))
            {
            	ui->serialPortComboBox->removeItem(i);
            	removedAny = true;
            }
            else
            {
            	seen.insert(key);
            }
        }
        if (removedAny)
        {
        	DebugMsg::print(__func__, QString("Removed duplicate serial port entries"));
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
        int baseBarWidth = 24;
        int baseBarSpacing = 2;
        int scaledBarWidth = static_cast<int>(baseBarWidth * m_scaleFactor);
        int scaledBarSpacing = static_cast<int>(baseBarSpacing * m_scaleFactor);
        vuMeterWidth = channels * scaledBarWidth + (channels + 1) * scaledBarSpacing;
    } else {
        vuMeterWidth = static_cast<int>(100 * m_scaleFactor); // Fallback to default minimum width
    }

    // Calculate position
    int x = (this->width() - vuMeterWidth) / 2;
    int baseMargin = 5;
    int scaledMargin = static_cast<int>(baseMargin * m_scaleFactor);
    int y = this->height() - vuMeter->height() - scaledMargin; // 5 pixels from the bottom

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

void MainWindow::onLanguageSettings()
{
    QStringList languages;
    languages << "English" << "Italiano" << "Deutsch" << "Español" 
              << "Português" << "Français" << "Polski" << "Ελληνικά" 
              << "Magyar" << "Русский" << "Українська" << "日本語" << "简体中文"
              << "Română" << "Српски/Hrvatski" << "Čeština";
    
    QStringList codes;
    codes << "en" << "it" << "de" << "es" << "pt" << "fr" << "pl" << "el" << "hu" << "ru" << "uk" << "ja" << "zh_CN" << "ro" << "sr" << "cs";
    
    QString currentLang = settings.value("LANGUAGE", "en").toString();
    int currentIndex = codes.indexOf(currentLang);
    if (currentIndex < 0) currentIndex = 0;
    
    bool ok;
    QString selectedLang = QInputDialog::getItem(this, tr("Language Settings"),
                                                  tr("Select language:"),
                                                  languages, currentIndex, false, &ok);
    
    if (ok && !selectedLang.isEmpty()) {
        int index = languages.indexOf(selectedLang);
        if (index >= 0) {
            QString langCode = codes[index];
            changeLanguage(langCode);
        }
    }
}

void MainWindow::loadLanguage(const QString &language)
{
    qApp->removeTranslator(&m_translator);
    
    // Try loading from resources first, then from file system
    QString qmFile = ":/translations/wafflecopy_" + language + ".qm";
    if (QFile::exists(qmFile)) {
        if (m_translator.load(qmFile)) {
            qApp->installTranslator(&m_translator);
        }
    } else {
        // Fallback: try loading .ts file directly (Qt can load .ts files too)
        QString tsFile = "translations/wafflecopy_" + language + ".ts";
        if (QFile::exists(tsFile)) {
            if (m_translator.load(tsFile)) {
                qApp->installTranslator(&m_translator);
            }
        }
    }
    
    // Retranslate UI to apply the loaded language
    ui->retranslateUi(this);
    
    // For languages with non-Latin scripts (Greek, Russian, Ukrainian),
    // reduce font size slightly to prevent clipping when fallback fonts are used
    QFont currentFont = this->font();
    if (language == "el" || language == "ru" || language == "uk") {
        // Reduce by 1 point for Cyrillic and Greek
        currentFont.setPointSize(currentFont.pointSize() > 8 ? currentFont.pointSize() - 1 : 8);
        qApp->setFont(currentFont);
        this->setFont(currentFont);
    }
    
    // Reapply Amiga font to all widgets after retranslation
    applyAmigaFontToWidgets();
}

void MainWindow::changeLanguage(const QString &language)
{
    settings.setValue("LANGUAGE", language);
    
    qApp->removeTranslator(&m_translator);
    
    // Try loading from resources first, then from file system  
    QString qmFile = ":/translations/wafflecopy_" + language + ".qm";
    if (QFile::exists(qmFile)) {
        if (m_translator.load(qmFile)) {
            qApp->installTranslator(&m_translator);
        }
    } else {
        // Fallback: try loading .ts file directly
        QString tsFile = "translations/wafflecopy_" + language + ".ts";
        if (QFile::exists(tsFile)) {
            m_translator.load(tsFile);
            qApp->installTranslator(&m_translator);
        }
    }
    
    // Retranslate UI
    ui->retranslateUi(this);
    
    // For languages with non-Latin scripts (Greek, Russian, Ukrainian, Japanese, Chinese),
    // reduce font size slightly to prevent clipping when fallback fonts are used
    QFont currentFont = this->font();
    if (language == "el" || language == "ru" || language == "uk" || language == "ja" || language == "zh_CN") {
        // Reduce by 1 point for Cyrillic, Greek and CJK
        currentFont.setPointSize(currentFont.pointSize() > 8 ? currentFont.pointSize() - 1 : 8);
        qApp->setFont(currentFont);
        this->setFont(currentFont);
    } else {
        // Restore original font size for Latin scripts
        QFont originalFont = this->font();
        originalFont.setPointSize(9); // Original Topaz size
        qApp->setFont(originalFont);
        this->setFont(originalFont);
    }
    
    // Reapply Topaz font to all widgets after retranslation
    // This prevents font changes and clipping issues
    applyAmigaFontToWidgets();
    
    QMessageBox::information(this, tr("Language Changed"),
        tr("Language has been changed. The application will now close. Please restart it."));
    
    // Close the application so user can restart with new language
    qApp->quit();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About Waffle Copy Pro"),
        tr("<h3>Waffle Copy Professional v%1</h3>"
           "<p>The essential USB floppy drive solution for the real Amiga user.</p>"
           "<p><b>Original Concept:</b> Rob Smith<br>"
           "<b>Modified version by:</b> Gianluca Renzi<br>"
           "<b>Product by:</b> RetroBit Lab and RetroGiovedi</p>"
           "<p>IPF support powered by CAPS image library.<br>"
           "Music playback powered by libmikmod.</p>").arg(WAFFLE_VERSION));
}

void MainWindow::startSimulation()
{
    m_simulationIsWrite = false;
    if (m_simulationMode) {
        QMessageBox::warning(this, tr("Simulation"), tr("Simulation already running"));
        return;
    }
    
    // Initialize simulation
    m_simulationMode = true;
    m_simulationTrack = 0;
    m_simulationSide = 0;  // Start with side 0 (Lower)
    readyReadSHM = true;
    
    // Reset all tracks to black
    for (int i = 0; i < MAX_TRACKS; i++) {
        upperTrack[i]->setStyleSheet("background-color: rgb(0,0,0);");
        lowerTrack[i]->setStyleSheet("background-color: rgb(0,0,0);");
        upperTrack[i]->hide();
        lowerTrack[i]->hide();
    }
    
    // Disable controls, bring track frame to front (no overlay)
    setOperationMode(true);
    ui->stopButton->setVisible(true);
    ui->stopButton->raise();
    
    // Create and start timer
    if (!m_simulationTimer) {
        m_simulationTimer = new QTimer(this);
        connect(m_simulationTimer, &QTimer::timeout, this, &MainWindow::simulationStep);
    }
    
    m_simulationTimer->start(100); // Update every 100ms
    
    // Show a message about what the simulation will do
    QMessageBox::information(this, tr("Simulation Starting"),
        tr("Read simulation will process all %1 tracks:\n\n"
           "Track 0: Side 0 (Lower) then Side 1 (Upper)\n"
           "Track 1: Side 0 (Lower) then Side 1 (Upper)\n"
           "...and so on\n\n"
           "Side 0 = Lower row (right side)\n"
           "Side 1 = Upper row (left side)").arg(MAX_TRACKS));
}

void MainWindow::startWriteSimulation()
{
    m_simulationIsWrite = true;
    if (m_simulationMode) {
        QMessageBox::warning(this, tr("Simulation"), tr("Simulation already running"));
        return;
    }
    
    // Initialize simulation
    m_simulationMode = true;
    m_simulationTrack = 0;
    m_simulationSide = 0;  // Start with side 0 (Lower)
    readyReadSHM = true;
    
    // Reset all tracks to black
    for (int i = 0; i < MAX_TRACKS; i++) {
        upperTrack[i]->setStyleSheet("background-color: rgb(0,0,0);");
        lowerTrack[i]->setStyleSheet("background-color: rgb(0,0,0);");
        upperTrack[i]->hide();
        lowerTrack[i]->hide();
    }
    
    // Disable controls, bring track frame to front (no overlay)
    setOperationMode(true);
    ui->stopButton->setVisible(true);
    ui->stopButton->raise();
    
    // Create and start timer
    if (!m_simulationTimer) {
        m_simulationTimer = new QTimer(this);
        connect(m_simulationTimer, &QTimer::timeout, this, &MainWindow::simulationStep);
    }
    
    m_simulationTimer->start(100); // Update every 100ms
    
    // Show a message about what the simulation will do
    QMessageBox::information(this, tr("Simulation Starting"),
        tr("Write simulation will process all %1 tracks:\n\n"
           "Track 0: Side 0 (Lower) then Side 1 (Upper)\n"
           "Track 1: Side 0 (Lower) then Side 1 (Upper)\n"
           "...and so on\n\n"
           "Side 0 = Lower row (right side)\n"
           "Side 1 = Upper row (left side)").arg(MAX_TRACKS));
}

void MainWindow::simulationStep()
{
    if (!m_simulationMode) {
        return;
    }
    
    // Simulate all tracks up to MAX_TRACKS (84)
    int maxTracks = MAX_TRACKS;

    if (!m_simulationSkipped) {
        // Randomly decide if this track is successful or has an error (95% success rate)
        bool success = (QRandomGenerator::global()->bounded(100) < 95);

        // Update track/side/status variables as the real code does
        track = m_simulationTrack;
        side = m_simulationSide;
        status = success ? 0 : 1;
        err = success ? 0 : 1;

        // Use the existing progressChange logic
        readyReadSHM = true;
        progressChange("Track", track);
        progressChange("Side", side);
        progressChange("Status", status);
        progressChange("Error", err);

        // Force UI update
        QApplication::processEvents();

        // If an error dialog is shown waiting for user input, pause the timer
        if (err != 0 && !(m_simulationIsWrite ? skipWriteError : skipReadError) && ui->errorDialog->isVisible()) {
            m_simulationTimer->stop();
            return;
        }
    } else {
        // Skip was pressed: the track stays red, just advance
        m_simulationSkipped = false;
    }

    // Alternate between sides: 0->1, then move to next track
    m_simulationSide++;
    
    if (m_simulationSide >= 2) {
        m_simulationSide = 0;
        m_simulationTrack++;
        
        if (m_simulationTrack >= maxTracks) {
            // Simulation complete
            m_simulationMode = false;
            m_simulationTimer->stop();
            setOperationMode(false);
            ui->stopButton->setVisible(false);
            
            QMessageBox::information(this, tr("Simulation Complete"),
                tr("%1 simulation completed successfully.\n\n%2 tracks processed (both sides: lower and upper)")
                .arg(m_simulationIsWrite ? tr("Write") : tr("Read"))
                .arg(maxTracks));
        }
    }
}


