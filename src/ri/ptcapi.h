/**
 * Project: Pixie
 *
 * File: ptcapi.h
 *
 * Description:
 *   This file defines the interface for ptcapi.
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
//  File				:	ptcapi.h
//  Classes				:
//  Description			:	Point Cloud API
//
////////////////////////////////////////////////////////////////////////
#ifndef _PTCAPI_H_
#define _PTCAPI_H_

#ifndef LIB_EXPORT
#ifdef _WINDOWS
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT extern
#endif
#endif

typedef void *PtcPointCloud;

#ifdef __cplusplus
extern "C" {
#endif

// Create a blank point cloud with nvars channels
LIB_EXPORT PtcPointCloud PtcCreatePointCloudFile(char *filename, int nvars, const char **vartypes, const char **varnames, float *world2eye, float *world2ndc, float *format);

// Write a point to the file
LIB_EXPORT void PtcWriteDataPoint(PtcPointCloud pointcloud, float *point, float *normal, float radius, float *data);

// Finish an close the file
LIB_EXPORT void PtcFinishPointCloudFile(PtcPointCloud pointcloud);

// Open an existing point cloud
LIB_EXPORT PtcPointCloud PtcOpenPointCloudFile(char *filename, int *nvars, char **vartypes, char **varnames);

// Get info on the point cloud
LIB_EXPORT int PtcGetPointCloudInfo(PtcPointCloud pointcloud, const char *request, void *result);

// Read a single point
LIB_EXPORT int PtcReadDataPoint(PtcPointCloud pointcloud, float *point, float *normal, float *radius, float *data);

// Close the file
LIB_EXPORT void PtcClosePointCloudFile(PtcPointCloud pointcloud);

#ifdef __cplusplus
};
#endif

#endif //_PTCAPI_H_
