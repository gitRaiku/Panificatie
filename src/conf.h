#ifndef CONF_H
#define CONF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include "util.h"
#include "config.h"

struct panix_conf {
  struct strv *repos;
};

struct panix_conf *panix_read_conf(char *fname);
void panix_free_conf(struct panix_conf *conf);

#endif
