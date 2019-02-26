#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QProcess>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QFileSystemWatcher *watcher;
    int track;
    QString side;
    int bad;
    QProcess avrfloppy;
    bool copyInProgress;

private slots:
    void copyDisk(bool);
    void progressChange(QString);
    void startAgain(void);

};

#endif // MAINWINDOW_H
