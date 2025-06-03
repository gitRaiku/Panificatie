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

  requiredPackages = alpm_list_add(requiredPackages, pkg);
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

#define EALPM(cmd, res, args...) if ((cmd) < 0) { fprintf(stderr, res ": %s!\n", alpm_strerror(alpm_errno(alpm)), ##args); exit(1); }
void transflag_pacman() { // Pacman supports trans rights
  EALPM(alpm_trans_init(alpm, ALPM_TRANS_FLAG_DOWNLOADONLY | ALPM_TRANS_FLAG_NEEDED), "Could not init alpm transaction")
  alpmforeach(requiredPackages, pkg) { EALPM(alpm_add_pkg(alpm, pkg->data), "Could not add package %s to the transaction", alpm_pkg_get_name(pkg->data)); }
  alpm_list_t *depmissing;
  if (alpm_trans_prepare(alpm, &depmissing) < 0) {
    alpmforeach(depmissing, dep) {
      fprintf(stderr, "Could not complete pacman transaction: [target:%s] [depend:%s] [causingpkg:%s]!\n",  /// TODO: Cleanup
          ((alpm_depmissing_t*)dep->data)->target,
          ((alpm_depmissing_t*)dep->data)->depend->name,
          ((alpm_depmissing_t*)dep->data)->causingpkg
          );
    }
    exit(1);
  }

  

       int alpm_trans_get_flags (alpm_handle_t *handle)
           Returns the bitfield of flags for the current
           transaction.
       alpm_list_t * alpm_trans_get_add (alpm_handle_t *handle)
           Returns a list of packages added by the transaction.
       alpm_list_t * alpm_trans_get_remove (alpm_handle_t *handle)
           Returns the list of packages removed by the
           transaction.
       int alpm_trans_init (alpm_handle_t *handle, int flags)
           Initialize the transaction.
       int alpm_trans_prepare (alpm_handle_t *handle, alpm_list_t
           **data)
           Prepare a transaction.
       int alpm_trans_commit (alpm_handle_t *handle, alpm_list_t
           **data)
           Commit a transaction.
       int alpm_trans_interrupt (alpm_handle_t *handle)
           Interrupt a transaction.
       int alpm_trans_release (alpm_handle_t *handle)
           Release a transaction.
}

void install_pacman(struct strv *pkgs) {
  vecforeach(pkgs, char*, pname) {
    require_package(*pname);
  }
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
