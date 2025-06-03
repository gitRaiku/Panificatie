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

void db_push_pkg(const char *name, struct package pkg) {
  int gi = shgeti(pkgdb, name);
  if (gi < 0) {
    struct pkgv *cv; vecsi(pkgv, cv);
    vecp(cv, pkg);
    shput(pkgdb, name, cv);
  } else { vecp(pkgdb[gi].value, pkg); }
}

void db_insert_pkg(alpm_pkg_t *pkg) {
  db_push_pkg(alpm_pkg_get_name(pkg), (struct package){pkg, PKG_ALPM});
  alpmforeach(alpm_pkg_get_provides(pkg), cp) {
    if (strcmp(((alpm_depend_t*)cp->data)->name, alpm_pkg_get_name(pkg))) {
      db_push_pkg(((alpm_depend_t*)cp->data)->name, (struct package) {pkg, PKG_ALPM});
    }
  }
}

void init_packagedb() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

  //alpm_option_set_logcb(alpm, alpm_log_callback, NULL);

  svecforeach(pacman_repositories, const char* const, repo) { 
    fprintf(stdout, "Registering %s\n", *repo);
    alpm_register_syncdb(alpm, *repo, 0); 
  }
  dblist = alpm_get_syncdbs(alpm);

  alpmforeach(dblist, i) {
    svecforeach(pacman_mirrors, const char* const, mirror) { 
      fprintf(stdout, "Adding mirror %s!\n", *mirror);
      alpm_db_add_server(i->data, *mirror);
    }
    alpmforeach(alpm_db_get_pkgcache(i->data), j) {
      db_insert_pkg(j->data);
    }
  }
}

alpm_pkg_t *find_package(char *pname, uint8_t firstRun) {
  if (shgeti(pkgdb, pname) >= 0) {
    struct pkgv *cp = shget(pkgdb, pname);
    if (cp->l > 1) { 
      if (!firstRun) {
        fprintf(stderr, "Package %s has multiple providers:", pname);
        vecforeach(cp, struct package, pkg) { fprintf(stderr, " %s", alpm_pkg_get_name(pkg->d)); }
        fprintf(stderr, "\nPlease pick one inside the configuration file!\n");
      }
      return NULL;
    } else {
      return cp->v[0].d;
    }
  } else {
    return NULL;
  }
}

/// TODO: Improve error checking and add it to all functions
void require_package(char *pname, uint8_t firstRun) {
  /// TODO: Handle replaces as well
  alpm_pkg_t *pkg = find_package(pname, firstRun);
  if (pkg == NULL) { fprintf(stderr, "Could not find package %s, skipping!\n", pname); return; }

  if (!firstRun) {
    requiredPackages = alpm_list_add(requiredPackages, pkg);
  }
}

void alpm_log_callback(void *ctx, alpm_loglevel_t level, const char *fmt, va_list args) {char logf[256];snprintf(logf, 256, "Log: %u %s", level, fmt);vfprintf(stdout, logf, args);}
#define EALPM(cmd, res, args...) if ((cmd) < 0) { fprintf(stderr, res ": %s!\n", alpm_strerror(alpm_errno(alpm)), ##args); alpm_trans_release(alpm); alpm_release(alpm); exit(1); }
void transflag_pacman() { // Pacman supports trans rights
  EALPM(alpm_trans_init(alpm, /*ALPM_TRANS_FLAG_DOWNLOADONLY |*/ ALPM_TRANS_FLAG_NEEDED), "Could not init alpm transaction, are you running as root?"); // TODO: Make error messages better
  alpmforeach(requiredPackages, pkg) { EALPM(alpm_add_pkg(alpm, pkg->data), "Could not add package %s to the transaction", alpm_pkg_get_name(pkg->data)); }
  alpm_list_t *errlist;
  if (alpm_trans_prepare(alpm, &errlist) < 0) {
    fprintf(stderr, "Could not prepare the pacman transaction: %s!\n", alpm_strerror(alpm_errno(alpm)));
    alpmforeach(errlist, dep) {
      fprintf(stderr, "  [target:%s] [depend:%s] [causingpkg:%s]!\n",  /// TODO: Cleanup
          ((alpm_depmissing_t*)dep->data)->target,
          ((alpm_depmissing_t*)dep->data)->depend->name,
          ((alpm_depmissing_t*)dep->data)->causingpkg
          );
    }
    alpm_trans_release(alpm);
    alpm_release(alpm);
    exit(1);
  }
  /*
  if (alpm_trans_commit(alpm, &errlist) < 0) {
    fprintf(stderr, "Could not commit the pacman transaction: %s!\n", alpm_strerror(alpm_errno(alpm)));
    alpmforeach(errlist, err) {
      fprintf(stderr, "  fileconflict: [target:%s] [type:%u] [file:%s] [ctarget:%s]!\n",  /// TODO: Cleanup
          ((alpm_fileconflict_t*)err->data)->target,
          ((alpm_fileconflict_t*)err->data)->type,
          ((alpm_fileconflict_t*)err->data)->file,
          ((alpm_fileconflict_t*)err->data)->ctarget);
    }
    alpm_trans_release(alpm);
    alpm_release(alpm);
    exit(1);
  }
  */
  EALPM(alpm_trans_release(alpm), "Could not release the pacman transaction!\n");
}

void install_pacman(struct strv *pkgs) {
  vecforeach(pkgs, char*, pname) { require_package(*pname, 1); } /// First pass is to resolve multiple
  vecforeach(pkgs, char*, pname) { require_package(*pname, 0); } /// Providers for the same dependency
  alpmforeach(requiredPackages, pkg) {
    fprintf(stdout, "Final package: %s\n", alpm_pkg_get_name(((alpm_pkg_t*)pkg->data)));
  }
  alpm_list_t *conflicts = alpm_checkconflicts(alpm, requiredPackages);
  alpmforeach(conflicts, conf) { /// TODO: Print conflict waterfall
                                 /// TODO: Deps are also checked in transaction, maybe better errors there
    fprintf(stderr, "Package %s conflicts with %s due to [%s]! Aborting!\n", 
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package1),
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package2),
        alpm_dep_compute_string(((alpm_conflict_t*)conf->data)->reason));
    exit(1);
  }

  transflag_pacman();
}

void free_package(struct package *pkg) { 
  switch (pkg->t) {
    default:
  }
}

void free_packagedb() {
  size_t np = shlenu(pkgdb);
  for(uint32_t i = 0; i < np; ++i) {
    vecforeach(pkgdb[i].value, struct package, pkg) { free_package(pkg); }
    vecfree(pkgdb[i].value);
  }

  shfree(pkgdb);
  ENEG(alpm_release(alpm), "Could not release alpm!");
}
