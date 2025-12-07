/**
 * Project: Pixie
 *
 * File: statView.h
 *
 * Description:
 *   This file defines the interface for statView.
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
//  File				:	statView.h
//  Classes				:	CStatView
//  Description			:	This class visualizes the statistics during the view
//
////////////////////////////////////////////////////////////////////////
#ifndef STATVIEW_H
#define STATVIEW_H

#include <QDialog>
#include <QTabWidget>

#include "common/global.h"
#include "ri/stats.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CTimeMemory
// Description			:	Holds info about time and memory
// Comments				:
class CTimeMemory : public QWidget {
        Q_OBJECT
    public:
        CTimeMemory(QWidget *parent = 0) {}

        void update(CStatistics *stats);

        QLabel memory;
        QLabel startTime;
        QLabel overhead;
        QLabel renderTime;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CNumbers
// Description			:	Numbers of various items
// Comments				:
class CNumbers : public QWidget {
        Q_OBJECT
    public:
        CNumbers(QWidget *parent = 0) {}

        void update(CStatistics *stats);

        QLabel numAttributes;
        QLabel numXforms;
        QLabel numOptions;
        QLabel numShaders;
        QLabel numShaderInstances;
        QLabel numOutDevices;
        QLabel numGprimCores;
        QLabel numVariableDatas;
        QLabel numVertexDatas;
        QLabel numParameters;
        QLabel numParameterLists;
        QLabel numPls;
        QLabel numObjects;
        QLabel numGprims;
        QLabel numSurfaces;
        QLabel numPeakSurfaces;
        QLabel numComposites;
        QLabel numDelayeds;
        QLabel numInstances;
        QLabel numTextures;
        QLabel numEnvironments;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CStatView
// Description			:	This is the main statistics view class
// Comments				:
class CStatView : public QDialog {
        Q_OBJECT
    public:
        CStatView(CStatistics *stats, QWidget *parent = 0);

    private:
        CTimeMemory *timeMemory; // Time and memory usage information
        CStatistics *stats;
};

#endif
