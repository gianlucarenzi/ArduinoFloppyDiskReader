#ifndef MIKMODPLAYER_H
#define MIKMODPLAYER_H

#include <QThread>
#include <QString>
#include <QVector>
#include <QTimer>

#ifndef Q_MOC_RUN
#include <mikmod.h>
#endif

class MikModPlayer : public QThread
{
    Q_OBJECT

public:
    explicit MikModPlayer(QObject *parent = nullptr, int updateTimer = 20, int pollTimer = 50);
    ~MikModPlayer();

    void loadModule(const QString &fileName);
    void stopPlayback();
    void pausePlayback();
    bool isPlaying() const;
    QVector<float> getCurrentLevels() const;
    int getNumChannels() const;

signals:
    void songFinished();

    void requestStartUpdateTimer();
    void requestStopUpdateTimer();
    void requestTogglePause();
    void requestStopPlayback();

protected:
    void run() override;

private:
    MODULE *module = nullptr;
    bool keepPlaying = false;
    bool libLoaded = false;
    bool m_paused = false;

    QTimer *m_updateTimer = nullptr;

    bool initLibrary();

private slots:
    void updateMikMod();
    void m_startUpdateTimer();
    void m_stopUpdateTimer();
    void m_togglePause();
    void m_stopPlayback();

#ifdef Q_OS_WIN
#ifndef Q_MOC_RUN
    typedef void   (*v_v)(void);
    typedef int    (*i_p)(char*);
    typedef int    (*i_v)(void);
    typedef MODULE*(*m_p)(char*, int, int);
    typedef void   (*v_m)(MODULE*);
    typedef void   (*v_m_v)(MODULE*);
    typedef void   (*v_i)(int);
    typedef int    (*i_uw_vp)(UWORD, VOICEINFO*);
    typedef ULONG  (*ul_sb)(SBYTE);
    typedef SBYTE  (*sb_ub)(UBYTE);

    // Puntatori statici per evitare conflitti multipli
    static v_v     p_MikMod_RegisterAllDrivers;
    static v_v     p_MikMod_RegisterAllLoaders;
    static i_p     p_MikMod_Init;
    static v_v     p_MikMod_Exit;

    static m_p     p_Player_Load;
    static v_m_v   p_Player_Start;
    static v_v     p_Player_Stop;
    static i_v     p_Player_Active;
    static v_m     p_Player_Free;
    static v_v     p_MikMod_Update;

    static v_i     p_Player_SetVolume;
    static v_v     p_Player_TogglePause;
    static i_uw_vp p_Player_QueryVoices;
    static ul_sb   p_Voice_RealVolume;
    static sb_ub   p_Player_GetChannelVoice;

    static uint *p_md_mode;
    static uint *p_md_mixfreq;
    static uint *p_md_devicebuffer;
    static int  *p_Player_Volume;
#endif
#endif
};

#endif // MIKMODPLAYER_H
