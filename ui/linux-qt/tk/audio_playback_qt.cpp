// Qt6 audio output backend for tk::AudioPlayback.
// Uses QAudioSink (Qt Multimedia) to play 48kHz/16-bit S16LE PCM received
// from remote MatrixRTC participants.  The remote stream is always mono
// (NativeAudioStream asks LiveKit for 1 channel), but the sink is opened on the
// *device's* channel layout and the mono PCM is upmixed into it here: driving a
// stereo output with a mono stream only fills the left channel, so remote call
// audio came out on one ear on headphones.  Frames arrive on a worker thread at
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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

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
        // open_sink_() picks the real channel count from the output device;
        // this is only a starting value for the first format query.
        fmt_.setChannelCount(2);
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

        // Re-open with the new sample rate if it changes mid-call. The sink is
        // torn down and rebuilt on the UI thread, never here. The channel count
        // is *not* taken from the stream: it is the device's layout, and the
        // stream is converted onto it below.
        if (static_cast<int>(sample_rate) != fmt_.sampleRate())
        {
            queue_reopen_(sample_rate);
            return;
        }

        if (!sink_ || !io_)
            return;

        // sample_count is the total number of interleaved samples, so the
        // frame count is that divided by the stream's channel count.
        const std::size_t in_ch =
            num_channels > 0 ? static_cast<std::size_t>(num_channels) : 1;
        const std::size_t out_ch =
            static_cast<std::size_t>(fmt_.channelCount());
        if (out_ch == 0)
            return;
        const std::size_t frames = sample_count / in_ch;
        if (frames == 0)
            return;

        if (in_ch == out_ch)
        {
            io_->write(reinterpret_cast<const char*>(samples),
                       static_cast<qsizetype>(frames * out_ch
                                              * sizeof(std::int16_t)));
            return;
        }

        // Convert onto the sink's layout. This is the mono -> stereo case in
        // practice (remote audio is always mono), and duplicating the sample
        // into both channels is what puts the voice back in the middle.
        mix_.resize(frames * out_ch);
        for (std::size_t f = 0; f < frames; ++f)
        {
            const std::int16_t* in = samples + (f * in_ch);
            std::int16_t*       out = mix_.data() + (f * out_ch);
            if (in_ch > out_ch)
            {
                // Downmix: average the input channels folded into each output.
                for (std::size_t c = 0; c < out_ch; ++c)
                {
                    const std::size_t first = (c * in_ch) / out_ch;
                    const std::size_t last  = ((c + 1) * in_ch) / out_ch;
                    std::int32_t acc = 0;
                    for (std::size_t k = first; k < last; ++k)
                        acc += in[k];
                    out[c] = static_cast<std::int16_t>(
                        acc / static_cast<std::int32_t>(last - first));
                }
            }
            else
            {
                // Upmix: every output channel takes its nearest input channel,
                // so a single input channel lands in all of them.
                for (std::size_t c = 0; c < out_ch; ++c)
                    out[c] = in[c % in_ch];
            }
        }

        io_->write(reinterpret_cast<const char*>(mix_.data()),
                   static_cast<qsizetype>(mix_.size() * sizeof(std::int16_t)));
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

        // Open on the device's own channel layout, not on the incoming
        // stream's. A stereo output driven with a mono stream only fills the
        // left channel, which is why remote call audio was only audible on the
        // left ear. Prefer stereo, then whatever the device prefers, then
        // mono for devices that only accept the stream layout, rather than
        // going silent.
        const int candidates[] = {
            2, std::max(1, dev.preferredFormat().channelCount()), 1 };
        for (const int ch : candidates)
        {
            fmt_.setChannelCount(ch);
            if (dev.isFormatSupported(fmt_))
                break;
        }

        if (dev.isFormatSupported(fmt_))
        {
            sink_ = std::make_unique<QAudioSink>(dev, fmt_);
            sink_->setBufferSize(buffer_bytes_());
            io_ = sink_->start();
        }

        reopen_pending_ = false;
    }

    // Records the new sample rate and queues a re-creation onto the UI thread.
    // Called with mu_ held; coalesces, so a rate change at 100 Hz queues one
    // rebuild rather than one per frame.
    void queue_reopen_(std::uint32_t sample_rate)
    {
        reopen_pending_ = true;
        fmt_.setSampleRate(static_cast<int>(sample_rate));

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
    // Guards sink_/io_/fmt_/mix_ against the UI thread rebuilding the sink
    // while a worker thread is writing a frame into it.
    std::mutex                      mu_;
    // Channel-conversion scratch, kept as a member so the audio path does not
    // allocate once the stream is steady.
    std::vector<std::int16_t>       mix_;
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
