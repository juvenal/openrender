/**
 * Project: openRender
 *
 * File: spectrum.h
 *
 * Description:
 *   Spectrum (spec 035, data-model.md "Spectrum"): a fixed number of float channels, three today (D18). Header-only, Infra.
 *   The channel count is a compile-time constant, so a later spectral build changes one number and the places that name
 *   .r/.g/.b stay few: only fromRGB()/toRGB() name them. A Spectrum is exactly its channels, so across a C boundary it is a
 *   `const float *` plus a count (FR-005), never a value. The node weights of the closure structs stay float[3] (D17); the C++
 *   view converts them with fromArray3().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <cfloat>

namespace openrender {

    inline constexpr int kSpectrumChannels = 3;

    struct Spectrum {
            float c[kSpectrumChannels];

            constexpr Spectrum() : c{} {}

            // The conversions: the array is `kSpectrumChannels` floats (fromArray3/toArray3 name the three of today's build)
            static constexpr Spectrum fromArray3(const float *values) {
                Spectrum s;
                for (int i = 0; i < kSpectrumChannels; ++i)
                    s.c[i] = values[i];
                return s;
            }
            constexpr void toArray3(float *values) const {
                for (int i = 0; i < kSpectrumChannels; ++i)
                    values[i] = c[i];
            }

            // The only places that name red, green and blue
            static constexpr Spectrum fromRGB(float r, float g, float b) {
                Spectrum s;
                s.c[0] = r;
                s.c[1] = g;
                s.c[2] = b;
                return s;
            }
            constexpr void toRGB(float &r, float &g, float &b) const {
                r = c[0];
                g = c[1];
                b = c[2];
            }

            // Per channel arithmetic
            friend constexpr Spectrum operator+(const Spectrum &a, const Spectrum &b) {
                Spectrum s;
                for (int i = 0; i < kSpectrumChannels; ++i)
                    s.c[i] = a.c[i] + b.c[i];
                return s;
            }
            friend constexpr Spectrum operator-(const Spectrum &a, const Spectrum &b) {
                Spectrum s;
                for (int i = 0; i < kSpectrumChannels; ++i)
                    s.c[i] = a.c[i] - b.c[i];
                return s;
            }
            friend constexpr Spectrum operator*(const Spectrum &a, const Spectrum &b) {
                Spectrum s;
                for (int i = 0; i < kSpectrumChannels; ++i)
                    s.c[i] = a.c[i] * b.c[i];
                return s;
            }
            friend constexpr Spectrum operator*(const Spectrum &a, float f) {
                Spectrum s;
                for (int i = 0; i < kSpectrumChannels; ++i)
                    s.c[i] = a.c[i] * f;
                return s;
            }
            friend constexpr Spectrum operator*(float f, const Spectrum &a) { return a * f; }
            friend constexpr Spectrum operator/(const Spectrum &a, float f) {
                Spectrum s;
                for (int i = 0; i < kSpectrumChannels; ++i)
                    s.c[i] = a.c[i] / f;
                return s;
            }

            // The channel mean (the sum of the channels in order, over the channel count)
            constexpr float mean() const {
                float sum = c[0];
                for (int i = 1; i < kSpectrumChannels; ++i)
                    sum += c[i];
                return sum / (float)kSpectrumChannels;
            }
            constexpr float maxChannel() const {
                float m = c[0];
                for (int i = 1; i < kSpectrumChannels; ++i)
                    if (c[i] > m)
                        m = c[i];
                return m;
            }

            // Every channel is zero (negative zero is zero)
            constexpr bool isZero() const {
                for (int i = 0; i < kSpectrumChannels; ++i)
                    if (!(c[i] == 0.0f))
                        return false;
                return true;
            }
            // Every channel is a finite number (neither NaN nor infinite)
            constexpr bool isFinite() const {
                for (int i = 0; i < kSpectrumChannels; ++i)
                    if (!(c[i] == c[i]) || c[i] > FLT_MAX || c[i] < -FLT_MAX)
                        return false;
                return true;
            }
            // Every channel is a number that is not below zero (NaN is not)
            constexpr bool isNonNegative() const {
                for (int i = 0; i < kSpectrumChannels; ++i)
                    if (!(c[i] >= 0.0f))
                        return false;
                return true;
            }
    };

} // namespace openrender

#endif
