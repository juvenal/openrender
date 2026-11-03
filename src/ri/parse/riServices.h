/**
 * Project: openRender
 *
 * File: riServices.h
 *
 * Description:
 *   The services the Scene Interface calls instead of the renderer core (spec 038): the declared-variable
 *   lookup the RIB grammar needs and the file search the lexer needs for ReadArchive. The renderer core
 *   installs its own when a renderer session begins; the writer, the preview and the data viewer begin a
 *   CRiStandardSession, which installs the standard ones (Graphics State's declaration table, the search
 *   path walk without the network). With no service installed riRetrieveVariable answers nullptr and
 *   riLocateFile finds only a name with a directory separator that exists.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RISERVICES_H
#define RISERVICES_H

class CVariable;
struct TSearchpath;

class CRiDeclarationService {
    public:
        virtual ~CRiDeclarationService() = default;
        virtual CVariable *retrieveVariable(const char *name) = 0; // nullptr: not declared
        virtual CVariable *declareVariable(const char *name, const char *type) = 0;
};

class CRiFileSearchService {
    public:
        virtual ~CRiFileSearchService() = default;
        // TRUE and the path in result (OS_MAX_PATH_LENGTH) when found
        virtual int locateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize) = 0;
};

// Install a service; nullptr uninstalls. Returns the service installed before, which the caller reinstalls
// at the end of its session (a nested session restores the outer one).
CRiDeclarationService *riInstallDeclarationService(CRiDeclarationService *service);
CRiFileSearchService *riInstallFileSearchService(CRiFileSearchService *service);

// What the grammar, the lexer and ri.cpp call.
CVariable *riRetrieveVariable(const char *name); // no service: nullptr
int riLocateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize);

// The Scene's standard implementation (writer, preview, data viewer): the Graphics State table and the
// search of research M4 without the network. Scoped.
class CRiStandardSession {
    public:
        CRiStandardSession();  // CDeclarations::acquire(); install the standard services, keeping the previous ones
        ~CRiStandardSession(); // reinstall the previous services; CDeclarations::release()
        CRiStandardSession(const CRiStandardSession &) = delete;
        CRiStandardSession &operator=(const CRiStandardSession &) = delete;

    private:
        CRiDeclarationService *previousDeclarations;
        CRiFileSearchService *previousSearch;
};

#endif
