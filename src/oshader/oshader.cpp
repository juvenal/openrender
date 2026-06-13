/**
 * Project: openRender
 *
 * File: oshader.cpp
 *
 * Description:
 *   This file implements the functionality for oshader.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

/**
 *  File: oshader.cpp
 *  Description: The main shader compiler source code
 */

#define LOGGING_IMPLEMENTATION
#include "logging.h"

#include <string.h>

#include "common/global.h" // The glorious global header
#include "common/os.h"     // OS dependent stuff)
#include "opcodes.h"       // The opcodes/arguments
#include "rslo.h"          // The CScriptContext

#ifdef OPENRENDER_HAVE_LLVM
#include "llvmEmitter.h"
#endif

// Standard includes
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __INTEL_COMPILER
#ifdef _WINDOWS
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif
#endif

extern "C" int preprocess(char *, FILE *, int, char **);

// The environment variables to be searched for the preprocessor and include files
#define SHADERS_INCLUDE "SHADERS_INCLUDE" // The default include path for the preprocessor

// Return codes
#define ERR_NONE 0       // No error
#define ERR_FILE 1       // Cannot load file, cannot create tempfile
#define ERR_PREPROCESS 2 // Error in preprocessing
#define ERR_COMPILE 3    // Error in compilation

// If the preprocessing is not turned off, then the preprocessor will be invoked to generate
// preprocessed stuff into the argumentTemporaryFileName and then the compiler will be run on
// this file

// The symbol to be defined by the preprocessor
static const char *defineProgramName = "OPENRENDER";
// The name of the executable
static const char *compilerName = "oshader";

// Program command line switches
static const char *argumentIncludeDirectory = "-I";
static const char *argumentDefine = "-D";
static const char *argumentOutput = "-o";
static const char *argumentSuppressWarnings = "-nw";
static const char *argumentSuppressErrors = "-ne";
static const char *argumentResolutionInfo = "-ri";
static const char *argumentHelp = "-h";
static const char *argumentPrintVersionInfo = "-v";
static const char *argumentQuietInfo = "-q";
static const char *argumentLogLevel = "-d";
static const char *argumentLegacyRSLObject = "--legacy-sdr";
static const char *argumentJIT = "--jit";
// static const char *argumentLogLevelLong = "--log";

/**
 * Function: printVersion
 * Description: Print the version
 * Return Value: -
 * Comments:
 */
static void printVersion() {
    printf("oshader - A RenderMan Shading Language Compiler\n");
    printf("openRender - Open Rendering Tools %s (Adheres to the RenderMan Standard)\n", openrender_version_string());
    printf("   (c) Copyright 2025-2026 by Juvenal A. Silva Jr.  All rights reserved.\n\n");
    printf("       The RenderMan (R) Interface Procedures and RIB Protocol are:\n");
    printf("            Copyright 1988, 1989, Pixar. All rights reserved.\n");
    printf("            RenderMan (R) is a registered trademark of Pixar\n");
    // printf("openRender is free software. There is NO warranty; not even for\n");
    // printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}

/**
 * Function: printUsage
 * Description: Print the compiler usage
 * Return Value: -
 * Comments:
 */
static void printUsage() {
    printf("Usage: %s [options] filename.sl [filename.sl ...]\n", compilerName);
    printf("The filenames are wildcard-expanded\n\n");
    printf("  Options:\n");
    printf("      %s <path>           Additional include path for the preprocessor\n", argumentIncludeDirectory);
    printf("      %s <symbol>         Define <symbol> for the preprocessor\n", argumentDefine);
    printf("      %s <symbol>=<value> Define <symbol> to be <value>\n", argumentDefine);
    printf("      %s <filename>       Output to <filename> \n", argumentOutput);
    printf("      %s        Force .sdr output extension (legacy)\n", argumentLegacyRSLObject);
#ifdef OPENRENDER_HAVE_LLVM
    printf("      %s              Emit .slo (LLVM JIT bitcode) instead of .rslo\n", argumentJIT);
#endif
    printf("      %s                 Suppress warnings\n", argumentSuppressWarnings);
    printf("      %s                 Suppress errors\n", argumentSuppressErrors);
    printf("      %s                  Quiet, suppress progress display\n", argumentQuietInfo);
    printf("      %s                 Display resolution information\n", argumentResolutionInfo);
    printf("      %s                  Display version information\n", argumentPrintVersionInfo);
    printf("      %s                  Display this help\n", argumentHelp);
    printf("      %s <level>          Set log verbosity: error|warn|info|debug (default: warn)\n\n", argumentLogLevel);
    printf("Environment variables:\n");
    printf("  SHADERS_INCLUDE    Additional include paths for the preprocessor\n");
}

/**
 * Function: initError
 * Description: Display an error and exit
 * Return Value: -
 * Comments:
 */
[[maybe_unused]] static void initError(const char *mes, ...) {
    char buf[1024];
    va_list args;

    va_start(args, mes);
    vsnprintf(buf, sizeof(buf), mes, args);
    va_end(args);
    fprintf(stderr, "%s\n", buf);
    exit(-1);
}

/**
 * Function: append
 * Description: Append a file name
 * Return Value: -
 * Comments:
 */
static int append(const char *file, void *ud) {
    CList<char *> *sourceFiles = (CList<char *> *)ud;

    sourceFiles->push(strdup(file));

    return TRUE;
}

/**
 * Function: main
 * Description: Shading compiler main
 * Return Value: -
 * Comments:
 */
int main(int argc, char *argv[]) {
    CScriptContext *currentCompiler;
    int i;
    int settings = COMPILER_SUPPRESS_DEFINITIONS;
    TSearchpath *dsoPath;
    CList<char *> *sourceFiles;
    char *sourceFile;
    const char *ppargv[100];
    int ppargc;
    char *outName = nullptr;
    char *includeEnv = osEnvironment(SHADERS_INCLUDE);
    int error = ERR_NONE;
    int legacyRSLObjectExt = FALSE;
    int emitJIT = FALSE;

    // Default log level is NONE; can be overridden via -x or -d.
    LOG_INIT(stderr, LOG_LEVEL_NONE);

    sourceFiles = new CList<char *>;

    dsoPath = new TSearchpath;
    dsoPath->directory = strdup(".");
    dsoPath->next = nullptr;

    ppargc = 1;
    ppargv[ppargc++] = "-c";
    ppargv[ppargc++] = "6";
    ppargv[ppargc++] = "-d";
    ppargv[ppargc++] = defineProgramName;

    if (includeEnv != nullptr) {
        char *envCopy = strdup(includeEnv);
        char *tok = strtok(envCopy, ":");
        while (tok != nullptr) {
            ppargv[ppargc++] = "-i";
            ppargv[ppargc++] = tok;
            tok = strtok(nullptr, ":");
        }
        // envCopy intentionally not freed — ppargv holds raw pointers into it
        // and must remain valid through the preprocess() call below.
    }

    // Require some parameters, or else print help and exit
    if (argc < 2) {
        printVersion();
        printf("\n");
        printUsage();
        exit(1);
    }

    // Process the arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], argumentSuppressWarnings) == 0) {
            settings |= COMPILER_SUPPRESS_WARNINGS;
        }
        else if (strcmp(argv[i], argumentSuppressErrors) == 0) {
            settings |= COMPILER_SUPPRESS_ERRORS;
        }
        else if (strcmp(argv[i], argumentResolutionInfo) == 0) {
            settings &= ~COMPILER_SUPPRESS_DEFINITIONS;
        }
        else if (strcmp(argv[i], argumentQuietInfo) == 0) {
            LOG_SET_LEVEL(LOG_LEVEL_ERROR);
        }
        else if (strcmp(argv[i], argumentLogLevel) == 0) {
            if (++i < argc) {
                if (strcasecmp(argv[i], "error") == 0)
                    LOG_SET_LEVEL(LOG_LEVEL_ERROR);
                else if (strcasecmp(argv[i], "warn") == 0)
                    LOG_SET_LEVEL(LOG_LEVEL_WARN);
                else if (strcasecmp(argv[i], "info") == 0)
                    LOG_SET_LEVEL(LOG_LEVEL_INFO);
                else if (strcasecmp(argv[i], "debug") == 0)
                    LOG_SET_LEVEL(LOG_LEVEL_DEBUG);
                else {
                    LOG_ERROR("Unknown log level: %s", argv[i]);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], "-x") == 0) {
            if (++i < argc) {
                int level = atoi(argv[i]);
                if (level == 1)
                    LOG_SET_LEVEL(LOG_LEVEL_ERROR);
                else if (level == 2)
                    LOG_SET_LEVEL(LOG_LEVEL_WARN);
                else if (level == 3)
                    LOG_SET_LEVEL(LOG_LEVEL_INFO);
                else if (level == 4)
                    LOG_SET_LEVEL(LOG_LEVEL_DEBUG);
                else {
                    LOG_ERROR("Unknown numeric log level: %s", argv[i]);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], argumentPrintVersionInfo) == 0 || strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            exit(0);
        }
        else if (strcmp(argv[i], argumentLegacyRSLObject) == 0) {
            legacyRSLObjectExt = TRUE;
        }
        else if (strcmp(argv[i], argumentJIT) == 0) {
#ifdef OPENRENDER_HAVE_LLVM
            emitJIT = TRUE;
#else
            fprintf(stderr, "oshader: --jit requires LLVM support (not compiled in)\n");
            exit(1);
#endif
        }
        else if (strcmp(argv[i], argumentHelp) == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
            printVersion();
            printf("\n");
            printUsage();
            exit(0);
        }
        else if (strcmp(argv[i], argumentOutput) == 0) {
            if (i < (argc - 1)) {
                outName = argv[i + 1];
                i++;
            }
            else {
                fprintf(stderr, "Output filename expected\n");
                // LOG_ERROR("Output filename expected");
            }
        }
        else if (strncmp(argv[i], argumentDefine, strlen(argumentDefine)) == 0) {
            ppargv[ppargc++] = "-d";
            ppargv[ppargc++] = &argv[i][2];
        }
        else if (strncmp(argv[i], argumentIncludeDirectory, strlen(argumentIncludeDirectory)) == 0) {
            TSearchpath *nPath = new TSearchpath;

            ppargv[ppargc++] = "-i";
            ppargv[ppargc++] = &argv[i][2];

            nPath->directory = strdup(&argv[i][2]);
            nPath->next = dsoPath;
            dsoPath = nPath;
        }
        else if (argv[i][0] == '-' && argv[i][1] != 0) {
            // Starts with '-' but not matched any option
            if (!(settings & (COMPILER_SUPPRESS_WARNINGS | COMPILER_SUPPRESS_ERRORS))) {
                fprintf(stderr, "Unknown option '%s'\n", argv[i]);
                // LOG_ERROR("Unknown option '%s'", argv[i]);
            }
        }
        else {
            // Save the files
            sourceFiles->push(strdup(argv[i]));
            argv[i] = nullptr;
        }
    }

    ppargv[ppargc++] = "-v";

    // Go over the arguments and replace the wildcards
    {
        char *file;
        CList<char *> *newSources = new CList<char *>;

        for (file = sourceFiles->first(); file != nullptr; file = sourceFiles->next()) {
            if ((strchr(file, '*') != nullptr) || (strchr(file, '?') != nullptr)) {
                osEnumerate(file, append, newSources);
                free(file);
            }
            else {
                newSources->push(file);
            }
        }

        delete sourceFiles;
        sourceFiles = newSources;
    }

    // Require input file
    if (sourceFiles->numItems == 0) {
        fprintf(stderr, "No input files\n");
        // LOG_ERROR("no input files");
        exit(0);
    };

    // If using -o only one file can be processed
    if (outName != nullptr && sourceFiles->numItems > 1) {
        fprintf(stderr, "Output file specified with multiple input files...\n");
        fprintf(stderr, "Named output file requires single input file\n");
        // LOG_ERROR("Named output file requires single input file");
        exit(1);
    };

    // Get a temp file
    char tempfile[OS_MAX_PATH_LENGTH];
    char tempdir[OS_MAX_PATH_LENGTH];
    osTempdir(tempdir, sizeof(tempdir));
    osCreateDir(tempdir);
    osTempname(tempdir, "oshader", tempfile);

    for (sourceFile = sourceFiles->first(); error == ERR_NONE && sourceFile != nullptr; sourceFile = sourceFiles->next()) {

        const char *srcSep = strrchr(sourceFile, OS_DIR_SEPERATOR);
        const char *sourceBasename = (srcSep != nullptr) ? srcSep + 1 : sourceFile;
        fprintf(stderr, "Compiling %s\n", sourceBasename);
        // LOG_INFO("Compiling %s", sourceBasename);

        FILE *in = fopen(tempfile, "w+");
        if (in == nullptr) {
            fprintf(stderr, "Failed to create a temporary file\n");
            // LOG_ERROR("Failed to create a temporary file");
            error = ERR_FILE;
            break;
        }

        // Preprocess the file
        if (!preprocess(sourceFile, in, ppargc, (char **)ppargv)) {
            error = ERR_PREPROCESS;
            fclose(in);
            break;
        }

        // Rewind in the file
        fseek(in, 0, SEEK_SET);

        // Create the compiler
        currentCompiler = new CScriptContext(settings);
        currentCompiler->dsoPath = dsoPath;
        currentCompiler->legacyRSLObjectExt = legacyRSLObjectExt;
#ifdef OPENRENDER_HAVE_LLVM
        currentCompiler->emitJIT = (emitJIT == TRUE);
#endif

        // Compile the file
        currentCompiler->sourceFile = sourceFile;
        currentCompiler->compile(in, outName);

        // Ditch the compiler
        if (currentCompiler->compileError) {
            error = ERR_COMPILE;
        }
        else {
            char rsloName[OS_MAX_PATH_LENGTH];
            const char *compiledBasename = nullptr;

#ifdef OPENRENDER_HAVE_LLVM
            // JIT path: emit .slo from the retained IRModule.
            if (emitJIT && currentCompiler->lastCompiledModule &&
                currentCompiler->shaderName != nullptr) {
                // Determine .slo output path.
                std::string sloPath;
                if (outName != nullptr) {
                    sloPath = outName;
                    // Replace or append .slo extension.
                    const char *ext = strrchr(outName, '.');
                    if (ext) sloPath = std::string(outName, ext - outName) + ".slo";
                    else     sloPath = std::string(outName) + ".slo";
                } else {
                    sloPath = std::string(currentCompiler->shaderName) + ".slo";
                }
                const bool ok = emitLLVMBitcode(
                    *currentCompiler->lastCompiledModule, sloPath,
                    currentCompiler->shaderName);
                if (!ok) error = ERR_COMPILE;
                else {
                    snprintf(rsloName, sizeof(rsloName), "%s", sloPath.c_str());
                    compiledBasename = strrchr(rsloName, OS_DIR_SEPERATOR);
                    compiledBasename = compiledBasename ? compiledBasename + 1 : rsloName;
                }
            } else
#endif
            {
                // Default: .rslo path.
                if (outName != nullptr) {
                    const char *outSep = strrchr(outName, OS_DIR_SEPERATOR);
                    compiledBasename = (outSep != nullptr) ? outSep + 1 : outName;
                }
                else if (currentCompiler->shaderName != nullptr) {
                    if (currentCompiler->legacyRSLObjectExt)
                        snprintf(rsloName, sizeof(rsloName), "%s.sdr", currentCompiler->shaderName);
                    else
                        snprintf(rsloName, sizeof(rsloName), "%s.rslo", currentCompiler->shaderName);
                    compiledBasename = rsloName;
                }
            }

            if (compiledBasename)
                fprintf(stderr, "... compiled %s\n", compiledBasename);
        }

        // Cleanup
        delete currentCompiler;
        fclose(in);
    }

    // Ditch the temp file
    osDeleteFile(tempfile);
    osDeleteDir(tempdir);
    free(sourceFile);
    delete sourceFiles;

    TSearchpath *cPath, *nPath;
    for (cPath = dsoPath; cPath != nullptr;) {
        nPath = cPath->next;
        free(cPath->directory);
        delete cPath;
        cPath = nPath;
    }

    return error;
}
