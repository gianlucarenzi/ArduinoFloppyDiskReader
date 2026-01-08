#include "mikmodplayer.h"
#include <QDebug>
#include <QLibrary>
#include <QVector>
#include <algorithm>

#ifdef Q_OS_WIN
    #ifndef Q_MOC_RUN
        // Definitions of static pointers
        MikModPlayer::v_v     MikModPlayer::p_MikMod_RegisterAllDrivers = nullptr;
        MikModPlayer::v_v     MikModPlayer::p_MikMod_RegisterAllLoaders = nullptr;
        MikModPlayer::i_p     MikModPlayer::p_MikMod_Init = nullptr;
        MikModPlayer::v_v     MikModPlayer::p_MikMod_Exit = nullptr;

        MikModPlayer::m_p     MikModPlayer::p_Player_Load = nullptr;
        MikModPlayer::v_m_v   MikModPlayer::p_Player_Start = nullptr;
        MikModPlayer::v_v     MikModPlayer::p_Player_Stop = nullptr;
        MikModPlayer::i_v     MikModPlayer::p_Player_Active = nullptr;
        MikModPlayer::v_m     MikModPlayer::p_Player_Free = nullptr;
        MikModPlayer::v_v     MikModPlayer::p_MikMod_Update = nullptr;

        MikModPlayer::v_i     MikModPlayer::p_Player_SetVolume = nullptr;
        MikModPlayer::v_v     MikModPlayer::p_Player_TogglePause = nullptr;
        MikModPlayer::i_uw_vp MikModPlayer::p_Player_QueryVoices = nullptr;
        MikModPlayer::ul_sb   MikModPlayer::p_Voice_RealVolume = nullptr;
        MikModPlayer::sb_ub   MikModPlayer::p_Player_GetChannelVoice = nullptr;

        uint* MikModPlayer::p_md_mode = nullptr;
        uint* MikModPlayer::p_md_mixfreq = nullptr;
        uint* MikModPlayer::p_md_devicebuffer = nullptr;
        int*  MikModPlayer::p_Player_Volume = nullptr;
    #endif

#endif

MikModPlayer::MikModPlayer(QObject *parent, int updateTimer, int pollTimer)
: QThread(parent)
{
    Q_UNUSED(pollTimer);
    libLoaded = initLibrary();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setTimerType(Qt::PreciseTimer);
    connect(m_updateTimer, &QTimer::timeout, this, &MikModPlayer::updateMikMod);
    m_updateTimer->setInterval(updateTimer);

    qDebug() << __func__ << "Update (ms): " << m_updateTimer->interval();
    connect(this, &MikModPlayer::requestStartUpdateTimer, this, &MikModPlayer::m_startUpdateTimer);
    connect(this, &MikModPlayer::requestStopUpdateTimer, this, &MikModPlayer::m_stopUpdateTimer);
    connect(this, &MikModPlayer::requestTogglePause, this, &MikModPlayer::m_togglePause);
    connect(this, &MikModPlayer::requestStopPlayback, this, &MikModPlayer::m_stopPlayback);
}

bool MikModPlayer::initLibrary()
{
#ifdef Q_OS_WIN
    QLibrary lib;
    lib.setFileName("libmikmod.dll");
    if (!lib.load()) {
        qDebug() << "Warning: libmikmod.dll not found.";
        lib.setFileName("mikmod.dll");
        if (!lib.load()) {
            qCritical() << "Error: mikmod.dll not found.";
            return false;
        } else {
            qDebug() << "Alternate mikmod.dll OK";
        }
    }

    p_MikMod_RegisterAllDrivers = (v_v)lib.resolve("MikMod_RegisterAllDrivers");
    p_MikMod_RegisterAllLoaders = (v_v)lib.resolve("MikMod_RegisterAllLoaders");
    p_MikMod_Init = (i_p)lib.resolve("MikMod_Init");
    p_MikMod_Exit = (v_v)lib.resolve("MikMod_Exit");
    p_Player_Load = (m_p)lib.resolve("Player_Load");
    p_Player_Start = (v_m_v)lib.resolve("Player_Start");
    p_Player_Stop = (v_v)lib.resolve("Player_Stop");
    p_Player_Active = (i_v)lib.resolve("Player_Active");
    p_Player_Free = (v_m)lib.resolve("Player_Free");
    p_MikMod_Update = (v_v)lib.resolve("MikMod_Update");
    p_Player_SetVolume = (v_i)lib.resolve("Player_SetVolume");
    p_Player_TogglePause = (v_v)lib.resolve("Player_TogglePause");
    p_Player_QueryVoices = (i_uw_vp)lib.resolve("Player_QueryVoices");
    p_Voice_RealVolume = (ul_sb)lib.resolve("Voice_RealVolume");
    p_Player_GetChannelVoice = (sb_ub)lib.resolve("Player_GetChannelVoice");

    p_md_mode = (uint*)lib.resolve("md_mode");
    p_md_mixfreq = (uint*)lib.resolve("md_mixfreq");
    p_md_devicebuffer = (uint*)lib.resolve("md_devicebuffer");
    p_Player_Volume = (int*)lib.resolve("Player_Volume");

    if (!p_MikMod_Init || !p_md_mode || !p_Player_SetVolume || !p_Player_TogglePause ||
        !p_Player_QueryVoices || !p_Voice_RealVolume || !p_Player_GetChannelVoice)
        return false;

    *p_md_mode = DMODE_SOFT_MUSIC | DMODE_INTERP | DMODE_STEREO | DMODE_16BITS;
    *p_md_mixfreq = 44100;
    if (p_md_devicebuffer) *p_md_devicebuffer = 30;

    if (p_MikMod_RegisterAllDrivers) (*p_MikMod_RegisterAllDrivers)();
    if (p_MikMod_RegisterAllLoaders) (*p_MikMod_RegisterAllLoaders)();
    if (p_MikMod_Init((char*)"")) return false;

#else
    md_mode = DMODE_SOFT_MUSIC | DMODE_INTERP | DMODE_STEREO | DMODE_16BITS;
    md_mixfreq = 44100;
    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();
    if (MikMod_Init((char*)"")) return false;
#endif
    return true;
}

MikModPlayer::~MikModPlayer()
{
    m_stopPlayback();
    if (m_updateTimer) delete m_updateTimer;
    if (module) {
#ifdef Q_OS_WIN
        if (p_Player_Free) (*p_Player_Free)(module);
        if (p_MikMod_Exit) (*p_MikMod_Exit)();
#else
        Player_Free(module);
        MikMod_Exit();
#endif
    }
}

void MikModPlayer::loadModule(const QString &fileName)
{
    if (!libLoaded) return;
    if (module) stopPlayback();

    QByteArray ba = fileName.toLocal8Bit();
#ifdef Q_OS_WIN
    if (p_Player_Load) module = (*p_Player_Load)(ba.data(), 64, 0);
#else
    module = Player_Load(ba.data(), 64, 0);
#endif
}

void MikModPlayer::run()
{
    if (!module) return;
    keepPlaying = true;
    m_paused = false;

#ifdef Q_OS_WIN
    if (p_Player_Start) (*p_Player_Start)(module);
#else
    Player_Start(module);
#endif
    emit requestStartUpdateTimer();
    exec();
}

void MikModPlayer::updateMikMod()
{
    if (!keepPlaying) { m_stopUpdateTimer(); return; }

    if (!m_paused) {
#ifdef Q_OS_WIN
        if (p_Player_Active && p_Player_Active()) {
            if (p_MikMod_Update) (*p_MikMod_Update)();
        }
#else
        if (Player_Active()) {
            MikMod_Update();
        }
#endif
        else {
            keepPlaying = false;
            m_stopUpdateTimer();
            emit songFinished();
        }
    }
}

void MikModPlayer::stopPlayback() { emit requestStopPlayback(); }
void MikModPlayer::pausePlayback() { emit requestTogglePause(); }
bool MikModPlayer::isPlaying() const {
#ifdef Q_OS_WIN
    return keepPlaying && !m_paused && p_Player_Active && p_Player_Active();
#else
    return keepPlaying && !m_paused && Player_Active();
#endif
}

int MikModPlayer::getNumChannels() const
{
    if (module) {
        return module->numchn;
    }
    return 0;
}


QVector<float> MikModPlayer::getCurrentLevels() const
{
    if (!module || m_paused) { return QVector<float>(); }

    QVector<float> levels(module->numchn, 0.0f);
    float maxVolume = 65535.0f;

    for (UBYTE ch=0; ch<module->numchn; ++ch) {
        SBYTE voice = -1;
#ifdef Q_OS_WIN
        if (p_Player_GetChannelVoice) voice = (*p_Player_GetChannelVoice)(ch);
#else
        voice = Player_GetChannelVoice(ch);
#endif
        if (voice != -1) {
            ULONG vol = 0;
#ifdef Q_OS_WIN
            if (p_Voice_RealVolume) vol = (*p_Voice_RealVolume)(voice);
#else
            vol = Voice_RealVolume(voice);
#endif
            levels[ch] = static_cast<float>(vol)/maxVolume;
        }
    }
    return levels;
}

void MikModPlayer::m_startUpdateTimer() { if(m_updateTimer) m_updateTimer->start(); }
void MikModPlayer::m_stopUpdateTimer() { if(m_updateTimer) m_updateTimer->stop(); }

void MikModPlayer::m_togglePause()
{
    m_paused = !m_paused;
#ifdef Q_OS_WIN
    if (p_Player_TogglePause) (*p_Player_TogglePause)();
#else
    Player_TogglePause();
#endif
    if(m_paused) m_stopUpdateTimer();
    else m_startUpdateTimer();
}

void MikModPlayer::m_stopPlayback()
{
    keepPlaying = false;
    m_paused = false;
    m_stopUpdateTimer();
#ifdef Q_OS_WIN
    if (p_Player_Stop) (*p_Player_Stop)();
#else
    Player_Stop();
#endif
    quit();
    wait();
}
