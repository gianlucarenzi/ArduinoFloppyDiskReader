#ifndef QTMODPLAYER_H
#define QTMODPLAYER_H

#include <QThread>
#include <QString>
#include <QDebug>

class QtModPlayer : public QThread
{
    Q_OBJECT
public:
    QtModPlayer() {
        qDebug() << __PRETTY_FUNCTION__ << "Called";
    }
    virtual void run() override {
        qDebug() << __PRETTY_FUNCTION__ << "Called";
        // Placeholder for now
        emit modPlayerSignal(0);
    }
    void setup(const QString &filename) {
        qDebug() << __PRETTY_FUNCTION__ << "Called with" << filename;
        m_filename = filename;
    }

signals:
    void modPlayerSignal(int rval);

private:
    QString m_filename;
};

#endif // QTMODPLAYER_H
