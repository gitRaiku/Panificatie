#include <alpm.h>
#include "config.h"
#include "util.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

alpm_handle_t *alpm;

struct packageEntry { char *key; alpm_pkg_t *value; } *requiredPackages;
enum packageType {
  PKG_ALPM,
  PKG_AUR,
  PKG_GIT,
  PKG_CUSTOM
}

struct package {
  union { alpm_pkg_t *a, void *v } d;
  enum packageType t;
}
DEF_VEC(struct package, pkgv);

struct packageEntry { char *key; struct pkgv *value; } *requiredPackages;

void print_pkg(alpm_handle_t *alpm, alpm_pkg_t *pkg) {
  fprintf(stdout, "%s\n", alpm_pkg_get_name(pkg));
  alpmforeach(alpm_pkg_get_depends(pkg), i) {
    fprintf(stdout, "  %s\n", ((alpm_depend_t*)i->data)->name);
  }
  fprintf(stdout, "\n");
}

char *packages[] = {
  //"base", "base-devel", "fish", "neovim", "foot"
};
alpm_list_t *req_packages, *dblist;

alpm_pkg_t *find_package(char *pname) {
  alpmforeach(dblist, db) {
    alpm_pkg_t *pkg = alpm_db_get_pkg(db->data, pname);
    if(pkg != NULL) { return pkg; }
  }
  return NULL;
}

void require_package(char *pname) {
  static uint32_t indentLevel = -1;
  indentLevel += 1;
  alpm_pkg_t *pkg = find_package(pname);
  if (pkg == NULL) { fprintf(stderr, "Could not find package %s, skipping!\n", pname); goto end; }

  if (shgeti(requiredPackages, pname) < 0) {
    for(uint32_t i = 0; i < indentLevel; ++i) { fprintf(stdout, "  "); } fprintf(stdout, "Added %s\n", pname);
    shput(requiredPackages, pname, pkg);
    alpmforeach(alpm_pkg_get_depends(pkg), pk) {
      require_package(((alpm_depend_t*)pk->data)->name);
    }
  }
end:;
  indentLevel -= 1;
}

void init_alpm() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

  alpm_register_syncdb(alpm, "core", 0);
  alpm_register_syncdb(alpm, "extra", 0);
  alpm_register_syncdb(alpm, "multilib", 0);

  dblist = alpm_get_syncdbs(alpm);

  svecforeach(packages, char*, pkg) {
    require_package(*pkg);
  }
  return 0;

  alpm_list_t *packages = NULL;
  alpmforeach(dblist, i) {
    packages = alpm_list_join(packages, alpm_list_copy(alpm_db_get_pkgcache(i->data)));
  }

  alpmforeach(packages, i) {
    print_pkg(alpm, i->data);
  }
  alpm_list_free(packages);

  pacman_free_conf(pc);
  ENEG(alpm_release(alpm), "Could not release alpm!");
}


