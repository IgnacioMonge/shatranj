// Stand-in UCI engine for the desktop tests, so no Stockfish download is
// involved. It identifies itself as a Stockfish and answers each search, after
// SHATRANJ_FAKE_UCI_DELAY_MS, with the next move of SHATRANJ_FAKE_UCI_MOVES
// (comma separated; the last one repeats; default e2e4). It can also die on
// "go" to exercise the client's engine-loss handling:
//   SHATRANJ_FAKE_UCI_CRASH_FILE    exit on the first "go" when this file is
//                                   missing; the file is created first, so the
//                                   next process answers normally
//   SHATRANJ_FAKE_UCI_CRASH_ALWAYS  exit on every "go"
//   SHATRANJ_FAKE_UCI_START_LOG     append one line per process start
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

const char *envOrNull(const char *name)
{
    const char *value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? value : nullptr;
}

bool fileExists(const char *path)
{
    return std::ifstream(path).good();
}

void appendLine(const char *path, const char *line)
{
    std::ofstream(path, std::ios::app) << line << '\n';
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (const char *log = envOrNull("SHATRANJ_FAKE_UCI_START_LOG")) {
        appendLine(log, "start");
    }
    const char *delay = envOrNull("SHATRANJ_FAKE_UCI_DELAY_MS");
    const int delayMs = delay != nullptr ? std::atoi(delay) : 0;
    const char *crashFile = envOrNull("SHATRANJ_FAKE_UCI_CRASH_FILE");
    const bool crashAlways = envOrNull("SHATRANJ_FAKE_UCI_CRASH_ALWAYS") != nullptr;
    std::vector<std::string> moves;
    {
        const char *list = envOrNull("SHATRANJ_FAKE_UCI_MOVES");
        std::string text = list != nullptr ? list : "e2e4";
        std::string::size_type start = 0;
        while (start <= text.size()) {
            const std::string::size_type comma = text.find(',', start);
            const std::string::size_type end = comma == std::string::npos ? text.size() : comma;
            if (end > start) {
                moves.push_back(text.substr(start, end - start));
            }
            if (comma == std::string::npos) {
                break;
            }
            start = comma + 1;
        }
        if (moves.empty()) {
            moves.push_back("e2e4");
        }
    }
    std::size_t searches = 0;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "uci") {
            std::puts("id name Stockfish Fake");
            std::puts("id author NetChessZX tests");
            std::puts("uciok");
        } else if (line == "isready") {
            std::puts("readyok");
        } else if (line.compare(0, 2, "go") == 0) {
            if (crashAlways || (crashFile != nullptr && !fileExists(crashFile))) {
                if (crashFile != nullptr) {
                    appendLine(crashFile, "crashed");
                }
                return 3;
            }
            if (delayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
            const std::string &move = moves[std::min(searches, moves.size() - 1)];
            ++searches;
            std::printf("bestmove %s\n", move.c_str());
        } else if (line == "quit") {
            return 0;
        }
    }
    return 0;
}
