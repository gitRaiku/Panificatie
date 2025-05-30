#ifndef PACKAGE_H
#define PACKAGE_H

#include <alpm.h>
#include "config.h"
#include "util.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

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

struct packageEntry { char *key; struct pkgv *value; };


#endif

