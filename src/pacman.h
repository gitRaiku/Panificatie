#ifndef PACKAGE_H
#define PACKAGE_H

#include <alpm.h>
#include <unistd.h>
#include "config.h"
#include "conf_parser.h"
#include "util.h"
#include "stb_ds.h"

enum packageType {
  PKG_ALPM,
  PKG_ALPM_FIXED,
  PKG_ALPM_ADDED,
  PKG_AUR,
  PKG_GIT,
  PKG_CUSTOM
};

struct package {
  void *d;
  enum packageType t;
};

DEF_VEC(struct package, pkgv);

struct depEntry { char *key; struct pkgv *value; };
struct pacEntry { char *key; alpm_pkg_t *value; };

void pacman_set_cenv(struct cenv *_ce);
void pacman_install();

void pacman_initdb();
void pacman_freedb();

#endif

