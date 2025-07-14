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
  PKG_CUSTOM,
  PKG_END
};

struct package {
  void *d;
  enum packageType t;
};

DEF_VEC(struct package, pkgv);
DEF_VEC(alpm_pkg_t*, alpmpkgv);

struct depEntry { char *key; struct pkgv *value; };
struct pacEntry { char *key; alpm_pkg_t *value; };
struct strEntry { char *key; char *value; };

void pacman_set_cenv(struct cenv *_ce);
void pacman_read_config();
void pacman_install();

void pacman_require(const char *pname, uint8_t base);
int32_t aur_require(char *pname);

void pacman_init();
void pacman_freedb();

#endif

