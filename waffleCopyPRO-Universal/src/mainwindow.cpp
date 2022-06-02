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

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
  ui(new Ui::MainWindow),
  track(-1),
  side(-1),
  status(-1),
  readInProgress(false),
  writeInProgress(false),
  preComp(true),
  doRefresh(true)
{
    ui->setupUi(this);
    cursor = QCursor(QPixmap("WaffleUI/cursor.png"));
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
#endif
    watcher = new QFileSystemWatcher(this);
    fileList.append(TRACKFILE);
    fileList.append(SIDEFILE);
    fileList.append(STATUSFILE);
    watcher->addPaths(fileList);
    connect(watcher, SIGNAL(fileChanged(QString)), this, SLOT(progressChange(QString)));
    amigaBridge = new QtDrawBridge();
    connect(amigaBridge, SIGNAL(finished()), SLOT(doneWork()));
    ui->scrollText->setStyleSheet("color: rgb(255,255,255)");
    QString empty = "                                                ";
    stext += empty;
    stext += empty;
    stext += empty;
    // The following QString should be localized
    QString sctext = tr("The essential USB floppy drive for the real Amiga user."
    "It allows you to read and write ADF and, thanks to a specific version of AmiBerry (C) MiDWan, "
    "it works like a real Amiga drive allowing you to directly read and write your floppies! "
    "The package contains the black or white Waffle, a USB cable with the possibility of "
    "double powering if the USB port of the PC is not very powerful and a USB stick "
    "with all the necessary software to use the Waffle immediately.");
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
    ui->copyCompleted->hide();
    ui->copyError->hide();
    // Busy background is invisible now
    ui->busy->raise();
    ui->busy->hide();
    ui->stopButton->raise();
    ui->stopButton->hide();
    ui->showError->raise();
    ui->showError->hide();
    ui->preCompSelection->setFont(this->font());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::togglePreComp(void)
{
    preComp = !preComp;
    qDebug() << "PRECOMP" << preComp;
}

void MainWindow::doneWork(void)
{
    prepareTracksPosition();
    if (status != 0) {
        ui->copyError->show();
        ui->copyError->raise();
    } else {
        ui->copyCompleted->show();
        ui->copyCompleted->raise();
    }
}

void MainWindow::done(void)
{
    ui->copyCompleted->hide();
    ui->copyError->hide();
    ui->busy->hide();
    ui->stopButton->hide();
    prepareTracksPosition();
}

void MainWindow::stopClicked(void)
{
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
    if (doRefresh) {
        ui->preCompSelection->setFont(this->font());
        doRefresh = ! doRefresh;
    }
}

void MainWindow::prepareTracks(void)
{
    for (int j = 0; j < 82; j++)
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
    int ut[82][2] = {
        /* 0 */{394,240},{422,240},{450,240},{478,240},{505,240},{533,240},{561,240},{589,240},{617,240},{644,240},
        /* 1 */{394,267},{422,267},{450,267},{478,267},{505,267},{533,267},{561,267},{589,267},{617,267},{644,267},
        /* 2 */{394,295},{422,295},{450,295},{478,295},{505,295},{533,295},{561,295},{589,295},{617,295},{644,295},
        /* 3 */{394,323},{422,323},{450,323},{478,323},{505,323},{533,323},{561,323},{589,323},{617,323},{644,323},
        /* 4 */{394,350},{422,350},{450,350},{478,350},{505,350},{533,350},{561,350},{589,350},{617,350},{644,350},
        /* 5 */{394,379},{422,379},{450,379},{478,379},{505,379},{533,379},{561,379},{589,379},{617,379},{644,379},
        /* 6 */{394,406},{422,406},{450,406},{478,406},{505,406},{533,406},{561,406},{589,406},{617,406},{644,406},
        /* 7 */{394,434},{422,434},{450,434},{478,434},{505,434},{533,434},{561,434},{589,434},{617,434},{644,434},
        /* 8 */{394,461},{422,461}
    };
    for (j = 0; j < 82; j++)
    {
       upperTrack[counter]->hide();
       upperTrack[counter]->setGeometry(ut[j][0], ut[j][1], 16, 16);
       // All blacks
       upperTrack[counter]->setStyleSheet("background-color: rgb(0,0,0)");
       counter++;
    }
    // lut lower side track
    int lt[82][2] = {
        /* 0 */{723,240},{751,240},{779,240},{807,240},{834,240},{862,240},{890,240},{918,240},{945,240},{973,240},
        /* 1 */{723,267},{751,267},{779,267},{807,267},{834,267},{862,267},{890,267},{918,267},{945,267},{973,267},
        /* 2 */{723,295},{751,295},{779,295},{807,295},{834,295},{862,295},{890,295},{918,295},{945,295},{973,295},
        /* 3 */{723,323},{751,323},{779,323},{807,323},{834,323},{862,323},{890,323},{918,323},{945,323},{973,323},
        /* 4 */{723,350},{751,350},{779,350},{807,350},{834,350},{862,350},{890,350},{918,350},{945,350},{973,350},
        /* 5 */{723,379},{751,379},{779,379},{807,379},{834,379},{862,379},{890,379},{918,379},{945,379},{973,379},
        /* 6 */{723,406},{751,406},{779,406},{807,406},{834,406},{862,406},{890,406},{918,406},{945,406},{973,406},
        /* 7 */{723,434},{751,434},{779,434},{807,434},{834,434},{862,434},{890,434},{918,434},{945,434},{973,434},
        /* 8 */{723,461},{751,461}
    };
    counter = 0;
    for (j = 0; j < 82; j++)
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
        showError("NEED ADF FILENAME FIRST TO WRITE TO FLOPPY");
        return;
    }
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
    command.append("PRECOMP");
    qDebug() << "PORT: " << port;
    qDebug() << "FILENAME: " << filename;
    qDebug() << "COMMAND: " << command;

    amigaBridge->setup(port, filename, command);
    startWrite();
}

void MainWindow::showError(QString err)
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

void MainWindow::checkStartRead(void)
{
    qDebug() << "CHECK FOR READ";
    qDebug() << "START READ DISK";
    // This should start the reading from WAFFLE and write to ADF File
    if (ui->setADFFileName->text().isEmpty())
    {
        qDebug() << "NEED ADF FILENAME First to write to disk from floppy";
        showError("NEED ADF FILENAME FIRST TO WRITE TO DISK FROM FLOPPY");
        return;
    }
    QString port =
#ifdef _WIN32
    "COM";
#else
    "/dev/ttyUSB";
#endif
    int value = ui->serial->value();
    port += QString::number(value);

    QString filename = ui->setADFFileName->text();
    QStringList command;
    command.append("READ");
    qDebug() << "PORT: " << port;
    qDebug() << "FILENAME: " << filename;
    qDebug() << "COMMAND: " << command;

    amigaBridge->setup(port, filename, command);
    startRead();
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

void MainWindow::progressChange(QString s)
{
    QFile file;
    bool toShow = false;

    if (s == TRACKFILE)
    {
        file.setFileName(TRACKFILE);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray qbaTrack = file.readLine();
        QString t = qbaTrack.data();
        track = t.toInt();
        // Now we have the track!
        toShow = true;
    }
    else
    if (s == SIDEFILE)
    {
        file.setFileName(SIDEFILE);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray qbaSide = file.readLine();
        QString t = qbaSide.data();
        side = t.toInt();
        // Now we have the side!
        toShow = true;
    }
    else
    if (s == STATUSFILE)
    {
        file.setFileName(STATUSFILE);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray qbaStatus = file.readLine();
        QString t = qbaStatus.data();
        status = t.toInt();
        // Now we have the status!
        toShow = true;
    }
    else
    {
        qDebug() << "Nothing to do";
    }
    //qDebug() << "TRACK: " << track << "SIDE: " << side << "STATUS: " << status;
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
    }
}

void MainWindow::on_fileReadADF_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select ADF FileName To be written on Waffle"),
                                                    ui->getADFFileName->text(),
                                                    tr("ADF Files(*.adf)"));
    if (!fileName.isEmpty())
        ui->getADFFileName->setText(fileName);
 //   demoTimer->start();
}

void MainWindow::on_fileSaveADF_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Write ADF FileName to be written on hard disk"),
                                                    ui->setADFFileName->text(),
                                                    tr("ADF Files(*.adf)"));
    if (!fileName.isEmpty())
        ui->setADFFileName->setText(fileName);
//    demoTimer->start();
}
