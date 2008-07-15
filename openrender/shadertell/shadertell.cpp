/*
 *  shadertell.cpp - Read and report the parameters of a compiled shader (.slo)
 *  openRender
 *
 *  Description:
 *    This application scans the object shader folders to find the given shader
 *    object (.slo), and once found, it gives a description of it's kind and
 *    parameters.
 *
 *  Creation:
 *    Tue Oct 22 2002
 *
 *  Original Development:
 *    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: shadertell.cpp,v 1.4 2008/07/15 03:24:58 juvenal.silva Exp $
 */

// Member function to print the program id
void slotell::identity() {
  std::cout << "\n\n";
  std::cout <<  << "  ";
}

// Member function to print an informative message
void slotell::inform(int exit_code) {
  this->identity();
  std::cerr << "Usage: " << slotell::progname << " [options] shadername(s)\n";
  std::cerr << "   options:\n";
  std::cerr << "         -h            Detailed help screen\n\n";
  std::cerr.flush();
  exit(exit_code);
}

// Member function to print message and abort operation
void slotell::abort(int exit_code, char *message) {
  std::cerr << message;
  std::cerr << "Try '" << slotell::progname << " -h' or '" <<
               slotell::progname << " --help' for more information.\n\n";
  std::cerr.flush();
  exit(exit_code);
}

// Main function
int main(int argc, char *argv[]) {
  // Define variables
  int     i;
  char    shadername[1024];
  slotell shader;

  // Initialize main object
  shader = new slotell(argv[0]);

  // Scan the options in the command line
  shader->inform("");
  
  // Only process if shaders given
  if (optind < argc) {
    // Only process the given shaders
    for (i = optind; argc > i; i++) {
      strcpy(shadername, argv[i]);
      shader->process(shadername);
    }
  }
  else {
    shader->abort(1, "Please give a shader name to process...\n");
  }
}
