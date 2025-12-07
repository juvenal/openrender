/**
 * Project: Pixie
 *
 * File: statView.cpp
 *
 * Description:
 *   This file implements the functionality for statView.
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
//  File				:	statView.cpp
//  Classes				:	CStatView
//  Description			:	This class visualizes the statistics during the view
//
////////////////////////////////////////////////////////////////////////
#include <QtGui>

#include "statView.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CStatView
// Method				:	CStatView
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CStatView::CStatView(CStatistics *s, QWidget *parent) : QDialog(parent) {

    // Create the tab widget
    stats = s;
    tabWidget = new CTabWidget;
    tabWidget->addTab(new QTabWidget(stats), tr("General"));

    // Create the buttons
    QPushButton *okButton = new QPushButton(tr("OK"));
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

    // Create the layouts
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);

    setWindowTitle(tr("Statistics"));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTimeMemory
// Method				:	update
// Description			:	Update the tabs
// Return Value			:	-
// Comments				:
void CTimeMemory::update(CStatistics *stats) {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CNumbers
// Method				:	update
// Description			:	Update the tabs
// Return Value			:	-
// Comments				:
void CNumbers::update(CStatistics *stats) {
    QGridLayout *layout = new QGridLayout;

    layout->addWidget(new QLabel("Num. Attributes"), 1, 1);
    layout->addWidget(&numAttributes, 1, 2);

    layout->addWidget(new QLabel("Num. Options"), 1, 1);
    layout->addWidget(&numOptions, 1, 2);

    setLayout(layout);
}
