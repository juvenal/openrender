/**
 * Project: openRender
 *
 * File: remoteChannel.cpp
 *
 * Description:
 *   This file implements the functionality for remoteChannel.
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
//  File				:	remoteChannel.cpp
//  Classes				:
//  Description			:	This base class for remote deep data channels
//
////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <string.h>

#include <vector>

#include "common/containers.h"
#include "common/global.h"

#include "common/diagnostics.h"
#include "irradianceCache.h"
#include "pointCloud.h"
#include "remoteChannel.h"
#include "renderer.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	requestRemoteChannel
// Description			:	Ask client to set up remote channel like the
//							one passed as an argument
// Return Value			:
// Comments				:	The channel will either be deleted or managed
int CRenderer::requestRemoteChannel(CRemoteChannel *serverChannel) {
    int nameLength = (int)strlen(serverChannel->name) + 1;
    int clientInitialized = FALSE;
    T32 buffer[3];

    // Send the request
    // Note: we send a byte-order marker to ensure same-endianness
    buffer[0].integer = NET_CREATE_CHANNEL;
    buffer[1].integer = nameLength;
    buffer[2].integer = (serverChannel->channelType << 8) | CHANNELTYPE_BOM;
    rcSend(netClient, &buffer[0], 2 * sizeof(T32));
    rcSend(netClient, &buffer[2], 1 * sizeof(T32), FALSE);
    rcSend(netClient, serverChannel->name, nameLength, FALSE);

    // Does the client accept the request?
    rcRecv(netClient, buffer, 2 * sizeof(T32));
    if (buffer[0].integer != -1) {
        serverChannel->remoteId = buffer[0].integer;

        // do we need to intialize
        if (buffer[1].integer == NET_ACK) {

            // yes? initialize the client channel
            if (serverChannel->sendSetupData(netClient) == FALSE) {
                // Doesn't matter what the client thought, just ignore it
                rcRecv(netClient, buffer, sizeof(T32));

                // Notify client of failure
                buffer[0].integer = NET_NACK;
                rcSend(netClient, buffer, sizeof(T32));

                error(CODE_BUG, "Remote channel initialization failed\n");
                delete serverChannel;
                return FALSE;
            }

            // Did the client initialize correctly
            rcRecv(netClient, buffer, sizeof(T32));
            clientInitialized = (buffer[0].integer == NET_ACK);

            // let client know we initialized
            buffer[0].integer = NET_ACK;
            rcSend(netClient, buffer, sizeof(T32));
        }
        else {
            clientInitialized = TRUE;
        }
    }
    else {
        error(CODE_BUG, "Client refused remote channel request\n");
        delete serverChannel;
        return FALSE;
    }

    if (clientInitialized == TRUE) {
        // record the channel
        remoteChannels->push(serverChannel);
        declaredRemoteChannels->insert(serverChannel->name, serverChannel);
    }
    else {
        error(CODE_BUG, "Remote channel initialization failed\n");
        delete serverChannel;
        return FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	processChannelRequest
// Description			:	Service request for channel, adding the
//							newly created channel to the list if successful
// Return Value			:
// Comments				:	called from CRendererContext:processServerRequest
int CRenderer::processChannelRequest(int, SOCKET s) {
    int channelNameLength = 0;
    int channelType = 0;
    CRemoteChannel *rChannel = NULL;

    // Receive the request
    T32 buffer[2];
    buffer[0].integer = REMOTECHANNEL_NONE;
    buffer[1].integer = 0;
    rcRecv(s, &buffer[0], sizeof(T32));
    rcRecv(s, &buffer[1], sizeof(T32), FALSE);

    // verify type
    channelNameLength = buffer[0].integer;
    channelType = buffer[1].integer;
    char *channelName = (char *)alloca(channelNameLength);
    rcRecv(s, channelName, channelNameLength, FALSE);

    // check channel byte order
    if ((channelType & CHANNELTYPE_BOM) != CHANNELTYPE_BOM) {
        // unknown type!
        buffer[0].integer = -1;
        buffer[1].integer = NET_NACK;
        rcSend(s, buffer, 2 * sizeof(T32));
        error(CODE_BUG, "Remote channels may not communicate over different byte orders\n");
        return FALSE;
    }

    channelType &= ~CHANNELTYPE_BOM;
    channelType = channelType >> 8;

    // Check if we've seen the channel before
    if (declaredRemoteChannels->find(channelName, rChannel) == TRUE) {
        // If so, trivially accept it
        buffer[0].integer = rChannel->remoteId;
        buffer[1].integer = NET_NACK;
        rcSend(s, buffer, 2 * sizeof(T32));
        return TRUE;
    }
    else {
        switch (channelType) {
            case CHANNELTYPE_TSM:
                // create a TSM channel
                rChannel = new CRemoteTSMChannel(channelName, deepShadowFile, deepShadowIndex, xBuckets, yBuckets);
                rChannel->remoteId = remoteChannels->numItems;
                buffer[0].integer = rChannel->remoteId;
                buffer[1].integer = NET_ACK;
                break;
            case CHANNELTYPE_ICACHE:
            {
                // create an irradiancecache channel
                CIrradianceCache *cache = static_cast<CIrradianceCache *>(getCache(channelName, "w", CRenderer::fromWorld, CRenderer::toWorld));
                if (cache == NULL) {
                    // getCache answers nothing for a name already registered as another kind of 3D file
                    buffer[0].integer = -1;
                    buffer[1].integer = NET_NACK;
                    rcSend(s, buffer, 2 * sizeof(T32));
                    error(CODE_BADTOKEN, "Remote channel \"%s\" is already registered as another kind of file than an irradiance cache\n", channelName);
                    return FALSE;
                }
                rChannel = new CRemoteICacheChannel(cache);
                rChannel->remoteId = remoteChannels->numItems;
                buffer[0].integer = rChannel->remoteId;
                buffer[1].integer = NET_ACK;
                break;
            }
            case CHANNELTYPE_PTCLOUD:
            {
                // create an pointcloud channel.
                // Note: the channel definitions are duff
                CPointCloud *cloud = (CPointCloud *)getTexture3d(channelName, TRUE, NULL, CRenderer::fromWorld, CRenderer::toWorld);
                rChannel = new CRemotePtCloudChannel(cloud);
                rChannel->remoteId = remoteChannels->numItems;
                buffer[0].integer = rChannel->remoteId;
                buffer[1].integer = NET_ACK;
                break;
            }
            case CHANNELTYPE_INVALID:
            default:
                // unknown type!
                buffer[0].integer = -1;
                buffer[1].integer = NET_NACK;
                rcSend(s, buffer, 2 * sizeof(T32));
                error(CODE_BUG, "Invalid remote channel type requested\n");
                return FALSE;
        }

        // Send the accept
        rcSend(s, buffer, 2 * sizeof(T32));

        // Attempt to setup the channel
        if (rChannel->setup(s) == FALSE) {
            // tell server that initialization failed
            buffer[0].integer = NET_NACK;
            rcSend(s, buffer, sizeof(T32));

            // Ignore the server response
            rcRecv(s, buffer, sizeof(T32));
            error(CODE_BUG, "Remote channel initialization failed\n");
            return FALSE;
        }

        // Notify server that we succeeded
        buffer[0].integer = NET_ACK;
        rcSend(s, buffer, sizeof(T32));

        // Find out if the server succeeded...
        rcRecv(s, buffer, sizeof(T32));
        if (buffer[0].integer == NET_ACK) {
            remoteChannels->push(rChannel);
            declaredRemoteChannels->insert(rChannel->name, rChannel);
        }
        else {
            error(CODE_BUG, "Remote channel initialization failed\n");
            delete rChannel;
            return FALSE;
        }
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	sendBucketDataChannels
// Description			:	Send all bucket-data channels
// Return Value			:
// Comments				:	called from render loop (reyes.cpp or raytracer.cpp)
void CRenderer::sendBucketDataChannels(int x, int y) {
    unsigned int numChannelsToSend = remoteChannels->numItems;
    CRemoteChannel **channels = remoteChannels->array;
    T32 buffer[2];

    for (unsigned int i = 0; i < numChannelsToSend; i++) {
        if (channels[i] == NULL)
            continue; // it's an errored-out channel

        if (channels[i]->flags & REMOTECHANNEL_PERBUCKET) {
            // Request update and send channel index
            buffer[0].integer = NET_ACK;
            buffer[1].integer = channels[i]->remoteId;
            rcSend(netClient, buffer, 2 * sizeof(T32));
            rcRecv(netClient, buffer, 1 * sizeof(T32)); // get response back

            if (buffer[0].integer == NET_ACK) {
                if (channels[i]->sendRemoteBucket(netClient, x, y) == FALSE) {
                    error(CODE_BUG, "Remote channel communication error\n");

                    CRemoteChannel *val;
                    declaredRemoteChannels->erase(channels[i]->name, val);
                    delete channels[i];
                    channels[i] = NULL;
                }
            }
            else {
                error(CODE_BUG, "Client refused update for remote channel\n");

                CRemoteChannel *val;
                declaredRemoteChannels->erase(channels[i]->name, val);
                delete channels[i];
                channels[i] = NULL;
            }
        }
    }
    buffer[0].integer = NET_NACK;
    rcSend(netClient, buffer, 2 * sizeof(T32));
    rcRecv(netClient, buffer, 1 * sizeof(T32)); // get response back
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	recvBucketDataChannels
// Description			:	Recieve all bucket-data channels
// Return Value			:
// Comments				:	Each channel update is preceeded by identified
//							which was assigned when creating it
void CRenderer::recvBucketDataChannels(SOCKET s, int x, int y) {
    unsigned int numKnownChannels = remoteChannels->numItems;
    CRemoteChannel **channels = remoteChannels->array;
    T32 buffer[2];

    while (TRUE) {
        // receive update request - buffer[0] tells us
        // if there are any pending updates
        rcRecv(s, buffer, 2 * sizeof(T32));

        if (buffer[0].integer == NET_ACK) {
            // if we have updates then figure out what channel index
            unsigned int remoteId = (unsigned int)buffer[1].integer;

            if ((remoteId < numKnownChannels) && (channels[remoteId] != NULL)) {
                // Accept the update
                buffer[0].integer = NET_ACK;
                rcSend(s, buffer, 1 * sizeof(T32));
                // Perform it
                if (channels[remoteId]->recvRemoteBucket(s, x, y) == FALSE) {
                    error(CODE_BUG, "Remote channel communication error\n");
                }
            }
            else {
                error(CODE_BUG, "Update received for unknown remote channel\n");
                buffer[0].integer = NET_NACK;
                rcSend(s, buffer, 1 * sizeof(T32));
            }
        }
        else {
            // No pending updates - we are done
            break;
        }
    }
    buffer[0].integer = NET_ACK;
    rcSend(s, buffer, 1 * sizeof(T32));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	sendFrameDataChannels
// Description			:	Send all frame-data channels
// Return Value			:
// Comments				:	called from render loop (reyes.cpp or raytracer.cpp)
void CRenderer::sendFrameDataChannels() {
    unsigned int numChannelsToSend = remoteChannels->numItems;
    CRemoteChannel **channels = remoteChannels->array;
    T32 buffer[2];

    for (unsigned int i = 0; i < numChannelsToSend; i++) {
        if (channels[i] == NULL)
            continue; // it's an errored-out channel

        if (channels[i]->flags & REMOTECHANNEL_PERFRAME) {
            // Request update and send channel index
            buffer[0].integer = NET_ACK;
            buffer[1].integer = channels[i]->remoteId;
            rcSend(netClient, buffer, 2 * sizeof(T32));
            rcRecv(netClient, buffer, 1 * sizeof(T32)); // get response back

            if (buffer[0].integer == NET_ACK) {
                if (channels[i]->sendRemoteFrame(netClient) == FALSE) {
                    error(CODE_BUG, "Remote channel communication error\n");

                    CRemoteChannel *val;
                    declaredRemoteChannels->erase(channels[i]->name, val);
                    delete channels[i];
                    channels[i] = NULL;
                }
            }
            else {
                error(CODE_BUG, "Client refused update for remote channel\n");

                CRemoteChannel *val;
                declaredRemoteChannels->erase(channels[i]->name, val);
                delete channels[i];
                channels[i] = NULL;
            }
        }
    }
    buffer[0].integer = NET_NACK;
    rcSend(netClient, buffer, 2 * sizeof(T32));
    rcRecv(netClient, buffer, 1 * sizeof(T32)); // get response back
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	recvFrameDataChannels
// Description			:	Recieve all frame-data channels
// Return Value			:
// Comments				:	Each channel update is preceeded by identified
//							which was assigned when creating it
void CRenderer::recvFrameDataChannels(SOCKET s) {
    unsigned int numKnownChannels = remoteChannels->numItems;
    CRemoteChannel **channels = remoteChannels->array;
    T32 buffer[2];

    while (TRUE) {
        // receive update request - buffer[0] tells us
        // if there are any pending updates
        rcRecv(s, buffer, 2 * sizeof(T32));

        if (buffer[0].integer == NET_ACK) {
            // if we have updates then figure out what channel index
            unsigned int remoteId = (unsigned int)buffer[1].integer;

            if ((remoteId < numKnownChannels) && (channels[remoteId] != NULL)) {
                // Accept the update
                buffer[0].integer = NET_ACK;
                rcSend(s, buffer, 1 * sizeof(T32));
                // Perform it
                if (channels[remoteId]->recvRemoteFrame(s) == FALSE) {
                    error(CODE_BUG, "Remote channel communication error\n");
                }
            }
            else {
                error(CODE_BUG, "Update received for unknown remote channel\n");
                buffer[0].integer = NET_NACK;
                rcSend(s, buffer, 1 * sizeof(T32));
            }
        }
        else {
            // No pending updates - we are done
            break;
        }
    }
    buffer[0].integer = NET_ACK;
    rcSend(s, buffer, 1 * sizeof(T32));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteTSMChannel
// Method				:	ctor
// Description			:	-
// Comments				:	The file is opened elsewhere and closed elsewhere
CRemoteTSMChannel::CRemoteTSMChannel(const char *name, FILE *f, int *idx, int xb, int yb) : CRemoteChannel(name, REMOTECHANNEL_PERBUCKET, CHANNELTYPE_TSM) {
    tsmFile = f;
    index = idx;
    lastPosition = ftell(f);
    xBuckets = xb;
    yBuckets = yb;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteTSMChannel
// Method				:	sendRemoteBucket
// Description			:	send a buckets worth of tsm data
// Return Value			:	success or failure
// Comments				:
int CRemoteTSMChannel::sendRemoteBucket(SOCKET s, int, int) {
    // Record current position, seek back to tile start
    long curPos = ftell(tsmFile);
    fseek(tsmFile, lastPosition, SEEK_SET);
    long sz = curPos - lastPosition;
    rcSend(s, &sz, sizeof(uint64_t));

    // Reset the sz because we may have overwritten it
    sz = curPos - lastPosition;

    // Send the tile data
    char buf[NETWORK_BUFFER_LENGTH];
    while (sz > 0) {
        int nn = (int)((sz > (NETWORK_BUFFER_LENGTH)) ? (NETWORK_BUFFER_LENGTH) : sz);
        if (fread(buf, nn, 1, tsmFile) != 1) { /* read error */
        }
        rcSend(s, buf, nn, FALSE);
        sz -= nn;
    }

    // Overwrite the last position
    lastPosition = (long)curPos;

    // Seek back to the saved position (why is this necessary?)
    assert(ftell(tsmFile) == curPos);
    fseek(tsmFile, curPos, SEEK_SET);

    // Good to go
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteTSMChannel
// Method				:	recvRemoteBucket
// Description			:	receive a buckets worth of tsm data
// Return Value			:	success or failure
// Comments				:
int CRemoteTSMChannel::recvRemoteBucket(SOCKET s, int x, int y) {
    // record where we are
    uint64_t prevPos = ftell(tsmFile);

    // recieve data
    uint64_t sz;
    rcRecv(s, &sz, sizeof(uint64_t));
    char buf[NETWORK_BUFFER_LENGTH];
    while (sz > 0) {
        int nn = (int)((sz > (NETWORK_BUFFER_LENGTH)) ? (NETWORK_BUFFER_LENGTH) : sz);
        rcRecv(s, buf, nn, FALSE);
        fwrite(buf, nn, 1, tsmFile);
        sz -= nn;
    }

    // record the bucket start and bucket index
    uint64_t newPos = ftell(tsmFile);
    index[y * xBuckets + x] = (int)prevPos;
    index[y * xBuckets + x + xBuckets * yBuckets] = (int)(newPos - prevPos);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteTSMChannel
// Method				:	ctor
// Description			:	-
// Comments				:	The cache is opened elsewhere and closed elsewhere
CRemoteICacheChannel::CRemoteICacheChannel(CIrradianceCache *c) : CRemoteChannel(c->name, REMOTECHANNEL_PERFRAME, CHANNELTYPE_ICACHE) {
    cache = c;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteICacheChannel
// Method				:	sendRemoteBucket
// Description			:	send all the cache's data
// Return Value			:	success or failure
// Comments				:
int CRemoteICacheChannel::sendRemoteFrame(SOCKET s) {
    // The cache hands over its nodes in the order its tree is walked (the root first, a node's children pushed 0 to 7 and
    // popped last in, first out): for each node that has records, their number and then each record as it lies in memory
    cache->forEachNode([&](const CIrradianceCache::CCacheSample *head) {
        const CIrradianceCache::CCacheSample *cSample;
        int numSamples;

        // Send the values in this level (if any)
        for (numSamples = 0, cSample = head; cSample != NULL; cSample = cSample->next)
            numSamples++;

        if (numSamples != 0) {
            rcSend(s, &numSamples, sizeof(int), FALSE);

            for (cSample = head; cSample != NULL; cSample = cSample->next) {
                rcSend(s, cSample, sizeof(CIrradianceCache::CCacheSample), FALSE);
            }
        }
    });

    const int numSamples = 0;
    rcSend(s, &numSamples, sizeof(int), FALSE);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteICacheChannel
// Method				:	recvRemoteBucket
// Description			:	receive all the server's cache data
// Return Value			:	success or failure
// Comments				:
int CRemoteICacheChannel::recvRemoteFrame(SOCKET s) {
    int numSamples;

    rcRecv(s, &numSamples, sizeof(int), FALSE);

    while (numSamples > 0) {
        // The batch lands in a buffer; the cache copies each record into its own memory and places it
        std::vector<CIrradianceCache::CCacheSample> batch((size_t)numSamples);

        rcRecv(s, batch.data(), numSamples * sizeof(CIrradianceCache::CCacheSample), FALSE);
        for (int t = 0; t < numSamples; t++) {
            batch[t].next = NULL;
        }

        // GSHTODO filter thru samples and discard this one iff within eps of another
        for (int t = 0; t < numSamples; t++) {
            cache->insertReceived(batch[t]);
        }

        rcRecv(s, &numSamples, sizeof(int), FALSE);
    }

    return TRUE;
}

// Help us set up the client cache (it's worldBound will be wrong)
int CRemoteICacheChannel::sendSetupData(SOCKET s) {
    const float side = cache->rootSide();

    rcSend(s, cache->rootCenter(), sizeof(vector), FALSE);
    rcSend(s, &side, sizeof(float), FALSE);

    return TRUE;
}

int CRemoteICacheChannel::setup(SOCKET s) {
    vector center;
    float side;

    rcRecv(s, center, sizeof(vector), FALSE);
    rcRecv(s, &side, sizeof(float), FALSE);
    cache->setRoot(center, side);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemotePtCloudChannel
// Method				:	ctor
// Description			:	-
// Comments				:	The cache is opened elsewhere and closed elsewhere
CRemotePtCloudChannel::CRemotePtCloudChannel(CPointCloud *c) : CRemoteChannel(c->name, REMOTECHANNEL_PERFRAME, CHANNELTYPE_PTCLOUD) {
    cloud = c;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteICacheChannel
// Method				:	sendRemoteBucket
// Description			:	send all the cache's data
// Return Value			:	success or failure
// Comments				:
int CRemotePtCloudChannel::sendRemoteFrame(SOCKET s) {

    // Send the number of items
    const int numPoints = cloud->getNumPoints();
    rcSend(s, &numPoints, sizeof(int), FALSE);

    // Send the points
    rcSend(s, cloud->points(), sizeof(CPointCloudPoint) * numPoints, FALSE);

    // Send the data
    rcSend(s, cloud->pointData(), sizeof(float) * cloud->dataSize * numPoints, FALSE);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemoteICacheChannel
// Method				:	recvRemoteBucket
// Description			:	receive all the server's cache data
// Return Value			:	success or failure
// Comments				:
int CRemotePtCloudChannel::recvRemoteFrame(SOCKET s) {
    int i, numItems;

    // recive the points
    rcRecv(s, &numItems, sizeof(int), FALSE);

    // Allocate some temp memory
    CPointCloudPoint *items = new CPointCloudPoint[numItems];
    float *data = new float[cloud->dataSize * numItems];

    // Receive the points
    rcRecv(s, items, sizeof(CPointCloudPoint) * numItems, FALSE);

    // Receive the data
    rcRecv(s, data, sizeof(float) * cloud->dataSize * numItems, FALSE);

    // Store the data
    for (i = 0; i < numItems; i++)
        cloud->store(data + items[i].entryNumber, items[i].P, items[i].N, items[i].dP);

    // Delete the data
    delete[] items;
    delete[] data;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemotePtCloudChannel
// Method				:	sendSetupData
// Description			:	sendSetupData
// Return Value			:	success or failure
// Comments				:
int CRemotePtCloudChannel::sendSetupData(SOCKET s) {
    char channelDef[1024];
    char *ptr = channelDef;

    // encode the channel (tidy this up)
    channelDef[0] = '\0';
    int i = 0;
    while (i < (cloud->numChannels() - 1)) {
        snprintf(ptr, sizeof(channelDef) - (size_t)(ptr - channelDef), "%s,", cloud->channelName(i));
        ptr += strlen(cloud->channelName(i)) + 1;
        i++;
    }
    strcpy(ptr, cloud->channelName(cloud->numChannels() - 1));

    rcSend(s, channelDef, 1024, FALSE);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRemotePtCloudChannel
// Method				:	setup
// Description			:	setup
// Return Value			:	success or failure
// Comments				:
int CRemotePtCloudChannel::setup(SOCKET s) {
    char channelDef[1024];

    // get the channels
    rcRecv(s, channelDef, 1024, FALSE);
    cloud->defineChannels(channelDef);

    matrix identity;
    identitym(identity);
    cloud->setTransform(identity, identity);

    // set the scale to 1

    cloud->setDPScale(1);

    return TRUE;
}
