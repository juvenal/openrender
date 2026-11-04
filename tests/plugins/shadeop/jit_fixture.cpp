/**
 * Project: openRender
 *
 * File: jit_fixture.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), the fixture module of the JIT DSO parity tests
 *   (ShadeopAbi_Jit_*, contracts/jit-dso-call.md). Three operands in the spec 037 declaration format, each
 *   chosen to exercise one shape of argument the JIT's DSO call has to carry the way the interpreter does:
 *
 *     float jit_mix(float, float, float)   a * 0.5 + b * 0.25 + c: varying and uniform-promoted arguments
 *     float jit_len(string, float)         strlen(s) + x: a string argument
 *     void  jit_record(float)              a void operand: it writes a per-point record to the call log
 *                                          ("jit:record <value>"), which the test reads back and compares
 *                                          between the .rslo and the .slo render
 *
 *   The operands keep no state: they are safe to call from several shading threads. The log lines carry the
 *   value with %.6g; the test sorts them (the order of points across threads is not part of the contract).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <stdio.h>
#include <string.h>

#include "pluginLog.h"
#include "shadeop.h"

SHADEOP_DECLARE_VERSION()

SHADEOP_TABLE(jit_mix) = {
    {"float jit_mix(float, float, float)", "", ""},
    {"", "", ""}};

SHADEOP(jit_mix) {
    (void)initdata;
    (void)argc;
    const float a = *(const float *)argv[1];
    const float b = *(const float *)argv[2];
    const float c = *(const float *)argv[3];
    *(float *)argv[0] = a * 0.5f + b * 0.25f + c;
}

SHADEOP_TABLE(jit_len) = {
    {"float jit_len(string, float)", "", ""},
    {"", "", ""}};

SHADEOP(jit_len) {
    (void)initdata;
    (void)argc;
    const char *text = *(const char *const *)argv[1];
    const float x = *(const float *)argv[2];
    *(float *)argv[0] = (float)strlen(text != NULL ? text : "") + x;
}

SHADEOP_TABLE(jit_record) = {
    {"void jit_record(float)", "", ""},
    {"", "", ""}};

SHADEOP(jit_record) {
    (void)initdata;
    (void)argc;
    char line[96];
    snprintf(line, sizeof(line), OR_TEST_TAG ":record %.6g", (double)*(const float *)argv[1]);
    or_test_log(line);
}

OR_TEST_LOG_UNLOAD()
