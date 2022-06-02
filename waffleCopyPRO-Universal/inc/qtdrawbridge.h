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
    void setup(QString port, QString filename, QStringList command);

private:
    QString m_port;
    QString m_filename;
    QStringList m_command;
};

#endif // QTDRAWBRIDGE_H
