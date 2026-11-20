/**
 * tests/shading_system/test_support.h
 *
 * Spec 033, Phase 2: what the ShadingSystem tests share (contracts/shading-system.md, shader-backend.md, shader-group.md).
 * Everything here names only the interfaces of the contracts: ShadingSystem, IShaderBackend, IShaderFileResolver and
 * IShaderBindHost. None of them exists on the starting tree, which is the red evidence of the tests that include this file
 * (a build failure naming the missing header).
 *
 *   CMapResolver     the resolver double: "name" + extension -> a path the test chose; records every request
 *   CRecordingHost   the bind host double: hands out keys 1, 2, 3 ..., records the lights it was asked to register
 *   the "tst" backend lives in test_backend.h
 *
 * The tests link libshader_shading, libRIBCommon and the State-only library: no renderer (SC-003).
 */
#ifndef TESTS_SHADING_SYSTEM_TEST_SUPPORT_H
#define TESTS_SHADING_SYSTEM_TEST_SUPPORT_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "case_common.h"       // CRecordingSink (tests/shading_contract)
#include "contract_harness.h"  // CHarness, contract_test.h (tests/shading_contract)
#include "libshader/shading/shaderBackend.h"
#include "libshader/shading/shadingSystem.h"

namespace sstest {

    using namespace openrender::shading;

    // The resolver double: files["name.ext"] = path. A name that holds an extension is looked up as written, like
    // CRenderer::locateFileEx does.
    class CMapResolver : public IShaderFileResolver {
        public:
            std::map<std::string, std::string> files;
            mutable std::vector<std::string> requests; // "name.ext", in the order asked
            mutable std::mutex lock;

            void add(const std::string &name, const std::string &extension, const std::string &path) { files[name + "." + extension] = path; }

            int locate(char *result, const char *name, const char *extension) const override {
                const std::string key = (strchr(name, '.') != nullptr) ? std::string(name) : std::string(name) + "." + extension;
                std::lock_guard<std::mutex> guard(lock);
                requests.push_back(key);
                auto it = files.find(key);
                if (it == files.end())
                    return FALSE;
                strcpy(result, it->second.c_str());
                return TRUE;
            }

            int asked(const std::string &key) const {
                std::lock_guard<std::mutex> guard(lock);
                int n = 0;
                for (const std::string &r : requests)
                    n += (r == key);
                return n;
            }
    };

    // The bind host double: keys 1, 2, 3 ... and the lights registered, with an optional shared event log
    class CRecordingHost : public IShaderBindHost {
        public:
            uint64_t next = 1;
            int keysTaken = 0;
            std::vector<CShaderInstance *> lights;
            std::vector<std::string> *events = nullptr;

            uint64_t takeInstanceKey() override {
                ++keysTaken;
                if (events != nullptr)
                    events->push_back("takeInstanceKey");
                return next++;
            }
            void registerLight(CShaderInstance *light) override {
                lights.push_back(light);
                if (events != nullptr)
                    events->push_back("registerLight");
            }
    };

    inline std::string env(const char *name) {
        const char *v = getenv(name);
        if (v == nullptr) {
            fprintf(stderr, "test_support: %s is not set\n", name);
            exit(2);
        }
        return v;
    }

    // The fixture directory of tests/shaderformat (fmtp_red.rslo, fmtp_green.slo)
    inline std::string fixtureDir() { return env("SHADING_SYSTEM_FIXTURES"); }

    inline std::string writeTemp(const std::string &name, const std::string &text) {
        const std::string path = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp") + "/033-" + name;
        std::ofstream f(path);
        f << text;
        return path;
    }

} // namespace sstest

#endif
