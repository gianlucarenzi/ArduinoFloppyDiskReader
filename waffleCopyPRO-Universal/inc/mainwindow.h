#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <clicklabel.h>
#include <QTimer>
#include <qtdrawbridge.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QLabel *upperTrack[82];
    QLabel *lowerTrack[82];
    void prepareTracks(void);
    QTimer *demoTimer;
    QtDrawBridge *amigaBridge;


private:
    void startWrite(void);
    void startRead(void);

private slots:
    void checkStartWrite(void);
    void checkStartRead(void);

    void on_fileReadADF_clicked();
    void on_fileSaveADF_clicked();

};
#endif // MAINWINDOW_H
