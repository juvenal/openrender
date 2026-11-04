/**
 * Project: openRender
 *
 * File: test_compiler_table_drift.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), LibShader_CompilerTableDrift (US5; contracts/compiler-tables.md,
 *   research R10). The compiler owns its builtin tables (compiler/builtinTables.cpp, Phase 5) instead of
 *   re-expanding the shading engine's scriptFunctions.h / scriptOpcodes.h. Two independent tables can
 *   drift, so this test is a consumer: it includes the engine headers under its own DEF* macros (as
 *   test_used_parameters_table.cpp does) and links the compiler's arrays, and compares row by row
 *
 *     kOpcodeParamTable       (text, PARAMETER_* bits)  against scriptFunctions.h + scriptOpcodes.h
 *     kAllFunctionMnemonics   (text)                    against scriptFunctions.h
 *     kAllOpcodeMnemonics     (compiler/opcodes.cpp)    against the texts of scriptOpcodes.h, under the
 *                                                       relation research R10a measured (below)
 *
 *   On any difference it prints the first differing row of each table (index, both sides) and fails.
 *
 *   Modes:
 *     (none)             compare the real tables, then run the self-check. Registered DISABLED until
 *                        builtinTables.cpp exists: before it, both sides come from the same headers and the
 *                        comparison cannot differ.
 *     --self-check       only the mutation self-check: the same comparator runs on a copy of each compiler
 *                        table with one row's text changed and one row's bits changed; every mutation must
 *                        be reported. It is this test's red evidence (decision Q3): the comparison cannot be
 *                        red on the starting tree, the self-check shows it can fail.
 *     --emit             print builtinTables.cpp from the engine side (each row's `par` stringified as the
 *                        engine spells it), for regeneration when the engine's tables change.
 *     --measure-opcodes  print how kAllOpcodeMnemonics and the engine's opcode texts differ (research R10a).
 *
 *   The opcode relation (R10a, measured in Phase 1, see validation.md): see kEngineOnlyOpcodes /
 *   kCompilerOnlyOpcodes below; each exception is named with the reason it is not a drift.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#define LOGGING_IMPLEMENTATION
#include "common/rendererc.h"
#include "llvmEmitter.h"
#include "logging.h"
#include "opcodes.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

    struct Row {
            std::string text;
            unsigned int bits;
    };

    struct EmitRow {
            const char *text;
            const char *par; // the engine's own spelling of the parameter expression
    };

    // ---- the engine's rows, under this test's own DEF* macros ------------------------------------------------

#define DEFOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFLINKOPCODE(name, text, nargs) {text, 0u},
#define DEFLINKFUNC(name, text, prototype, par) {text, static_cast<unsigned int>(par)},
#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFPRINTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},

    struct EngineRow {
            const char *text;
            unsigned int bits;
    };

    const EngineRow kEngineFunctionRows[] = {
#include "scriptFunctions.h"
        {nullptr, 0u}};
    const EngineRow kEngineOpcodeRows[] = {
#include "scriptOpcodes.h"
        {nullptr, 0u}};

#undef DEFOPCODE
#undef DEFSHORTOPCODE
#undef DEFLINKOPCODE
#undef DEFLINKFUNC
#undef DEFFUNC
#undef DEFPRINTFUNC
#undef DEFLIGHTFUNC
#undef DEFSHORTFUNC

    // The same rows with the parameter expression kept as text, for --emit
#define DEFOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, par) {text, #par},
#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, par) {text, #par},
#define DEFLINKOPCODE(name, text, nargs) {text, "0"},
#define DEFLINKFUNC(name, text, prototype, par) {text, #par},
#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, #par},
#define DEFPRINTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, #par},
#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, #par},
#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, #par},

    const EmitRow kEmitFunctionRows[] = {
#include "scriptFunctions.h"
        {nullptr, nullptr}};
    const EmitRow kEmitOpcodeRows[] = {
#include "scriptOpcodes.h"
        {nullptr, nullptr}};

#undef DEFOPCODE
#undef DEFSHORTOPCODE
#undef DEFLINKOPCODE
#undef DEFLINKFUNC
#undef DEFFUNC
#undef DEFPRINTFUNC
#undef DEFLIGHTFUNC
#undef DEFSHORTFUNC

    // ---- the relation between the compiler's opcode mnemonics and the engine's opcode texts (R10a) ------------
    // Measured in Phase 1 (validation.md section 1, "R10a measured"): the engine has 96 opcode rows with 89 distinct texts
    // (several arities share a text: illuminance, solar, ...); the compiler's kAllOpcodeMnemonics lists 95 distinct
    // mnemonics. Every engine opcode text is in the compiler's list; the compiler's list has six mnemonics the engine
    // has no opcode row for, each named here with the reason it is not a drift.

    const char *const kEngineOnlyOpcodes[] = {nullptr}; // none: every engine opcode text is a compiler mnemonic

    const char *const kCompilerOnlyOpcodes[] = {
        // listed in compiler/opcodes.cpp's kDeadOpcodes with their evidence: the compiler never emits them, so the engine
        // has no row for them
        "beginilluminance", // GI; zero call sites in the compiler
        "moveaff",          // array-element moves; zero call sites in the compiler
        "moveavv",
        "moveass",
        "moveamm",
        // emitted in function form, `gatherHeader ("o=spnff!") ...`, and implemented by the engine as a function row
        // (giFunctions.h DEFSHORTFUNC(GatherHeader, "gatherHeader", ...)), which kAllFunctionMnemonics compares
        "gatherHeader",
        nullptr};

    bool inList(const char *const *list, const std::string &text) {
        for (int i = 0; list[i] != nullptr; ++i)
            if (text == list[i])
                return true;
        return false;
    }

    // ---- views --------------------------------------------------------------------------------------------------

    std::vector<Row> engineParamRows() {
        std::vector<Row> rows;
        for (int i = 0; kEngineFunctionRows[i].text != nullptr; ++i)
            rows.push_back({kEngineFunctionRows[i].text, kEngineFunctionRows[i].bits});
        for (int i = 0; kEngineOpcodeRows[i].text != nullptr; ++i)
            rows.push_back({kEngineOpcodeRows[i].text, kEngineOpcodeRows[i].bits});
        return rows;
    }

    std::vector<Row> compilerParamRows() {
        std::vector<Row> rows;
        for (int i = 0; kOpcodeParamTable[i].text != nullptr; ++i)
            rows.push_back({kOpcodeParamTable[i].text, kOpcodeParamTable[i].params});
        return rows;
    }

    std::vector<std::string> engineFunctionNames() {
        std::vector<std::string> names;
        for (int i = 0; kEngineFunctionRows[i].text != nullptr; ++i)
            names.push_back(kEngineFunctionRows[i].text);
        return names;
    }

    std::vector<std::string> compilerFunctionNames() {
        std::vector<std::string> names;
        for (int i = 0; kAllFunctionMnemonics[i] != nullptr; ++i)
            names.push_back(kAllFunctionMnemonics[i]);
        return names;
    }

    std::set<std::string> engineOpcodeTexts() {
        std::set<std::string> names;
        for (int i = 0; kEngineOpcodeRows[i].text != nullptr; ++i)
            names.insert(kEngineOpcodeRows[i].text);
        return names;
    }

    std::set<std::string> compilerOpcodeTexts() {
        std::set<std::string> names;
        for (int i = 0; kAllOpcodeMnemonics[i] != nullptr; ++i) {
            char stripped[64];
            stripOpcodeMnemonic(kAllOpcodeMnemonics[i], stripped, sizeof(stripped));
            names.insert(stripped);
        }
        return names;
    }

    // ---- comparators: "" when equal, else a description of the first differing row ------------------------------

    std::string diffRows(const std::vector<Row> &compiler, const std::vector<Row> &engine) {
        const size_t n = compiler.size() < engine.size() ? compiler.size() : engine.size();
        for (size_t i = 0; i < n; ++i) {
            if (compiler[i].text != engine[i].text || compiler[i].bits != engine[i].bits) {
                char buffer[256];
                std::snprintf(buffer, sizeof(buffer), "row %zu: compiler (\"%s\", 0x%08x) engine (\"%s\", 0x%08x)", i, compiler[i].text.c_str(),
                              compiler[i].bits, engine[i].text.c_str(), engine[i].bits);
                return buffer;
            }
        }
        if (compiler.size() != engine.size())
            return "row count: compiler " + std::to_string(compiler.size()) + " engine " + std::to_string(engine.size());
        return "";
    }

    std::string diffNames(const std::vector<std::string> &compiler, const std::vector<std::string> &engine) {
        const size_t n = compiler.size() < engine.size() ? compiler.size() : engine.size();
        for (size_t i = 0; i < n; ++i)
            if (compiler[i] != engine[i])
                return "row " + std::to_string(i) + ": compiler \"" + compiler[i] + "\" engine \"" + engine[i] + "\"";
        if (compiler.size() != engine.size())
            return "row count: compiler " + std::to_string(compiler.size()) + " engine " + std::to_string(engine.size());
        return "";
    }

    std::string diffOpcodes(const std::set<std::string> &compiler, const std::set<std::string> &engine) {
        for (const std::string &e : engine)
            if (compiler.count(e) == 0 && !inList(kEngineOnlyOpcodes, e))
                return "the engine has an opcode \"" + e + "\" the compiler's kAllOpcodeMnemonics does not list (and it is not a named exception)";
        for (const std::string &c : compiler)
            if (engine.count(c) == 0 && !inList(kCompilerOnlyOpcodes, c))
                return "the compiler lists an opcode \"" + c + "\" the engine has no row for (and it is not a named exception)";
        return "";
    }

    // ---- modes --------------------------------------------------------------------------------------------------

    int g_failed = 0;

    void expectEqual(const char *what, const std::string &difference) {
        if (!difference.empty()) {
            std::fprintf(stderr, "DRIFT in %s: %s\n", what, difference.c_str());
            ++g_failed;
        }
    }

    void expectReported(const char *what, const std::string &difference) {
        if (difference.empty()) {
            std::fprintf(stderr, "SELF-CHECK FAILED: the mutation \"%s\" was not reported\n", what);
            ++g_failed;
        }
        else {
            std::printf("self-check: \"%s\" reported: %s\n", what, difference.c_str());
        }
    }

    void compareTables() {
        expectEqual("kOpcodeParamTable", diffRows(compilerParamRows(), engineParamRows()));
        expectEqual("kAllFunctionMnemonics", diffNames(compilerFunctionNames(), engineFunctionNames()));
        expectEqual("kAllOpcodeMnemonics", diffOpcodes(compilerOpcodeTexts(), engineOpcodeTexts()));
    }

    // The comparator on a copy of each compiler table with one row changed. The copy is the compiler's own table, so a
    // comparator that cannot see a difference would be reported here.
    void selfCheck() {
        const std::vector<Row> engineRows = engineParamRows();
        std::vector<Row> copy = compilerParamRows();
        if (copy.size() < 8) {
            std::fprintf(stderr, "SELF-CHECK FAILED: the compiler's kOpcodeParamTable has %zu rows\n", copy.size());
            ++g_failed;
            return;
        }
        const size_t textRow = copy.size() / 3, bitsRow = copy.size() / 2;
        copy[textRow].text += "_mutated";
        expectReported("one row's text changed (kOpcodeParamTable)", diffRows(copy, engineRows));
        copy = compilerParamRows();
        copy[bitsRow].bits ^= 0x1u;
        expectReported("one row's bits changed (kOpcodeParamTable)", diffRows(copy, engineRows));
        copy = compilerParamRows();
        copy.pop_back();
        expectReported("one row missing (kOpcodeParamTable)", diffRows(copy, engineRows));

        std::vector<std::string> names = compilerFunctionNames();
        names[names.size() / 2] += "_mutated";
        expectReported("one row's text changed (kAllFunctionMnemonics)", diffNames(names, engineFunctionNames()));

        std::set<std::string> opcodes = compilerOpcodeTexts();
        opcodes.erase(opcodes.begin());
        opcodes.insert("not_an_opcode");
        expectReported("an opcode renamed (kAllOpcodeMnemonics)", diffOpcodes(opcodes, engineOpcodeTexts()));
    }

    void emit() {
        std::printf(
            "/**\n"
            " * Project: openRender\n"
            " *\n"
            " * File: builtinTables.cpp\n"
            " *\n"
            " * Description:\n"
            " *   The RSL compiler's own tables of the shading engine's builtin functions and opcodes: the\n"
            " *   PARAMETER_* bits each carries (kOpcodeParamTable) and the builtin function names\n"
            " *   (kAllFunctionMnemonics). Generated by `test_libshader_compiler_table_drift --emit` from the\n"
            " *   engine's scriptFunctions.h and scriptOpcodes.h (spec 038); do not edit by hand. When the engine's\n"
            " *   tables change, LibShader_CompilerTableDrift fails and this file is regenerated with that command.\n"
            " *\n"
            " * Authors:\n"
            " *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>\n"
            " *\n"
            " * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>\n"
            " *\n"
            " * License: GNU Lesser General Public License (LGPL) 2.1\n"
            " *\n"
            " */\n"
            "#include \"common/rendererc.h\"\n"
            "#include \"llvmEmitter.h\"\n\n"
            "extern const OpcodeParamEntry kOpcodeParamTable[] = {\n");
        for (int i = 0; kEmitFunctionRows[i].text != nullptr; ++i)
            std::printf("    {\"%s\", %s},\n", kEmitFunctionRows[i].text, kEmitFunctionRows[i].par);
        for (int i = 0; kEmitOpcodeRows[i].text != nullptr; ++i)
            std::printf("    {\"%s\", %s},\n", kEmitOpcodeRows[i].text, kEmitOpcodeRows[i].par);
        std::printf("    {nullptr, 0u}};\n\nextern const char *const kAllFunctionMnemonics[] = {\n");
        for (int i = 0; kEmitFunctionRows[i].text != nullptr; ++i)
            std::printf("    \"%s\",\n", kEmitFunctionRows[i].text);
        std::printf("    nullptr};\n");
    }

    void measureOpcodes() {
        const std::set<std::string> engine = engineOpcodeTexts();
        const std::set<std::string> compiler = compilerOpcodeTexts();
        int engineRows = 0, compilerEntries = 0;
        for (int i = 0; kEngineOpcodeRows[i].text != nullptr; ++i)
            ++engineRows;
        for (int i = 0; kAllOpcodeMnemonics[i] != nullptr; ++i)
            ++compilerEntries;
        std::printf("engine: %d opcode rows, %zu distinct texts\n", engineRows, engine.size());
        std::printf("compiler: %d kAllOpcodeMnemonics entries, %zu distinct stripped mnemonics\n", compilerEntries, compiler.size());
        int onlyEngine = 0, onlyCompiler = 0;
        for (const std::string &e : engine)
            if (compiler.count(e) == 0) {
                std::printf("ENGINE-ONLY   %s\n", e.c_str());
                ++onlyEngine;
            }
        for (const std::string &c : compiler)
            if (engine.count(c) == 0) {
                std::printf("COMPILER-ONLY %s\n", c.c_str());
                ++onlyCompiler;
            }
        std::printf("%d engine-only, %d compiler-only\n", onlyEngine, onlyCompiler);
    }

} // namespace

int main(int argc, char **argv) {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "--emit") {
        emit();
        return 0;
    }
    if (mode == "--measure-opcodes") {
        measureOpcodes();
        return 0;
    }
    if (mode != "--self-check")
        compareTables();
    selfCheck();
    if (g_failed != 0) {
        std::fprintf(stderr, "LibShader_CompilerTableDrift: %d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("LibShader_CompilerTableDrift: ok\n");
    return 0;
}
