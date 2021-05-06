#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFile>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBar->setValue(0);
    // Connecting button
    connect(ui->readButton, SIGNAL(clicked(bool)), this, SLOT(copyDisk(bool)));
    watcher = new QFileSystemWatcher(this);
    QStringList fileList;
#if defined(__linux__) || defined(__APPLE__)
    fileList.append("/tmp/track");
    fileList.append("/tmp/side");
    fileList.append("/tmp/bad");
#else
    fileList.append("C:\tmptrack");
    fileList.append("C:\tmpside");
    fileList.append("C:\tmpbad");
#endif
    qDebug() << fileList.count();
    watcher->addPaths(fileList);
    connect(watcher, SIGNAL(fileChanged(QString)), this, SLOT(progressChange(QString)));
    bad = 0;
    QString t = QString::number(bad);
    ui->bad->setText(t);
    side = "Upper";
    ui->side->setText(side);
    track = 0;
    t = QString::number(track);
    ui->track->setText(t);
    copyInProgress = false;
    ui->finishBad->hide();
    ui->finishGood->hide();
    ui->shader->hide();
    // Z-Order
    ui->shader->raise();
    ui->finishBad->raise();
    ui->finishGood->raise();
    connect(ui->shader, SIGNAL(clicked()), this, SLOT(startAgain()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::startAgain()
{
    ui->shader->hide();
    ui->finishBad->hide();
    ui->finishGood->hide();
    copyInProgress = false;
    ui->readButton->setText("Copy Disk");
    ui->progressBar->setValue(0);
    ui->track->setText(0);
    track = 0;
    ui->side->setText("Upper");
    side = "Upper";
    bad = 0;
    ui->bad->setText("0");
}

void MainWindow::progressChange(QString s)
{
    //qDebug() << s << "changed";
    if (s == "/tmp/track")
    {
#if defined(__linux__) || defined(__APPLE__)
        QFile trackFile("/tmp/track");
#else
        QFile trackFile("C:\tmptrack");
#endif
        if (!trackFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray qbaTrack = trackFile.readLine();
        QString t = qbaTrack.data();
        track = t.toInt();
        ui->track->setText(t);
    }
    else
    if (s == "/tmp/side")
    {
#if defined(__linux__) || defined(__APPLE__)
        QFile sideFile("/tmp/side");
#else
        QFile sideFile("C:\tmpside");
#endif
        if (!sideFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray qbaSide = sideFile.readLine();
        QString t = qbaSide.data();
        side = t;
        ui->side->setText(t);
    }
    else
    if (s == "/tmp/bad")
    {
#if defined(__linux__) || defined(__APPLE__)
        QFile badFile("/tmp/bad");
#else
        QFile badFile("C:\tmpbad");
#endif
        if (!badFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray qbaBad = badFile.readLine();
        QString t = qbaBad.data();
        bad += t.toInt();
        ui->bad->setText(t);
    }
    // Calculate the progress
    int pct = (100 * track) / 79;
    ui->progressBar->setValue(pct);
    if (pct == 100)
    {
        ui->shader->show();
        if (bad != 0)
        {
            ui->finishBad->show();
            ui->bad->setText(QString::number(bad));
        }
        else
        {
            ui->finishGood->show();
        }
    }
}

void MainWindow::copyDisk(bool p)
{
    p = p;
    bool shouldStart = false;
    if (!copyInProgress)
    {
        //qDebug() << "Copy Button Pressed";
        // CHECK VALIDITY OF FILENAME
        if (ui->fileName->text().isNull() || ui->fileName->text().isEmpty())
        {
            //qDebug() << "No File Name Given";
            shouldStart = false;
        }
        else
        {
            //qDebug() << "Filename: " << ui->fileName->text();
            shouldStart = true;
        }

        if (shouldStart && (ui->serialPort->text().isNull() || ui->serialPort->text().isEmpty()))
        {
            shouldStart = false;
        }
        else
        {
           // qDebug() << "Serial Port " << ui->serialPort->text();
        }

        if (shouldStart)
        {
#if defined(__linux__) || defined(__APPLE__)
            QString cmdline = "../../avrfloppyreader ";
#else            
            QString cmdline = "../../avrfloppyreader.exe";
#endif
            cmdline += ui->serialPort->text() + " ";
            cmdline += ui->fileName->text() + " READ";
            //qDebug() << cmdline;
            avrfloppy.close();
            ui->readButton->setText("Stop Copy");
            copyInProgress = true;
            avrfloppy.start(cmdline);
        }
    }
    else
    {
        ui->readButton->setText("Copy Disk");
        copyInProgress = false;
        avrfloppy.close();
        ui->shader->show();
        ui->finishBad->show();
    }
}
