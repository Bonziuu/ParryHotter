// Development diagnostics: a frame-accurate text log of the casting chain,
// written next to the game executable (Gothic/System/ParryHotter_cast.log).
// The file is truncated on the first line of each game session and capped,
// so it can never grow without bound. Pure C++17 (portable tests).
#pragma once
#ifdef _MSC_VER
#include <share.h>
#endif
#include <chrono>
#include <cstdarg>
#include <cstdio>

namespace ParryHotter {

class CastLog final {
public:
    static constexpr long kMaxBytes = 2 * 1024 * 1024;

    explicit CastLog(const char* path = "ParryHotter_cast.log") : path_(path) {}
    CastLog(const CastLog&) = delete;
    CastLog& operator=(const CastLog&) = delete;
    ~CastLog() { Close(); }

    void Line(const char* format, ...) {
        if (!Open() || written_ >= kMaxBytes) return;
        char message[480];
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_).count();
        const int n = std::fprintf(file_, "%9.3f %s\n", seconds, message);
        if (n > 0) written_ += n;
        if (written_ >= kMaxBytes)
            std::fputs("--- log limit reached ---\n", file_);
        std::fflush(file_); // survive a crash: every line is on disk
    }

    long Written() const { return written_; }

    void Close() {
        if (file_) std::fclose(file_);
        file_ = nullptr;
    }

private:
    bool Open() {
        if (file_) return true;
        if (opened_) return false; // never reopen (and truncate) mid-session
        opened_ = true;
#if defined(_MSC_VER)
        // fopen_s opens the file exclusively, so it could not be read while
        // the game ran. _fsopen with _SH_DENYWR keeps it readable.
        file_ = _fsopen(path_, "w", _SH_DENYWR);
#else
        file_ = std::fopen(path_, "w");
#endif
        start_ = std::chrono::steady_clock::now();
        return file_ != nullptr;
    }

    const char* path_;
    std::FILE* file_ = nullptr;
    bool opened_ = false;
    long written_ = 0;
    std::chrono::steady_clock::time_point start_{};
};

} // namespace ParryHotter
