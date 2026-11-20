/**
 * Project: openRender
 *
 * File: oiio.cpp
 *
 * Description:
 *   The `oiio` display driver (spec 048): one display module that writes TIFF, PNG, OpenEXR (where built) and RGBE through libRIBImageIO, so
 *   through OpenImageIO. It is built on the same scanline pipeline as the legacy file, openexr and rgbe drivers (CFileOutputBase: quantize,
 *   dither, gamma), so the samples it writes are theirs; only the storage is OpenImageIO's. The legacy drivers are not changed.
 *
 *   The format: the display line's own `"string type"` (tiff, png, openexr, rgbe and their short names), else the extension of the file name
 *   (.tif .tiff .png .exr .hdr .pic), else an error. The renderer always passes a "type" parameter equal to the display type, which is
 *   "oiio" here: a "type" whose value is the display's own name is not a format and is ignored. RGBE needs 3 or 4 samples (R, G, B; alpha is
 *   dropped).
 *
 *   Display interface 2.0.0. start, data and finish catch everything: a failure is reported through error() and returns OR_STATUS_ERROR.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include "dsply.h"
#include "file/file_base.h"
#include "orImageIO.h"

#include "common/diagnostics.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <exception>

OR_DISPLAY_DECLARE_VERSION()

namespace {

    bool sameText(const char *a, const char *b) {
        for (; *a != '\0' && *b != '\0'; a++, b++)
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
                return false;
        return *a == *b;
    }

    // A format name or an extension (without the dot) as a format; false for anything else.
    bool formatOf(const char *text, orimageio::Format &format) {
        if (sameText(text, "tiff") || sameText(text, "tif")) {
            format = orimageio::Format::Tiff;
            return true;
        }
        if (sameText(text, "png")) {
            format = orimageio::Format::Png;
            return true;
        }
        if (sameText(text, "openexr") || sameText(text, "exr")) {
            format = orimageio::Format::OpenExr;
            return true;
        }
        if (sameText(text, "rgbe") || sameText(text, "hdr") || sameText(text, "pic")) {
            format = orimageio::Format::Rgbe;
            return true;
        }
        return false;
    }

    // The extension of the last path component, without the dot; null when it has none (a dot in a directory name is not an extension).
    const char *extensionOf(const char *name) {
        const char *base = strrchr(name, '/');
        base = (base != nullptr) ? base + 1 : name;
        const char *dot = strrchr(base, '.');
        return (dot != nullptr && dot[1] != '\0') ? dot + 1 : nullptr;
    }

    class COiioFramebuffer : public CFileOutputBase {
        public:
            explicit COiioFramebuffer(const OrDisplayInfo *info)
                : CFileOutputBase(info, /*pixelSize placeholder, set below*/ info->numSamples, /*isDepth=*/strcmp(info->mode, "z") == 0), writer(info->name) {
                orimageio::Format format;
                if (!resolveFormat(info, format))
                    return;

                const int ns = info->numSamples;
                orimageio::ScanlineDesc desc;
                desc.format = format;
                desc.width = info->width;
                desc.height = info->height;
                desc.channels = ns;
                desc.software = (const char *)orDisplayFindParameter(info, "Software", OR_DISPLAY_PARAM_STRING, 1);
                desc.compression = (const char *)orDisplayFindParameter(info, "compression", OR_DISPLAY_PARAM_STRING, 1);

                // the matrices of a TIFF display: always written, zeros when the renderer gave none (as the legacy driver does)
                float worldToScreen[16] = {};
                float worldToCamera[16] = {};
                const float *tmp;
                if ((tmp = (const float *)orDisplayFindParameter(info, "NP", OR_DISPLAY_PARAM_FLOAT, 16)))
                    memcpy(worldToScreen, tmp, sizeof(worldToScreen));
                if ((tmp = (const float *)orDisplayFindParameter(info, "Nl", OR_DISPLAY_PARAM_FLOAT, 16)))
                    memcpy(worldToCamera, tmp, sizeof(worldToCamera));

                switch (format) {
                case orimageio::Format::Tiff:
                    desc.worldToScreen = worldToScreen;
                    desc.worldToCamera = worldToCamera;
                    if (qmax == 0)
                        desc.type = orimageio::SampleType::Float;
                    else if (qmax > 65535)
                        desc.type = orimageio::SampleType::UInt32;
                    else if (qmax > 255)
                        desc.type = orimageio::SampleType::UInt16;
                    else
                        desc.type = orimageio::SampleType::UInt8;
                    break;
                case orimageio::Format::Png:
                    // PNG cannot store floats: 8 bits when no quantize is given (as the legacy driver does)
                    if (qmax == 0) {
                        qzero = 0;
                        qone = 255;
                        qmin = 0;
                        qmax = 255;
                    }
                    if (qmax > 65535) {
                        error(CODE_BADTOKEN, "oiio display: PNG holds at most 16 bits per sample (quantize maximum %g) for \"%s\"\n", (double)qmax, info->name);
                        return;
                    }
                    desc.type = (qmax > 255) ? orimageio::SampleType::UInt16 : orimageio::SampleType::UInt8;
                    break;
                case orimageio::Format::OpenExr:
#ifdef HAVE_OPENEXR
                    desc.type = orimageio::SampleType::Half;
                    break;
#else
                    error(CODE_BADTOKEN, "oiio display: OpenEXR support is not built into this binary (format \"openexr\", \"%s\")\n", info->name);
                    return;
#endif
                case orimageio::Format::Rgbe:
                    if (ns != 3 && ns != 4) {
                        error(CODE_BADTOKEN, "oiio display: an RGBE display needs 3 or 4 samples (R, G, B; alpha is dropped), not %d, for \"%s\"\n", ns, info->name);
                        return;
                    }
                    desc.channels = 3;
                    desc.type = orimageio::SampleType::Float;
                    break;
                }
                type = desc.type;
                outChannels = desc.channels;

                pixelSize = outChannels * sampleBytes(type);
                if (!writer.open(desc))
                    return;
                opened = true;
            }

            ~COiioFramebuffer() override {
                try {
                    writer.close();
                }
                catch (...) {
                }
            }

            bool success() const override { return opened; }

            // false once a row failed to write
            bool healthy() const { return !failedRow; }

        protected:
            // Stores the rows in their final type; the first outChannels samples of each pixel (RGBE drops alpha)
            void fillPixels(int row, int xOff, int nPx, const float *src) override {
                uint8_t *line = scanlines[row] + (size_t)xOff * outChannels * sampleBytes(type);
                for (int p = 0; p < nPx; p++, src += numSamples) {
                    for (int c = 0; c < outChannels; c++) {
                        switch (type) {
                        case orimageio::SampleType::UInt8:
                            *line++ = (uint8_t)src[c];
                            break;
                        case orimageio::SampleType::UInt16:
                        {
                            const uint16_t v = (uint16_t)src[c];
                            memcpy(line, &v, sizeof(v));
                            line += sizeof(v);
                            break;
                        }
                        case orimageio::SampleType::UInt32:
                        {
                            const uint32_t v = (uint32_t)src[c];
                            memcpy(line, &v, sizeof(v));
                            line += sizeof(v);
                            break;
                        }
                        default: // Half (delivered as float) and Float
                            memcpy(line, &src[c], sizeof(float));
                            line += sizeof(float);
                            break;
                        }
                    }
                }
            }

            void flushRow(int row) override {
                if (!writer.writeScanline(row, scanlines[row]))
                    failedRow = true;
            }

        private:
            static int sampleBytes(orimageio::SampleType t) {
                switch (t) {
                case orimageio::SampleType::UInt8:
                    return 1;
                case orimageio::SampleType::UInt16:
                    return 2;
                default: // UInt32, Float, and Half delivered as float
                    return 4;
                }
            }

            // Sets `format` from the display's own "type", else from the extension; reports and returns false when neither says.
            bool resolveFormat(const OrDisplayInfo *info, orimageio::Format &format) {
                const char *own = nullptr;
                for (int i = 0; i < info->numParameters; i++) {
                    const OrDisplayParameter *p = &info->parameters[i];
                    if (p->type != OR_DISPLAY_PARAM_STRING || p->numItems != 1 || strcmp(p->name, "type") != 0)
                        continue;
                    const char *value = (const char *)p->data;
                    if (value != nullptr && strcmp(value, "oiio") != 0) { // "oiio" is the renderer's own copy of the display type
                        own = value;
                        break;
                    }
                }
                if (own != nullptr) {
                    if (formatOf(own, format))
                        return true;
                }
                else {
                    const char *ext = extensionOf(info->name);
                    if (ext != nullptr && formatOf(ext, format))
                        return true;
                }
                error(CODE_BADTOKEN, "oiio display: cannot tell the format of \"%s\"; give a known extension or \"string type\"\n", info->name);
                return false;
            }

            orimageio::ScanlineWriter writer;
            orimageio::SampleType type = orimageio::SampleType::UInt8;
            int outChannels = 0;
            bool opened = false;
            bool failedRow = false;
    };

    OrStatus oiioStart(OrDisplayContext * /*ctx*/, const OrDisplayInfo *info, void **instance) {
        try {
            COiioFramebuffer *fb = new COiioFramebuffer(info);
            if (!fb->success()) {
                delete fb;
                return OR_STATUS_ERROR;
            }
            *instance = fb;
            return OR_STATUS_OK;
        }
        catch (const std::exception &e) {
            error(CODE_SYSTEM, "oiio display: cannot start \"%s\": %s\n", info->name, e.what());
        }
        catch (...) {
            error(CODE_SYSTEM, "oiio display: cannot start \"%s\"\n", info->name);
        }
        return OR_STATUS_ERROR;
    }

    OrStatus oiioData(OrDisplayContext * /*ctx*/, void *instance, int x, int y, int w, int h, float *pixels) {
        assert(instance != nullptr);
        COiioFramebuffer *fb = static_cast<COiioFramebuffer *>(instance);
        try {
            fb->write(x, y, w, h, pixels);
            return fb->healthy() ? OR_STATUS_OK : OR_STATUS_ERROR;
        }
        catch (const std::exception &e) {
            error(CODE_SYSTEM, "oiio display: cannot write a tile: %s\n", e.what());
        }
        catch (...) {
            error(CODE_SYSTEM, "oiio display: cannot write a tile\n");
        }
        return OR_STATUS_ERROR;
    }

    void oiioFinish(OrDisplayContext * /*ctx*/, void *instance) {
        assert(instance != nullptr);
        try {
            delete static_cast<COiioFramebuffer *>(instance);
        }
        catch (...) {
        }
    }

    const OrDisplayPlugin g_plugin = {{(uint32_t)sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, oiioStart, oiioData, oiioFinish};

} // namespace

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo * /*host*/) {
    return &g_plugin;
}
