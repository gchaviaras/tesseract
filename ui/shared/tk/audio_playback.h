#pragma once

// Per-platform audio output abstraction for MatrixRTC call audio.
// Accepts decoded S16LE PCM frames and routes them to the system speaker.
// Thread-safe: push_frame() may be called from any thread. See the note on
// push_frame() for what "thread-safe" does and does not cover.

#include <cstddef>
#include <cstdint>
#include <memory>

namespace tk
{

class AudioPlayback
{
public:
    virtual ~AudioPlayback() = default;

    // Feed decoded S16LE PCM samples to the output device.
    //
    // Called on a worker thread, not the UI thread: EventHandlerBase hands the
    // decoded frames straight through rather than marshalling them. Backends
    // must therefore treat this as a pure data sink and confine any device or
    // QObject lifetime change it triggers (a format switch, say) to the thread
    // that owns them.
    virtual void push_frame(const std::int16_t* samples,
                            std::size_t         sample_count,
                            std::uint32_t       sample_rate,
                            std::uint32_t       num_channels) = 0;
};

// Factory declarations — each defined in audio_playback_<platform>.cpp.
std::unique_ptr<AudioPlayback> make_audio_playback_qt();
std::unique_ptr<AudioPlayback> make_audio_playback_gtk();
std::unique_ptr<AudioPlayback> make_audio_playback_win32();
std::unique_ptr<AudioPlayback> make_audio_playback_macos();

} // namespace tk
