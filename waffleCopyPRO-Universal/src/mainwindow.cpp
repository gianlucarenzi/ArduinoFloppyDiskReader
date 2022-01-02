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
#include "qtdrawbridge.h"

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
  ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Connection when click actions
    connect(ui->startWrite, SIGNAL(emitClick()), SLOT(checkStartWrite()));
    connect(ui->startRead, SIGNAL(emitClick()), SLOT(checkStartRead()));
    // Prepare all squares for tracks
    prepareTracks();
    demoTimer = new QTimer(this);
    demoTimer->setInterval(250);
    demoTimer->setSingleShot(false);
//    connect(demoTimer, SIGNAL(timeout()), SLOT(checkStartWrite()));
#ifdef _WIN32
    // In Windows the first available COM port is 1.
    ui->serial->setMinimum(1);
#endif
    amigaBridge = new QtDrawBridge();
    connect(amigaBridge, SIGNAL(finished()), SLOT(doneWork()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::prepareTracks(void)
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
       upperTrack[counter] = new QLabel(this);
       upperTrack[counter]->hide();
       upperTrack[counter]->setGeometry(ut[j][0], ut[j][1], 16, 16);
       upperTrack[counter]->setStyleSheet("background-color: rgb(0,255,0)");
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
       lowerTrack[counter] = new QLabel(this);
       lowerTrack[counter]->hide();
       lowerTrack[counter]->setGeometry(lt[j][0], lt[j][1], 16, 16);
       lowerTrack[counter]->setStyleSheet("background-color: rgb(255,255,0)");
       counter++;
    }
}

void MainWindow::checkStartWrite(void)
{
    qDebug() << "CHECK FOR WRITE";
    if (ui->getADFFileName->text().isEmpty())
    {
        qDebug() << "NEED ADF FILENAME First to write to floppy";
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
    QString command = " READ"; // Should be WRITE
    qDebug() << "PORT: " << port;
    qDebug() << "FILENAME: " << filename;
    qDebug() << "COMMAND: " << command;

    amigaBridge->setup(port, filename, command);
    startWrite();
}

void MainWindow::checkStartRead(void)
{
    qDebug() << "CHECK FOR READ";
    qDebug() << "START READ DISK";
    // This should start the reading from WAFFLE and write to ADF File
    if (ui->setADFFileName->text().isEmpty())
    {
        qDebug() << "NEED ADF FILENAME First to write to disk from floppy";
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
    QString command = " READ";
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
#if 0
    static int counter = 0;
    static bool toShow = true;
    qDebug() << "START WRITE DISK";
    // This should start the writing of the ADF File to WAFFLE in a Thread
    qDebug() << "UpperTrack " << counter << "ToShow:" << toShow << upperTrack[counter]->x() << upperTrack[counter]->y();
    qDebug() << "LowerTrack " << counter << "ToShow:" << toShow << lowerTrack[counter]->x() << lowerTrack[counter]->y();
    if (toShow) {
        upperTrack[counter]->show();
        lowerTrack[counter]->show();
    }
    else
    {
        upperTrack[counter]->hide();
        lowerTrack[counter]->hide();
    }

    counter++;
    if (counter > 81)
    {
        counter = 0;
        toShow = !toShow;
        demoTimer->stop();
    }
    else
    {
        demoTimer->start();
    }
#endif
}

void MainWindow::startRead(void)
{
    QApplication::processEvents();
    amigaBridge->start();
}

void MainWindow::on_fileReadADF_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select ADF FileName To be written on Waffle"),
                                                    ui->getADFFileName->text(),
                                                    tr("ADF Files(*.adf)"));
    if (!fileName.isEmpty())
        ui->getADFFileName->setText(fileName);
    demoTimer->start();
}

void MainWindow::on_fileSaveADF_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Write ADF FileName to be written on hard disk"),
                                                    ui->setADFFileName->text(),
                                                    tr("ADF Files(*.adf)"));
    if (!fileName.isEmpty())
        ui->setADFFileName->setText(fileName);
    demoTimer->start();
}
