#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "client_optparse.h"

_Bool usage() {
  printf("valid arguments: -n (name), -h (hostname), -p (port)\n");
  return false;
}

/* Retrieves the size of the board
 * Prints instructions when user arguments are inconsistent */
_Bool optparse(int argc, char *argv[]) {
  int index;
  int c;

  opterr = 0;
  name = "";

  if(argc == 1) {
    usage();
  }

  while ((c = getopt (argc, argv, "h:n:p:")) != -1)
    switch (c) {
      case 'n':
        name = optarg;
        break;
      case 'h':
        hostname = optarg;
        break;
      case 'p':
        port = optarg;
        break;
      case '?':
        if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        return false;
      default:
        abort();
    }

  for (index = optind; index < argc; index++) {
    printf ("Non-option argument %s\n", argv[index]);
    usage();
  }

  if(strcmp(name, ""))
      usage();

  return true;
}
