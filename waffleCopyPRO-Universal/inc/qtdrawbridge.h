#ifndef QTDRAWBRIDGE_H
#define QTDRAWBRIDGE_H

#include <QThread>
#include <QString>
#include <QStringList>

class QtDrawBridge : public QThread
{
    Q_OBJECT
public:
    QtDrawBridge();
    virtual void run();
    void setup(QString port, QString filename, QStringList command);

signals:
    void QtDrawBridgeSignal(int rval);

private:
    QString m_port;
    QString m_filename;
    QStringList m_command;
};

#endif // QTDRAWBRIDGE_H
