#ifndef DIAGNOSTICTHREAD_H
#define DIAGNOSTICTHREAD_H

#include <QThread>
#include <QString>

class DiagnosticThread : public QThread
{
    Q_OBJECT
public:
    DiagnosticThread();
    virtual void run();
    void setup(QString port);

signals:
    void diagnosticMessage(QString message);
    void diagnosticComplete(bool success);

private:
    QString m_port;
};

#endif // DIAGNOSTICTHREAD_H
