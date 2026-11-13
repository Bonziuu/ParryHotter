#include "../hud/CastLog.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

static std::string ReadAll(const char* path) {
    std::string text;
    if (std::FILE* f = std::fopen(path, "rb")) {
        char buffer[4096];
        size_t n;
        while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) text.append(buffer, n);
        std::fclose(f);
    }
    return text;
}

int main() {
    const char* path = "ph_cast_log_test.txt";
    if (std::FILE* f = std::fopen(path, "wb")) { std::fputs("stale previous session\n", f); std::fclose(f); }
    {
        ParryHotter::CastLog log(path);
        log.Line("lmb down slot=%d", 1);
        log.Line("cast result=%d", 0);
        const std::string text = ReadAll(path); // flushed per line
        assert(text.find("stale") == std::string::npos); // new session truncates
        assert(text.find("lmb down slot=1") != std::string::npos);
        assert(text.find("cast result=0") != std::string::npos);
        const std::string snapshot(650, 'x');
        log.Line("controls %s dropped=7", snapshot.c_str());
        assert(ReadAll(path).find("dropped=7") != std::string::npos);
    }
    {
        ParryHotter::CastLog log(path);
        std::string big(400, 'x');
        for (int i = 0; i < 20000; ++i) log.Line("%s", big.c_str());
        assert(log.Written() >= ParryHotter::CastLog::kMaxBytes);
        assert(log.Written() < ParryHotter::CastLog::kMaxBytes + 1024);
    }
    const std::string capped = ReadAll(path);
    assert(capped.find("log limit reached") != std::string::npos);
    std::remove(path);
    std::cout << "Parry Hotter cast log truncation, flush and size cap: OK\n";
}
