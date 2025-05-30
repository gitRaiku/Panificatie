#include "package.h"

alpm_handle_t *alpm;
alpm_list_t *dblist;

char *packages[] = {
  //"base", "base-devel", "fish", "neovim", "foot"
  "tpm2-tss"
};

struct packageEntry *pkgdb = NULL;

struct packageEntry *requiredPackages;

void print_pkg(alpm_handle_t *alpm, alpm_pkg_t *pkg) {
  fprintf(stdout, "%s\n", alpm_pkg_get_name(pkg));
  alpmforeach(alpm_pkg_get_depends(pkg), i) {
    fprintf(stdout, "  %s\n", ((alpm_depend_t*)i->data)->name);
  }
  fprintf(stdout, "\n");
}

alpm_pkg_t *find_package(char *pname) {
  alpmforeach(dblist, db) {
    alpm_pkg_t *pkg = alpm_db_get_pkg(db->data, pname);
    if(pkg != NULL) { return pkg; }
  }
  return NULL;
}

void require_package(char *pname) {
  /*
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
  */
}

void push_pkg(const char *name, struct package pkg) {
  int gi = shgeti(pkgdb, name);
  if (gi < 0) {
    struct pkgv *cv; vecsi(pkgv, cv);
    vecp(cv, pkg);
    shput(pkgdb, name, cv);
  } else {
    vecp(pkgdb[gi].value, pkg);
  }
}

void insert_pkg(alpm_pkg_t *pkg) {
  fprintf(stdout, "Provides: %s\n", alpm_pkg_get_name(pkg));
  push_pkg(alpm_pkg_get_name(pkg), (struct package){pkg, PKG_ALPM});
  alpmforeach(alpm_pkg_get_provides(pkg), cp) {
    fprintf(stdout, "Provides: %s\n", alpm_pkg_get_name(cp->data));
    push_pkg(alpm_pkg_get_name(cp->data), (struct package) { cp->data, PKG_ALPM });
  }
}

void init_packagedb() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

  svecforeach(pacman_repositories, const char*, repo) { alpm_register_syncdb(alpm, *repo, 0); }
  dblist = alpm_get_syncdbs(alpm);

  alpmforeach(dblist, i) {
    alpmforeach(alpm_db_get_pkgcache(i->data), j) {
      insert_pkg(j->data);
    }
  }

  return;

  svecforeach(packages, char*, pkg) {
    require_package(*pkg);
  }

  alpm_list_t *packages = NULL;

  alpmforeach(packages, i) {
    print_pkg(alpm, i->data);
  }
  alpm_list_free(packages);

  ENEG(alpm_release(alpm), "Could not release alpm!");
}


