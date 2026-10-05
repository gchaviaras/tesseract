// Qt6 audio output backend for tk::AudioPlayback.
// Uses QAudioSink (Qt Multimedia) to play 48kHz/16-bit/mono S16LE PCM received
// from remote MatrixRTC participants.  Frames arrive on a worker thread at
// ~100 Hz (10 ms / 480 samples at 48kHz).
//
// A QAudioSink is a QObject, and Qt Multimedia marshals the endpoint's state to
// the thread that owns it using *posted* events.  Creating, destroying or
// re-creating a sink from any thread but the one running the event loop can
// therefore deliver a posted event to an endpoint that has already gone away:
// the event lands on a null `this` and the process dies in
// QPlatformAudioEndpointBase::updateStreamIdle.  Re-creating the sink used to
// happen right here in push_frame(), off the UI thread, whenever the incoming
// stream parameters changed - which is what a capture restart (for instance
// changing the camera in Settings) sets off, so the app crashed with a SIGSEGV
// a few seconds after every camera change.
//
// So PCM is written from the calling thread under mu_, but the *sink's*
// lifetime belongs to the UI thread: a format change is recorded here and the
// re-creation is queued onto the UI thread.  Frames that arrive before the new
// sink is live are dropped instead of being played on the old format.

#include "audio_playback.h"

#include <tesseract/settings.h>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QIODevice>
#include <QMediaDevices>
#include <QMetaObject>
#include <QObject>
#include <QThread>
#include <QtGlobal>

#include <cstring>
#include <memory>
#include <mutex>

namespace
{

// Returns the preferred audio output device. Falls back to the system default
// if the user preference is empty or the named device is not found.
QAudioDevice preferred_audio_output()
{
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    const std::string& pref =
        tesseract::Settings::instance().audio_output_device_id;
    if (!pref.empty())
    {
        const QByteArray want = QByteArray::fromStdString(pref);
        for (const QAudioDevice& d : QMediaDevices::audioOutputs())
        {
            if (d.id() == want) { dev = d; break; }
        }
    }
    return dev;
}

class AudioPlaybackQt : public tk::AudioPlayback
{
public:
    AudioPlaybackQt()
    {
        fmt_.setSampleRate(48000);
        fmt_.setChannelCount(1);
        fmt_.setSampleFormat(QAudioFormat::Int16);

        // make_audio_playback_qt() is called from start_call() on the UI
        // thread, so `this` is owned by it. ui_proxy_ is a QObject with no
        // behaviour of its own: it exists purely as the context object for the
        // queued sink (re)creation, so that call lands on the UI thread too.
        ui_thread_  = QThread::currentThread();
        ui_proxy_.reset(new QObject());

        open_sink_();
    }

    ~AudioPlaybackQt() override
    {
        // Constructed on the UI thread, and so must be destroyed there: the
        // sink's destruction tears down a QObject that posts events to itself.
        // The one caller (ShellBase's call teardown) is a UI-thread path.
        if (QThread::currentThread() != ui_thread_)
            qWarning("tesseract audio playback: destroyed off the UI thread; "
                     "the QAudioSink teardown is not safe from here");

        // Release the proxy *before* the sink. ~QObject drops the events still
        // posted to it, so a queued open_sink_() can never run against a
        // half-destroyed object.
        ui_proxy_.reset();
    }

    void push_frame(const std::int16_t* samples,
                    std::size_t         sample_count,
                    std::uint32_t       sample_rate,
                    std::uint32_t       num_channels) override
    {
        std::lock_guard<std::mutex> lk(mu_);

        // A re-creation is already queued: the live sink still has the old
        // format, so this frame does not belong on it.
        if (reopen_pending_)
            return;

        // Re-open with the new format if the stream parameters change mid-call.
        // The sink is torn down and rebuilt on the UI thread, never here.
        if (static_cast<int>(sample_rate) != fmt_.sampleRate()
            || static_cast<int>(num_channels) != fmt_.channelCount())
        {
            queue_reopen_(sample_rate, num_channels);
            return;
        }

        if (!sink_ || !io_)
            return;

        const auto* bytes   = reinterpret_cast<const char*>(samples);
        const qsizetype len = static_cast<qsizetype>(
            sample_count * sizeof(std::int16_t));
        io_->write(bytes, len);
    }

private:
    // 200 ms, to absorb jitter without underruns.
    qsizetype buffer_bytes_() const
    {
        return static_cast<qsizetype>(fmt_.sampleRate()
                                     * fmt_.channelCount()
                                     * sizeof(std::int16_t) / 5);
    }

    // UI thread only. Replaces the sink at the current fmt_.
    void open_sink_()
    {
        std::lock_guard<std::mutex> lk(mu_);

        sink_.reset();
        io_ = nullptr;

        const QAudioDevice dev = preferred_audio_output();
        if (dev.isFormatSupported(fmt_))
        {
            sink_ = std::make_unique<QAudioSink>(dev, fmt_);
            sink_->setBufferSize(buffer_bytes_());
            io_ = sink_->start();
        }

        reopen_pending_ = false;
    }

    // Records the new format and queues a re-creation onto the UI thread.
    // Called with mu_ held; coalesces, so a parameter change at 100 Hz queues
    // one rebuild rather than one per frame.
    void queue_reopen_(std::uint32_t sample_rate, std::uint32_t num_channels)
    {
        reopen_pending_ = true;
        fmt_.setSampleRate(static_cast<int>(sample_rate));
        fmt_.setChannelCount(static_cast<int>(num_channels));

        if (ui_proxy_)
        {
            QMetaObject::invokeMethod(ui_proxy_.get(), [this] { open_sink_(); },
                                      Qt::QueuedConnection);
        }
    }

    QAudioFormat                    fmt_;
    std::unique_ptr<QAudioSink>     sink_;
    std::unique_ptr<QObject>        ui_proxy_;
    QIODevice*                      io_ = nullptr; // owned by sink_
    QThread*                        ui_thread_ = nullptr;
    bool                            reopen_pending_ = false;
    // Guards sink_/io_/fmt_ against the UI thread rebuilding the sink while a
    // worker thread is writing a frame into it.
    std::mutex                      mu_;
};

} // namespace

namespace tk::qt6
{

std::unique_ptr<tk::AudioPlayback> make_audio_playback_qt()
{
    if (QMediaDevices::audioOutputs().isEmpty())
        return nullptr;
    return std::make_unique<AudioPlaybackQt>();
}

} // namespace tk::qt6
