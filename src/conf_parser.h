#ifndef CONF_PARSER_H
#define CONF_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "util.h"

struct panix_config *parse_config(char *fname);

struct panix_config {
  struct strv *pacmanPkgs;

  char curstr[128];
};

#endif
