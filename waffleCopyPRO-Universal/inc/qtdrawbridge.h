#ifndef QTDRAWBRIDGE_H
#define QTDRAWBRIDGE_H

#include <QThread>
#include <QString>

class QtDrawBridge : public QThread
{
public:
    QtDrawBridge();
    virtual void run();
    void setup(QString port, QString filename, QString command);

private:
    QString m_port;
    QString m_filename;
    QString m_command;
};

#endif // QTDRAWBRIDGE_H
