//
//  shader.c - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Wed Oct 02 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: shader.c,v 1.2 2004/01/07 11:33:19 juvenal Exp $
//

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/file.h>
#include <strings.h>

// Some usefull defines
#define READ 0
#define WRITE 1
#define BUFFSIZE 1024

// External global variables
extern int   line;
extern int   yylineno;
extern char  cppFilename[BUFFSIZE];
extern int   debug;
extern void  yyparse();

// Global variables and constants
char output_file[BUFFSIZE];
char outfile[BUFFSIZE];
const char *progname;

// Function prototypes
void error ( char *msg);
void print_ident ( FILE *stream);
void print_usage ( FILE *stream, int exit_code);
void print_abort ( FILE *stream, int exit_code, char *message);

// Function to print identification
void
print_ident ( FILE *stream) {
  fprintf ( stream, "\n");
  fprintf ( stream, "shader - Shading Language Compiler\n\n");
  fprintf ( stream, "openRender 1.0 Release 1 linux (Adheres to the RenderMan Standard)\n");
  fprintf ( stream, "(c) Copyright 2002 Juvenal A. Silva Jr.  Portions (c) Ian Stephenson.\n");
  fprintf ( stream, "All Rights Reserved.\n\n");
  fprintf ( stream, "The RenderMan (R) Interface Procedures and RIB Protocol are:\n");
  fprintf ( stream, "Copyright 1988,1989, Pixar. All Rights Reserved.\n");
  fprintf ( stream, "RenderMan (R) is a registered trademark of Pixar.\n\n");
}

// Function to print usage options
void
print_usage ( FILE *stream, int exit_code) {
  print_ident ( stream);
  fprintf ( stream, "Usage: %s [options] file.sl\n", progname);
  fprintf ( stream, "   options:\n");
  fprintf ( stream, "         -h            Detailed help screen\n");
  fprintf ( stream, "         -Ipath        Specify a path for #include files\n");
  fprintf ( stream, "         -Dsym[=val]   Specify preprocessor symbol definition\n");
  fprintf ( stream, "         -Usym         Undefine a preprocessor symbol\n");
  fprintf ( stream, "         -o output     Specify alternative output filename\n");
  fprintf ( stream, "         -d            Add debug information\n");
  fprintf ( stream, "         -v            Verbose mode\n");
  fprintf ( stream, "         -q            Quiet mode\n");
  fprintf ( stream, "\n");
  exit ( exit_code);
}

// Function to print message and abort operation
void
print_abort ( FILE *stream, int exit_code, char *message) {
  fprintf ( stream, message);
  fprintf ( stream, "Try '%s -h' or '%s --help' for more information.\n\n",
                    progname, progname);
  exit ( exit_code);
}


// Main function of progname
int
main ( int argc, char *argv[]) {
  // Define the local variables
  char cpp[BUFFSIZE],
       next_option;
  int src, dest, i,
      filedes[2],
      long_index,
      arg_index,
      filename;

  // Set the command line options
  const char *short_options = "hdqvo:";
  const struct option long_options[] = {
                                         { "help",    0, NULL, 'h' },
                                         { "debug",   0, NULL, 'd' },
                                         { "quiet",   0, NULL, 'q' },
                                         { "verbose", 0, NULL, 'v' },
                                         { "output",  1, NULL, 'o' },
                                         {  NULL,     0, NULL,  0  }
                                       };

  // Set some default options before processing
  char input_file[BUFFSIZE];
  char *output_file = NULL;
  int   verbose     = 0;
  int   quiet       = 0;
//  int   debug       = 0;

  // Set the global constant
  progname = basename ( argv[0]);

  // Parse the options on the command line
  while ( ( next_option = getopt_long ( argc, argv, short_options,
                                        long_options, &long_index)) != -1) {
    switch ( next_option) {
        case 'h':              // help (-h or --help)
            print_usage ( stdout, 0);
            break;
        case 'd':              // debug (-d or --debug)
            debug = 1;
            break;
        case 'q':              // quiet mode (-q or --quiet)
            quiet   = 1;
            verbose = 0;
            break;
        case 'v':              // verbose mode (-v or --verbose)
            verbose = 1;
            quiet   = 0;
            break;
        case 'o':              // output file (-o or --output)
            output_file = optarg;
            break;
        default:               // invalid option (abort)
            print_abort ( stderr, 1, "");
    }
  }

  // Only process if there is a SL file
  if ( optind < argc) {
    // Get argument input file
    strcpy ( input_file, argv[optind]);
    // Set output_file as needed
    if ( output_file == NULL) {
      output_file = malloc ( BUFFSIZE);
      sprintf ( output_file, "%so", argv[optind]);
    }
  }
  else {
    print_abort ( stderr, 1, "Please give a SL file to compile...\n");
  }

//  // Print the status and exit
//  printf ( "Output file is: %s\n", output_file);
//  printf ( "Debug is: %d\n", debug);
//  printf ( "Verbose is: %d\n", verbose);
//  printf ( "Quiet is: %d\n", quiet);
//  for ( i = optind; i < optind + 1; i++) {
//    printf ( "Argument: %s\n", argv[i]);
//  }
//  exit ( 0);


//  filename = 1;
//  if ( argc <= filename) {
//    print_usage ( stdout, 0);
//  }

//  if ( strcmp ( argv[filename], "-d") == 0) {
//    filename++;
//    debug = 1;
//  }

//  if ( argc <= filename) {
//    print_usage ( stdout, 0);
//  }

  // Send a simple message to the screen
  fprintf ( stderr, "Compiling %s\n", input_file);

  // Set the pipe to the preprocessor
  if ( pipe ( filedes) != 0) {
    print_abort ( stderr, 2, "Failed on pipe creation!\n");
//    fprintf ( stderr, "pipe failed!\n");
//    exit ( -1);
  };
  src = filedes[READ];

  // Fork and adjust environment for preprocessor
  if ( fork () == 0) {
    close ( filedes[READ]);

    // Send stdout to the pipe
    close ( WRITE);
    if ( dup ( filedes[WRITE]) == -1) {
      fprintf ( stderr, "child dup failed!\n");
      exit ( -1);
    };

//    sprintf ( cpp, "cpp %s", argv[filename]);
    sprintf ( cpp, "cpp %s", input_file);
    if ( system ( cpp) != 0) {
      fprintf ( stderr, "child failed\n");
    }

    exit ( 0);
  }


  close ( filedes[WRITE]);

  close ( READ);
  if ( dup ( src) == -1) {
    fprintf ( stderr, "dup failed!\n");
    exit ( -1);
  };

  // Name the object file *.slo
  strcpy ( outfile, output_file);
//  sprintf ( outfile, "%so", argv[filename]);
  dest = open ( outfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if ( dest == -1) {
    fprintf ( stderr, "shader: can't open %s\n", outfile);
    exit ( -1);
  }
  close ( WRITE);
  if ( dup ( dest) == -1) {
    fprintf ( stderr, "child dup failed!\n");
    exit ( -1);
  };

  yyparse ();

  // Notify user
  fprintf ( stderr, "...compiled %s\n", outfile);
  return 0;
}

int
yywrap () {
  return ( 1) ;
}

void
yyerror () {
  error ( "parse error");
}

void
warning ( char *msg) {
  fprintf ( stderr, "shader warning: %s at line %d in %s\n",
                    msg, line, cppFilename);
}

void
error ( char *msg) {
  remove ( outfile);
  fprintf ( stderr, "shader error : %s at line %d in %s\n",
                    msg, line, cppFilename);
  exit ( -1);
}
