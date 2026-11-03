/**
 * Project: openRender
 *
 * File: riServices.cpp
 *
 * Description:
 *   The services of riServices.h and the standard session.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <string.h>

#include "common/diagnostics.h"
#include "common/os.h"
#include "common/variable.h"
#include "declarations.h"
#include "options.h"
#include "riServices.h"

namespace {

    CRiDeclarationService *g_declarationService = nullptr;
    CRiFileSearchService *g_searchService = nullptr;

    // The standard declarations: Graphics State's table
    class CStandardDeclarations : public CRiDeclarationService {
        public:
            CVariable *retrieveVariable(const char *name) override { return CDeclarations::find(name); }
            CVariable *declareVariable(const char *name, const char *type) override { return CDeclarations::declare(name, type, 0); }
    };

    // The standard file search: CRenderer::locateFile without the network and the temporary directory
    // (research M4, R3). A name with a directory separator is checked as given; a bare name is looked for in
    // each search-path directory and then in the working directory; failing that the name is normalized
    // (separators become '_') and searched again; the info texts are the core's.
    class CStandardFileSearch : public CRiFileSearchService {
        public:
            int locateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize) override {
                if (strchr(name, OS_DIR_SEPERATOR)) {
                    // Supplied path
                    if (osFileExists(name)) {
                        strcpy(result, name);
                        info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, name);
                        return TRUE;
                    }
                }
                else {
                    // Only filename: the search path
                    for (; searchpath != NULL; searchpath = searchpath->next) {
                        snprintf(result, OS_MAX_PATH_LENGTH, "%s%s", searchpath->directory, name);
                        osFixSlashes(result);
                        if (osFileExists(result)) {
                            info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, result);
                            return TRUE;
                        }
                    }

                    // Then the working directory
                    snprintf(result, OS_MAX_PATH_LENGTH, "%s", name);
                    osFixSlashes(result);
                    if (osFileExists(result)) {
                        info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, result);
                        return TRUE;
                    }
                }

                // Should we check normalized?
                if (tryNormalize) {
                    char normalizedName[OS_MAX_PATH_LENGTH];

                    strcpy(normalizedName, name);
                    for (char *c = normalizedName; *c != '\0'; ++c)
                        if ((*c == '/') || (*c == '\\'))
                            *c = '_';

                    return locateFile(result, normalizedName, searchpath, FALSE);
                }

                info(CODE_RESOLUTION, "\"%s\" -> ???\n", name);

                return FALSE;
            }
    };

    CStandardDeclarations g_standardDeclarations;
    CStandardFileSearch g_standardSearch;

} // namespace

CRiDeclarationService *riInstallDeclarationService(CRiDeclarationService *service) {
    CRiDeclarationService *previous = g_declarationService;
    g_declarationService = service;
    return previous;
}

CRiFileSearchService *riInstallFileSearchService(CRiFileSearchService *service) {
    CRiFileSearchService *previous = g_searchService;
    g_searchService = service;
    return previous;
}

CVariable *riRetrieveVariable(const char *name) {
    return g_declarationService != nullptr ? g_declarationService->retrieveVariable(name) : nullptr;
}

int riLocateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize) {
    if (g_searchService != nullptr)
        return g_searchService->locateFile(result, name, searchpath, tryNormalize);

    // No service: only a name with a directory separator that exists
    if (strchr(name, OS_DIR_SEPERATOR) && osFileExists(name)) {
        strcpy(result, name);
        info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, name);
        return TRUE;
    }

    return FALSE;
}

CRiStandardSession::CRiStandardSession() {
    CDeclarations::acquire();
    previousDeclarations = riInstallDeclarationService(&g_standardDeclarations);
    previousSearch = riInstallFileSearchService(&g_standardSearch);
}

CRiStandardSession::~CRiStandardSession() {
    riInstallFileSearchService(previousSearch);
    riInstallDeclarationService(previousDeclarations);
    CDeclarations::release();
}
