/*
 *  orender.cpp
 *  openRender
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Sat Mar 20 2004
 *
 *  Original Development:
 *    (C) 2004 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software, you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: orender.cpp,v 1.6 2008/07/18 17:17:50 juvenal.silva Exp $
 */

// C includes
#include <math.h>

// C++ includes
#include <string>
#include <iostream>
#include <iomanip>

// Private includes
#include "orender.h"

// implementation of member functions
oRender::oRender() {
    
}

void oRender::prepareOptions(int argc, char *argv[]) {
    std::cout << "Numero de argumentos: " << argc << std::endl;
    std::cout << "Argumentos enviados: " << argv << std::endl;
}

void oRender::printHeader() {
    
}

void oRender::printUsage() {
    
}

void oRender::printVersion() {
    
}

int oRender::processRequest(int argc, char *argv[]) {
    this->prepareOptions(argc, argv);

    return 0;
}

oRender::~oRender() {
    
}

// Main execution for orender command.
int main(int argc, char *argv[]) {
    oRender *orender = new oRender();
    return orender->processRequest(argc, argv);
}
