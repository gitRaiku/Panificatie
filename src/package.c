#include "package.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

alpm_handle_t *alpm;
alpm_list_t *dblist;

char *packages[] = {
  //"base", "base-devel", "fish", "neovim", "foot"
  "tpm2-tss"
};

struct depEntry *pkgdb = NULL;
alpm_list_t *requiredPackages = NULL; /// I absolutely hate the fact searching in this is linear
                                     /// But it shouldn't be an actual problem
                                     /// (Especially when comparing speed with nix)

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
  /// TODO: Handle replaces as well
  alpm_pkg_t *pkg = find_package(pname);
  if (pkg == NULL) { fprintf(stderr, "Could not find package %s, skipping!\n", pname); return; }

  alpm_list_add(requiredPackages, pkg);
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
  //fprintf(stdout, "Package: %s\n", alpm_pkg_get_name(pkg));
  push_pkg(alpm_pkg_get_name(pkg), (struct package){pkg, PKG_ALPM});
  alpmforeach(alpm_pkg_get_provides(pkg), cp) {
    if (strcmp(((alpm_depend_t*)cp->data)->name, alpm_pkg_get_name(pkg))) {
      //fprintf(stdout, "  Provides: %s\n", ((alpm_depend_t*)cp->data)->name);
      push_pkg(((alpm_depend_t*)cp->data)->name, (struct package) {pkg, PKG_ALPM});
    }
  }
}

void init_packagedb() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

  svecforeach(pacman_repositories, const char* const, repo) { 
    //fprintf(stdout, "Registering %s\n", *repo);
    alpm_register_syncdb(alpm, *repo, 0); 
  }
  dblist = alpm_get_syncdbs(alpm);

  alpmforeach(dblist, i) {
    alpmforeach(alpm_db_get_pkgcache(i->data), j) {
      insert_pkg(j->data);
    }
  }
}

void expand_and_check_pacman(struct strv *pkgs) {
  vecforeach(pkgs, char*, pname) {
    require_package(*pname);
  }
  alpmforeach(requiredPackages, pkg) {
    fprintf(stdout, "Final package: %s\n", alpm_pkg_get_name(((alpm_pkg_t*)pkg->data)));
  }
  alpm_list_t *conflicts = alpm_checkconflicts(alpm, requiredPackages);
  alpmforeach(conflicts, conf) { /// TODO: Print conflict waterfall
    fprintf(stderr, "Package %s conflicts with %s due to %s! Aborting!", 
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package1),
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package2),
        alpm_dep_compute_string(((alpm_conflict_t*)conf->data)->reason));
  }
}

void free_package(struct package *pkg) {}

void free_packagedb() {
  size_t np = shlenu(pkgdb);
  for(uint32_t i = 0; i < np; ++i) {
    vecforeach(pkgdb[i].value, struct package, pkg) { free_package(pkg); }
    vecfree(pkgdb[i].value);
  }

  shfree(pkgdb);
  ENEG(alpm_release(alpm), "Could not release alpm!");
}
