#include "qtmodplayer.h"
#include <QDebug>
#include <fstream>
#include <stdexcept>
#include <vector>

QtModPlayer::QtModPlayer(void) : m_playing(false)
{
    qDebug() << __PRETTY_FUNCTION__ << "Called";
}

QtModPlayer::~QtModPlayer()
{
    stop();
    wait();
}

void QtModPlayer::setup(const QString &filename)
{
    qDebug() << __PRETTY_FUNCTION__ << "Called with" << filename;
    m_filename = filename;
    try {
        std::ifstream file(m_filename.toStdString(), std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file.");
        }
        m_mod = std::make_unique<openmpt::module>(file);
    } catch (const std::exception &e) {
        qWarning() << "Error setting up mod player:" << e.what();
        m_mod.reset();
    }
}

void QtModPlayer::stop()
{
    m_playing = false;
    emit playbackStopped();
}

void QtModPlayer::run()
{
    if (!m_mod) {
        qWarning() << "Module not set up. Call setup() first.";
        emit modPlayerSignal(1);
        return;
    }

    try {
        constexpr std::size_t buffersize = 480;
        constexpr std::int32_t samplerate = 48000;

        portaudio::AutoSystem portaudio_initializer;
        portaudio::System &portaudio = portaudio::System::instance();

        std::vector<float> left(buffersize);
        std::vector<float> right(buffersize);
        std::vector<float> interleaved_buffer(buffersize * 2);
        bool is_interleaved = false;

        portaudio::BlockingStream stream = [&]() {
            try {
                is_interleaved = false;
                portaudio::DirectionSpecificStreamParameters outputstream_parameters(portaudio.defaultOutputDevice(), 2, portaudio::FLOAT32, false, portaudio.defaultOutputDevice().defaultHighOutputLatency(), 0);
                portaudio::StreamParameters stream_parameters(portaudio::DirectionSpecificStreamParameters::null(), outputstream_parameters, samplerate, paFramesPerBufferUnspecified, paNoFlag);
                return portaudio::BlockingStream(stream_parameters);
            } catch (const portaudio::PaException &e) {
                if (e.paError() != paSampleFormatNotSupported) {
                    throw;
                }
                is_interleaved = true;
                portaudio::DirectionSpecificStreamParameters outputstream_parameters(portaudio.defaultOutputDevice(), 2, portaudio::FLOAT32, true, portaudio.defaultOutputDevice().defaultHighOutputLatency(), 0);
                portaudio::StreamParameters stream_parameters(portaudio::DirectionSpecificStreamParameters::null(), outputstream_parameters, samplerate, paFramesPerBufferUnspecified, paNoFlag);
                return portaudio::BlockingStream(stream_parameters);
            }
        }();

        stream.start();
        m_playing = true;

        while (m_playing) {
            std::size_t count = is_interleaved ? m_mod->read_interleaved_stereo(samplerate, buffersize, interleaved_buffer.data()) : m_mod->read(samplerate, buffersize, left.data(), right.data());
            if (count == 0) {
                // End of module, loop back to beginning
                m_mod->set_position_seconds(0.0);
                continue; // Continue the loop to read from the beginning
            }
            try {
                if (is_interleaved) {
                    stream.write(interleaved_buffer.data(), static_cast<unsigned long>(count));
                } else {
                    const float * const buffers[2] = { left.data(), right.data() };
                    stream.write(buffers, static_cast<unsigned long>(count));
                }

                QVector<float> vu_levels;
                int num_channels = m_mod->get_num_channels();
                vu_levels.reserve(num_channels);
                for (int i = 0; i < num_channels; ++i) {
                    vu_levels.append(m_mod->get_current_channel_vu_mono(i));
                }
                emit vuData(vu_levels);

            } catch (const portaudio::PaException &pa_exception) {
                if (pa_exception.paError() != paOutputUnderflowed) {
                    throw;
                }
            }
        }

        stream.stop();

        // Emit zero VU levels after stopping playback
        QVector<float> zero_vu_levels;
        if (m_mod) { // Ensure m_mod is still valid
            zero_vu_levels.resize(m_mod->get_num_channels());
            std::fill(zero_vu_levels.begin(), zero_vu_levels.end(), 0.0f);
        } else {
            zero_vu_levels.resize(2); // Default to 2 channels if mod is invalid
            std::fill(zero_vu_levels.begin(), zero_vu_levels.end(), 0.0f);
        }
        emit vuData(zero_vu_levels);

    } catch (const std::exception &e) {
        qWarning() << "Error during mod playback:" << e.what();
        emit modPlayerSignal(1);
        return;
    }

    emit modPlayerSignal(0);
}
