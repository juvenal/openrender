/**
 * Project: openRender
 *
 * File: remoteChannel.h
 *
 * Description:
 *   This file defines the interface for remoteChannel.
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

///////////////////////////////////////////////////////////////////////
//
//  File				:	remoteChannel.h
//  Classes				:
//  Description			:	This base class for remote deep data channels
//
////////////////////////////////////////////////////////////////////////
#ifndef REMOTECHANNEL_H
#define REMOTECHANNEL_H

#include "common/global.h"
#include "common/os.h"

// Forward declarations:
class CIrradianceCache;
class CPointCloud;

// Types and flags

typedef enum {
    REMOTECHANNEL_NONE,
    REMOTECHANNEL_PERFRAME,
    REMOTECHANNEL_PERBUCKET
} ERemoteChannelFlag;

typedef enum {
    CHANNELTYPE_INVALID = -1,
    CHANNELTYPE_TSM = 0,
    CHANNELTYPE_ICACHE = 1,
    CHANNELTYPE_PTCLOUD = 2,
    CHANNELTYPE_BOM = 0x55000000 // byte-order marker
} ERemoteChannelType;

// The low byte of the type word of a channel request (the type is in the bits above, the byte-order marker at the top): what the client does with its side
// of the channel. CHANNELFLAG_READ: the server opened its irradiance cache from the file, so the client opens its own from the same name too, and so holds the
// same fixed part (spec 040: the server sends only the records it made, never the file's)
typedef enum {
    CHANNELFLAG_READ = 1
} ERemoteChannelFlags;

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteChannel
// Description			:	base class for remote channels
// Comments				:
class CRemoteChannel {
    public:
        CRemoteChannel(const char *n, int f, int d) : name(strdup(n)), flags(f), channelType(d), remoteId(-1), openFlags(0) {}
        virtual ~CRemoteChannel() { free(name); };

        virtual int sendSetupData(SOCKET /*s*/) { return TRUE; }
        virtual int setup(SOCKET /*s*/) { return TRUE; }
        virtual int sendRemoteBucket(SOCKET /*s*/, int, int) { return TRUE; }
        virtual int recvRemoteBucket(SOCKET /*s*/, int, int) { return TRUE; }
        virtual int sendRemoteFrame(SOCKET /*s*/) { return TRUE; }
        virtual int recvRemoteFrame(SOCKET /*s*/) { return TRUE; }

    public:
        char *name;
        int flags;       // when should we be run?
        int channelType; // identify the derived type
        int remoteId;    // the Id of the channel on the client
        int openFlags;   // ERemoteChannelFlags: sent to the client with the request that creates the channel
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteTSMChannel
// Description			:	remote channel for tsm
// Comments				:
class CRemoteTSMChannel : public CRemoteChannel {
    public:
        CRemoteTSMChannel(const char *, FILE *, int *, int, int);
        ~CRemoteTSMChannel() {}

        int sendRemoteBucket(SOCKET s, int x, int y);
        int recvRemoteBucket(SOCKET s, int x, int y);

    private:
        FILE *tsmFile;
        int *index;
        long lastPosition;
        int xBuckets, yBuckets;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteICacheChannel
// Description			:	remote channel for irradiance caches
// Comments				:
class CRemoteICacheChannel : public CRemoteChannel {
    public:
        CRemoteICacheChannel(CIrradianceCache *);
        ~CRemoteICacheChannel() {}

        int sendSetupData(SOCKET s);
        int setup(SOCKET s);
        int sendRemoteFrame(SOCKET s);
        int recvRemoteFrame(SOCKET s);

    private:
        CIrradianceCache *cache;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRemotePtCloudChannel
// Description			:	remote channel for point clouds
// Comments				:
class CRemotePtCloudChannel : public CRemoteChannel {
    public:
        CRemotePtCloudChannel(CPointCloud *);
        ~CRemotePtCloudChannel() {}

        int sendSetupData(SOCKET s);
        int setup(SOCKET s);
        int sendRemoteFrame(SOCKET s);
        int recvRemoteFrame(SOCKET s);

    private:
        CPointCloud *cloud;
};

#endif
