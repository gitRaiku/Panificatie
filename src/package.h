#ifndef PACKAGE_H
#define PACKAGE_H

#include <alpm.h>
#include "config.h"
#include "util.h"

enum packageType {
  PKG_ALPM,
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
struct pacEntry { alpm_pkg_t *key; uint8_t value; };

void install_pacman(struct strv *pkgs);

void init_packagedb();
void free_packagedb();

#endif

