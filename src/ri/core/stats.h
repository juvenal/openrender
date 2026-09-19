/**
 * Project: openRender
 *
 * File: stats.h
 *
 * Description:
 *   This file defines the interface for stats.
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
//  File				:	stats.h
//  Classes				:	CStats
//  Description			:	Holds renderer statistics
//
////////////////////////////////////////////////////////////////////////
#ifndef STATS_H
#define STATS_H

#include "atomic.h"
#include "common/global.h" // The global header file

///////////////////////////////////////////////////////////////////////
// Class				:	CStats
// Description			:	Holds statistics
// Comments				:
class CStats {
    public:
        void reset();         // Reset all the stats
        void printStats(int); // Print the frame statistics
        void check();         // Check we have clean shutdown

        ///////////////////////////////////////////////////////////////////////////////
        //
        //	Global stats
        //
        ///////////////////////////////////////////////////////////////////////////////
        int zoneMemory;              // The current zone memory size
        int peakZoneMemory;          // The peak zone memeory size
        float rendererStartTime;     // The time when the renderer was started
        float rendererStartOverhead; // The time it took to initialize the renderer
        atomic_int32 numAttributes;  // The number of objects allocated of each type
        atomic_int32 numXforms;
        atomic_int32 numOptions;
        atomic_int32 numShaders;
        atomic_int32 numShaderInstances;
        atomic_int32 numVertexDatas;
        atomic_int32 numParameters;
        atomic_int32 numPls;
        atomic_int32 numObjects;
        atomic_int32 numGprims;
        atomic_int32 numSurfaces;
        int numPeakSurfaces;
        atomic_int32 numDelayeds;
        atomic_int32 numTextures;
        atomic_int32 numEnvironments;
        int textureMemory;
        int sequenceNumber;        // The sequence number
        int runningSequenceNumber; // The running sequence number
        int totalNetRecv;          // The total number of bytes received over the net
        int totalNetSend;          // The total number of bytes send over the net

        ///////////////////////////////////////////////////////////////////////////////
        //
        //	Frame stats
        //
        ///////////////////////////////////////////////////////////////////////////////
        float frameStartTime; // The time when we started rendering
        float frameTime;      // The current frame time
        float progress;       // The progress in the current frame

        int numShade;   // Number of times shade is called
        int numSampled; // The number of vertices that passed thru shade
        int numShaded;  // The number of vertices that ended up being shaded
        int numTracedRays;
        int numReflectionRays;
        int numTransmissionRays;
        int numGatherRays;
        int numPhotonRays;

        atomic_int32 numRasterGrids; // The following stats come from the CReyes
        atomic_int32 numRasterObjects;
        int numRasterGridsCreated;
        int numRasterVerticesCreated;
        int numRasterGridsShaded;
        int numRasterGridsRendered;
        int numRasterQuadsRendered;

        int numSplits; // The stats that come from CPatch
        int numVsplits, numUsplits, numUVsplits;

        atomic_int32 numBlobbies;            // The stats that come from RiBlobby (spec 015)
        atomic_int32 numBlobbyLeaves;        // Total primitive fields across all blobbies
        atomic_int32 numBlobbyFieldEvals;    // Field evaluations performed
        atomic_int32 numBlobbyWeightedEvals; // Of those, the ones that also produced per-leaf weights
        atomic_int32 numBlobbyCellsVisited;  // Cells the continuation walk examined
        atomic_int32 numBlobbySurfaceCells;  // Cells that actually straddled the surface
        int numBlobbyLatticeCells;           // Cells a dense grid over the extent would have had
        atomic_int32 numBlobbyTriangles;     // Triangles emitted

        int numTextureMisses;                   // The number of texture misses
        int transferredTextureData;             // The amount the texture data transmitted
        int textureSize;                        // The current amount of textures in the memory
        int numPeakTextures;                    // The peak number of textures
        int numPeakEnvironments;                // The peak number of environments
        int peakTextureSize;                    // The amount of memory at the peak denoted to textures
        int numIndirectDiffuseSamples;          // The number of final gather samples taken
        int numOcclusionSamples;                // The number of final gather samples taken
        int numIndirectDiffuseRays;             // The number of final gather samples taken
        int numOcclusionRays;                   // The number of final gather samples taken
        int numIndirectDiffusePhotonmapLookups; // The number of final gather photonmap lookups
        atomic_int32 numBrickmapLookups;        // The number of brickmap lookups
        atomic_int32 numBrickmapCacheHits;      // The number of brickmap cache hits
        int numBrickmapCachePageouts;           // The number of bricks paged out
        atomic_int32 numBrickmapCachePageins;   // The number of bricks paged in
        int brickmapPeakMem;                    // The peak memory usage for brickmaps
        int tesselationMemory;                  // The total memory usage for tesselations
        int tesselationPeakMemory;              // The peak total memory usage for tesselations
        atomic_int32 tesselationCacheMisses;    // The number of tesselation cache misses
        atomic_int32 tesselationCacheHits;      // The number of tesselation cache hits
        int tesselationOverhead;                // The memory overhead of tesselation patches
};

extern CStats stats;

#endif
