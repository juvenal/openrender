/**
 * tests/unit/texture_tile/texture_system_test_doubles.h
 *
 * Test doubles for the texture system tests (spec 027, T021): a directory-list resolver, a
 * resolver that counts and delays its locate() calls, and a diagnostic sink that counts
 * CODE_NOFILE reports. Header-only; includes only the texture system's own headers and common/.
 * No renderer, no frame, no RiBegin.
 */

#ifndef TEXTURE_SYSTEM_TEST_DOUBLES_H
#define TEXTURE_SYSTEM_TEST_DOUBLES_H

#include "ri/texture/textureResolver.h"
#include "ri/texture/textureSystem.h"

#include "common/diagnostics.h"
#include "common/os.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Finds a name in a list of directories (the test's search path).
class CDirectoryResolver : public CTextureFileResolver {
    public:
        explicit CDirectoryResolver(std::vector<std::string> dirs) : dirs_(std::move(dirs)) {}

        int locate(char *result, const char *name) const override {
            for (const std::string &dir : dirs_) {
                const std::string path = dir + "/" + name;
                if (osFileExists(path.c_str())) {
                    snprintf(result, OS_MAX_PATH_LENGTH, "%s", path.c_str());
                    return TRUE;
                }
            }
            return FALSE;
        }

    private:
        std::vector<std::string> dirs_;
};

// Wraps another resolver, counts the locate() calls per name, and sleeps 1 ms inside locate() to
// hold the first-load race window open (so a missing lock shows as more than one locate, not by luck).
class CCountingResolver : public CTextureFileResolver {
    public:
        explicit CCountingResolver(const CTextureFileResolver *inner) : inner_(inner) {}

        int locate(char *result, const char *name) const override {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                ++counts_[name];
                ++total_;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return inner_->locate(result, name);
        }

        int count(const char *name) const {
            std::lock_guard<std::mutex> guard(mutex_);
            auto it = counts_.find(name);
            return it == counts_.end() ? 0 : it->second;
        }
        int total() const { return total_.load(); }
        void reset() {
            std::lock_guard<std::mutex> guard(mutex_);
            counts_.clear();
            total_ = 0;
        }

    private:
        const CTextureFileResolver *inner_;
        mutable std::mutex mutex_;
        mutable std::map<std::string, int> counts_;
        mutable std::atomic<int> total_{0};
};

// Counts diagnostics (libRIBCommon's sink), in total and by code. Install with installDiagnosticSink()
// and restore the previous sink when done.
class CCountingDiagnostics : public CDiagnosticSink {
    public:
        void report(EErrorCode code, EDiagnosticSeverity, const char *message) override {
            ++total_;
            if (code == CODE_NOFILE)
                ++noFile_;
            std::lock_guard<std::mutex> guard(mutex_);
            lastMessage_ = message;
        }

        int total() const { return total_.load(); }
        int noFile() const { return noFile_.load(); }
        void reset() {
            total_ = 0;
            noFile_ = 0;
        }
        std::string lastMessage() const {
            std::lock_guard<std::mutex> guard(mutex_);
            return lastMessage_;
        }

    private:
        std::atomic<int> total_{0};
        std::atomic<int> noFile_{0};
        mutable std::mutex mutex_;
        std::string lastMessage_;
};

// Settings for a small test system over `resolver`: one thread unless asked otherwise.
inline CTextureSystemSettings testSystemSettings(const CTextureFileResolver *resolver, int numThreads = 1, TMutex *loadMutex = nullptr) {
    CTextureSystemSettings settings = {};
    settings.numThreads = numThreads;
    settings.maxMemory = 64 * 1024 * 1024;
    settings.resolver = resolver;
    settings.diskCacheEnabled = false;
    settings.diskCacheDirectory = nullptr;
    const matrix identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; i++)
        settings.toWorld[i] = identity[i];
    settings.loadMutex = loadMutex;
    return settings;
}

#endif
