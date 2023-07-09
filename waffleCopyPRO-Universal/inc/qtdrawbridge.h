#ifndef QTDRAWBRIDGE_H
#define QTDRAWBRIDGE_H

#include <QThread>
#include <QString>
#include <QStringList>

class QtDrawBridge : public QThread
{
public:
    QtDrawBridge();
    virtual void run();
    void setup(QString port, QString filename, QStringList command, QString track, QString side, QString status, QString error);

private:
    QString m_port;
    QString m_filename;
    QStringList m_command;
    QString m_track;
    QString m_side;
    QString m_status;
    QString m_error;
};

#endif // QTDRAWBRIDGE_H
