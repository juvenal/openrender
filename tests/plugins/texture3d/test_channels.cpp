/**
 * Project: openRender
 *
 * File: test_channels.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), US7 / FR-025: characterization of how a 3D texture
 *   (point cloud) parses its channel list, captured on the starting tree before Texturing
 *   gets a channel interface of its own (DB-021).
 *
 *   One mode per run, selected by argv[1]; the program prints a deterministic report to
 *   stdout and the Texture3dChannels_* tests compare it with a data file captured on the
 *   starting tree (texture3d/reference/<mode>.txt):
 *
 *     session    inside RiBegin/RiEnd, after DisplayChannel declared _area and Ci, a point cloud
 *                created from the channel list "_area,Ci" (what bake3d does): the channels' names, types and data size, three points written
 *                and read back through the point-cloud file with ptcapi's read functions
 *     nosession  no RiBegin: PtcCreatePointCloudFile with names and types (the ptcapi path, which
 *                parses with parseVariable only), read back; and the string form
 *                ("_area,Ci") with no session, which cannot resolve any channel name and reports
 *                the unknown-channel error for each
 *     unknown    inside a session, a channel list with a name the registry does not know: the error
 *                text and the channels that were kept
 *
 *   Fields not observable without new API: the channels' per-channel sample counts, start offsets
 *   and fill values (CTexture3d::CChannel is a protected nested class, CTexture3d::queryChannels
 *   returns only names and type strings). The report covers names, type strings, the total data
 *   size, the point count and the data of the written point; the fill values are not covered.
 *
 *   ptcapi.h declares PtcCreatePointCloudFile and PtcOpenPointCloudFile with `const char **`
 *   (create) and `char **` (open) arguments, while the library defines them with `char **` (create)
 *   and `const char **` (open): the C symbols the header names are not defined, only C++ overloads
 *   with those other signatures. This program declares the overloads that exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "ri/parse/ri.h"
#include "ri/texture/pointCloud.h"
#include "ri/texture/ptcapi.h"

// The overloads the library really defines (see the header comment).
PtcPointCloud PtcCreatePointCloudFile(char *filename, int nvars, char **vartypes, char **varnames, float *world2eye, float *world2ndc, float *format);
PtcPointCloud PtcOpenPointCloudFile(char *filename, int *nvars, const char **vartypes, const char **varnames);

namespace {

    class CRecordingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                std::string text(message);
                while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                    text.pop_back();
                lines.push_back("diagnostic code=" + std::to_string((int)code) + " severity=" + std::to_string((int)severity) + ": " + text);
            }
            std::vector<std::string> lines;
    };

    CRecordingSink g_sink;

    void flushDiagnostics(const char *where) {
        printf("%s: %zu diagnostic(s)\n", where, g_sink.lines.size());
        for (const std::string &line : g_sink.lines)
            printf("  %s\n", line.c_str());
        g_sink.lines.clear();
    }

    void identity(float *m) {
        for (int i = 0; i < 16; i++)
            m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    void describeChannels(const char *what, CPointCloud &pc) {
        const char *types[32];
        const char *names[32];
        int n = 0;
        pc.queryChannels(&n, types, names);
        printf("%s: %d channel(s), data size %d\n", what, n, pc.getDataSize());
        for (int i = 0; i < n; i++)
            printf("  channel %d: name=%s type=%s\n", i, names[i], types[i]);
    }

    void describeFile(const char *what, char *file, int expectedDataSize) {
        int nvars = 0;
        const char *types[32];
        const char *names[32];
        PtcPointCloud pc = PtcOpenPointCloudFile(file, &nvars, types, names);
        if (pc == nullptr) {
            printf("%s: cannot open %s\n", what, file);
            return;
        }
        int npoints = 0, datasize = 0;
        PtcGetPointCloudInfo(pc, "npoints", &npoints);
        PtcGetPointCloudInfo(pc, "datasize", &datasize);
        printf("%s: read back %d channel(s), %d point(s), data size %d (expected %d)\n", what, nvars, npoints, datasize, expectedDataSize);
        for (int i = 0; i < nvars; i++)
            printf("  channel %d: name=%s type=%s\n", i, names[i], types[i]);

        float point[3], normal[3], radius = 0, data[32] = {0};
        for (int n = 0; n < 8 && PtcReadDataPoint(pc, point, normal, &radius, data); n++) {
            printf("  point (%g %g %g) normal (%g %g %g) radius %g data", point[0], point[1], point[2], normal[0], normal[1], normal[2], radius);
            for (int i = 0; i < datasize; i++)
                printf(" %g", data[i]);
            printf("\n");
        }
        PtcClosePointCloudFile(pc);
    }


    // The channels the lists below name. rgb, rgba, a and z are predeclared; the rest are declared the
    // way a scene declares them (DisplayChannel), before the shaders that bake into them run.
    void declareChannels() {
        RiDisplayChannel("float _area", RI_NULL);
        RiDisplayChannel("color Ci", RI_NULL);
        flushDiagnostics("declare _area and Ci");
    }

    const float kPoint[3] = {0.1f, 0.2f, 0.3f};
    const float kNormal[3] = {0.0f, 0.0f, 1.0f};

    int runSession() {
        RiBegin(RI_NULL);
        CDiagnosticSink *previous = installDiagnosticSink(&g_sink);
        declareChannels();
        float I[16];
        identity(I);
        {
            CPointCloud pc("t3d_session.ptc", I, I, I, "_area,Ci", TRUE);
            describeChannels("session \"_area,Ci\"", pc);
            for (int n = 0; n < 3; n++) {
                const float data[4] = {0.5f + n, 1.0f, 0.25f, 0.125f};
                const float point[3] = {kPoint[0] + 0.5f * n, kPoint[1], kPoint[2]};
                pc.store(data, point, kNormal, 0.05f);
            }
        }
        flushDiagnostics("session");
        describeFile("session file", (char *)"t3d_session.ptc", 4);
        installDiagnosticSink(previous);
        RiEnd();
        return 0;
    }

    int runUnknown() {
        RiBegin(RI_NULL);
        CDiagnosticSink *previous = installDiagnosticSink(&g_sink);
        declareChannels();
        float I[16];
        identity(I);
        {
            CPointCloud pc("t3d_unknown.ptc", I, I, I, "_area,nosuchchannel,Ci", TRUE);
            describeChannels("unknown \"_area,nosuchchannel,Ci\"", pc);
        }
        flushDiagnostics("unknown");
        installDiagnosticSink(previous);
        RiEnd();
        return 0;
    }

    int runNoSession() {
        CDiagnosticSink *previous = installDiagnosticSink(&g_sink);
        float I[16];
        identity(I);

        // The ptcapi path: names and types, no session.
        {
            char *types[2] = {(char *)"float", (char *)"color"};
            char *names[2] = {(char *)"a", (char *)"b"};
            float format[3] = {0, 0, 0};
            PtcPointCloud pc = PtcCreatePointCloudFile((char *)"t3d_nosession.ptc", 2, types, names, I, I, format);
            for (int n = 0; n < 3; n++) {
                float data[4] = {0.5f + n, 1.0f, 0.25f, 0.125f};
                float point[3] = {kPoint[0] + 0.5f * n, kPoint[1], kPoint[2]};
                float normal[3] = {kNormal[0], kNormal[1], kNormal[2]};
                PtcWriteDataPoint(pc, point, normal, 0.05f, data);
            }
            PtcFinishPointCloudFile(pc);
        }
        flushDiagnostics("ptcapi create");
        describeFile("ptcapi file", (char *)"t3d_nosession.ptc", 4);

        // The string form with no session: no channel name can be resolved.
        {
            CPointCloud pc("t3d_nosession_string.ptc", I, I, I, "_area,Ci", TRUE);
            describeChannels("string form \"_area,Ci\" with no session", pc);
        }
        flushDiagnostics("string form");
        installDiagnosticSink(previous);
        return 0;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_texture3d_channels session|nosession|unknown\n");
        return 2;
    }
    int rc;
    if (strcmp(argv[1], "session") == 0)
        rc = runSession();
    else if (strcmp(argv[1], "nosession") == 0)
        rc = runNoSession();
    else if (strcmp(argv[1], "unknown") == 0)
        rc = runUnknown();
    else {
        fprintf(stderr, "unknown mode %s\n", argv[1]);
        return 2;
    }
    fflush(stdout);
    return rc;
}
