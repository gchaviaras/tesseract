#pragma once
/// A dedicated single-thread writer that offloads session persistence off the
/// matrix-sdk sync/worker thread.
///
/// Why this exists: matrix-sdk's save_session_callback (reached via the
/// persist_session FFI) runs synchronously on a runtime worker thread. The
/// underlying SessionStore::save_account does a platform credential-store write
/// (Keychain / libsecret / Windows Credential Manager — any of which can be
/// slow or even *prompt* the user) plus an fsync'd atomic file write. Doing
/// that work inline can stall Matrix sync. This queue lets persist_session
/// enqueue the job and return promptly.
///
/// Correctness: multiple refreshes for the SAME user_id may arrive in quick
/// succession; only the newest token must survive. The queue therefore
/// coalesces per user_id (last-write-wins): a pending job for a user_id has its
/// json replaced rather than appended, which also bounds the queue under rapid
/// token rotation. Distinct user_ids are independent and preserve arrival order
/// among themselves.

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace tesseract
{

class SessionPersistQueue
{
public:
    /// The function that actually performs the (blocking) persistence for one
    /// job. Pulled out as a parameter purely so the coalescing/ordering logic
    /// is unit-testable without touching the real credential store / disk.
    using Writer = void (*)(const std::string& user_id,
                            const std::string& json);

    explicit SessionPersistQueue(Writer writer);
    ~SessionPersistQueue();

    SessionPersistQueue(const SessionPersistQueue&) = delete;
    SessionPersistQueue& operator=(const SessionPersistQueue&) = delete;

    /// Enqueue (or coalesce) a persistence job and return immediately. If a job
    /// for `user_id` is already pending, its json is replaced (last-write-wins);
    /// otherwise a new job is appended. Never blocks on I/O.
    void enqueue(std::string user_id, std::string json);

    /// Block until every currently-pending job has been written. Used by
    /// SessionStore::flush_session_updates() for the synchronous
    /// (refresh / shutdown) path, and by tests.
    void drain();

    /// Drop every pending job for `user_id` and, if one is being written
    /// right now, wait for it to finish. Called before an account's
    /// credentials are cleared on logout, so a queued write can't land
    /// afterwards and recreate them.
    void discard(const std::string& user_id);

    /// Number of jobs waiting to be picked up (excludes one in flight).
    /// Test seam.
    std::size_t pending();

private:
    struct Job
    {
        std::string user_id;
        std::string json;
    };

    void run();

    Writer writer_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> queue_;       // distinct user_ids, FIFO among themselves
    bool busy_ = false;           // a job is currently being written
    std::string busy_user_;       // user_id of that job, when busy_
    bool stop_ = false;
    std::thread thread_;
};

/// Process-wide singleton used by persist_session(). Constructed on first use
/// and intentionally leaked (function-local static with a leaked thread) to
/// sidestep static-destruction-order hazards: the matrix-sdk runtime may still
/// be firing save callbacks while C++ statics are being torn down at exit, and
/// joining the writer thread there would deadlock or use freed state. Draining
/// is best-effort at exit only.
SessionPersistQueue& session_persist_queue();

} // namespace tesseract
