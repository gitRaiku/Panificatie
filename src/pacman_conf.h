#ifndef CONF_H
#define CONF_H

#include "util.h"
#include "config.h"

struct pacman_conf {
  struct strv *repos;
};

struct pacman_conf *pacman_read_conf(char *fname);
void pacman_free_conf(struct pacman_conf *conf);

#endif
