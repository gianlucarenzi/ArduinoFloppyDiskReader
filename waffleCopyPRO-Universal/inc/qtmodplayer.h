#ifndef QTMODPLAYER_H
#define QTMODPLAYER_H

#include <QThread>
#include <QString>
#include <atomic>
#include <memory>

#include <libopenmpt/libopenmpt.hpp>
#include <portaudiocpp/PortAudioCpp.hxx>

class QtModPlayer : public QThread
{
    Q_OBJECT
public:
    QtModPlayer();
    ~QtModPlayer();

    void setup(const QString &filename);
    void stop();

protected:
    void run() override;

signals:
    void modPlayerSignal(int rval);

private:
    QString m_filename;
    std::unique_ptr<openmpt::module> m_mod;
    std::atomic<bool> m_playing;
};

#endif // QTMODPLAYER_H