#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/persistence/sqlite_connection.hpp"

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace aedst::persistence {

/// Runs `callable` on some thread of the caller's choice (the GUI thread in the app: a queued
/// QMetaObject::invokeMethod). Used to deliver completions away from the database thread.
using Dispatcher = std::function<void(std::function<void()>)>;

/// The one thread that touches SQLite.
///
/// The connection is created, migrated, used and closed on the worker thread only; callers submit work and
/// receive the result through a Dispatcher, so neither opening the file, applying migrations, probing the OS
/// keyring for the master key nor reading or writing a row ever blocks the GUI thread. (SQLite connections
/// are used here single-threaded on purpose: no cross-thread sharing, no FULLMUTEX contention.)
///
///  - Tasks run in submission order, one at a time.
///  - If the file cannot be opened or migrated (including "database is newer than this build"), every task
///    completes with that error instead of running; the process keeps working without local storage.
///  - Destruction finishes the queued tasks, closes the connection on the worker thread and joins it.
class DatabaseWorker final {
public:
    /// Work executed with the open connection; a failed open never reaches it.
    template <typename T>
    using Work = std::function<core::Outcome<T>(const std::shared_ptr<SqliteConnection>&)>;

    /// Starts the thread and returns immediately; opening and migrating happen on the worker.
    explicit DatabaseWorker(std::filesystem::path file);
    ~DatabaseWorker();

    DatabaseWorker(const DatabaseWorker&) = delete;
    DatabaseWorker& operator=(const DatabaseWorker&) = delete;
    DatabaseWorker(DatabaseWorker&&) = delete;
    DatabaseWorker& operator=(DatabaseWorker&&) = delete;

    /// Executes `work` on the worker thread and delivers its result through `dispatcher`.
    template <typename T>
    void run(Work<T> work, Dispatcher dispatcher, core::Completion<T> done) {
        enqueue(
            [work = std::move(work), dispatcher, done](const std::shared_ptr<SqliteConnection>& connection) {
                dispatcher([done, result = work(connection)]() mutable { done(std::move(result)); });
            },
            [dispatcher, done](core::Error error) {
                dispatcher([done, error = std::move(error)]() mutable { done(core::fail(std::move(error))); });
            });
    }

    /// The thread all database work runs on (for tests and assertions).
    [[nodiscard]] std::thread::id threadId() const noexcept { return threadId_; }

private:
    using Job = std::function<void(const std::shared_ptr<SqliteConnection>&)>;
    using Unavailable = std::function<void(core::Error)>;

    void enqueue(Job job, Unavailable unavailable);
    void loop();

    struct Item {
        Job job;
        Unavailable unavailable;
    };

    std::filesystem::path file_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Item> queue_;
    bool closing_{false};
    std::thread::id threadId_;
    // Touched on the worker thread only:
    std::shared_ptr<SqliteConnection> connection_;
    std::optional<core::Error> openError_;
    std::thread thread_;
};

} // namespace aedst::persistence
