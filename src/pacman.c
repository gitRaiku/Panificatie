#include "pacman.h"

#define shforeach(shm, res) for (size_t sl__shm ##res = shlenu(shm), res = 0; res < sl__shm ##res; ++res)

struct cenv *ce;
void pacman_set_cenv(struct cenv *_ce) { ce = _ce; }

alpm_handle_t *alpm;
alpm_list_t *dblist;

struct pacEntry *installedPackages = NULL;

struct depEntry *pkgdb = NULL;
struct pacEntry *requiredPackages = NULL;
struct pacEntry *removablePackages = NULL;

struct pdbEntry *pdb = NULL;

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

void alpm_log_callback(void *ctx, alpm_loglevel_t level, const char *fmt, va_list args) {
  if (level < 17) {
    char logf[256];
    snprintf(logf, 256, "Log: %u %s", level, fmt);
    vfprintf(stdout, logf, args);
  }
}

void alpm_download_callback(void *ctx, const char *filename, alpm_download_event_type_t event, void *data) {
  fprintf(stdout, "Download callback for %s: ", filename);
  switch (event) {
    case ALPM_DOWNLOAD_INIT:
      fprintf(stdout, "Init [optional:%u]\n", 
        ((alpm_download_event_init_t*)data)->optional);
      break;
    case ALPM_DOWNLOAD_PROGRESS:
      fprintf(stdout, "Progress [downloaded:%li] [total:%li]\n", 
        ((alpm_download_event_progress_t*)data)->downloaded,
        ((alpm_download_event_progress_t*)data)->total);
      break;
    case ALPM_DOWNLOAD_RETRY:
      fprintf(stdout, "Retry [resume:%u]\n", 
        ((alpm_download_event_retry_t*)data)->resume);
      break;
    case ALPM_DOWNLOAD_COMPLETED:
      fprintf(stdout, "Completed [total:%li] [result:%u]\n", 
        ((alpm_download_event_completed_t*)data)->total,
        ((alpm_download_event_completed_t*)data)->result);
      break;
  }
}

void alpm_question_callback(void *ctx, alpm_question_t *question) {
  //TODO("Print questions then die");
}

void alpm_event_callback(void *ctx, alpm_event_t *event) { /// TODO: Event cb
  fprintf(stdout, "Alpm event callback!\n");
}

void alpm_progress_callback(void *ctx, alpm_progress_t progress, const char *pkg, int percent, size_t howmany, size_t current) {
  fprintf(stdout, "Alpm progress: %u %s %u%% %lu/%lu\n", progress, pkg, percent, current, howmany);
}

void pacman_initdb() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

  //alpm_list_t *cachedirs = alpm_list_add(NULL, ALPM_CACHEPATH);
  //ENEZ(alpm_option_set_cachedirs(alpm, cachedirs));

  svecforeach(pacman_repositories, const char* const, repo) { 
    alpm_register_syncdb(alpm, *repo, 0); 
  }
  dblist = alpm_get_syncdbs(alpm);

  alpmforeach(dblist, i) {
    svecforeach(pacman_mirrors, const char* const, mirror) { 
      alpm_db_add_server(i->data, *mirror);
    }
    alpmforeach(alpm_db_get_pkgcache(i->data), j) {
      db_insert_pkg(j->data);
    }
  }

  //alpm_option_set_logcb(alpm, alpm_log_callback, NULL);
  alpm_option_set_dlcb(alpm, alpm_download_callback, NULL);
  alpm_option_set_eventcb(alpm, alpm_event_callback, NULL);
  alpm_option_set_progresscb(alpm, alpm_progress_callback, NULL);
  alpm_option_set_questioncb(alpm, alpm_question_callback, NULL);

  // init_pdb(PANIFICATIE_CACHE_FILE);
}

uint8_t check_pkgv_type(struct pkgv *pv, enum packageType pt) {
  vecforeach(pv, struct package, pkg) { if (pkg->t == pt) { return 1; } }
  return 0;
}

void check_add_package(char *pname) {
  struct pkgv *pv = (shgeti(pkgdb, pname) >= 0) ? shget(pkgdb, pname) : NULL;
  if (pv == NULL) { fprintf(stderr, "Could not find package %s, not good!\n", pname); exit(1); }
  uint8_t noneSelected = 1;
  vecforeach(pv, struct package, pkg) {
    if (pkg->t == PKG_ALPM_FIXED) {
      noneSelected = 0;
      pkg->t = PKG_ALPM_ADDED;

      shput(requiredPackages, alpm_pkg_get_name(pkg->d), pkg->d);

      alpmforeach(alpm_pkg_get_depends(pkg->d), dep) {
        char *dn = ((alpm_depend_t*)dep->data)->name; 
        if (strcmp(dn, pname)) { check_add_package(dn); }
      }
    } else if (pkg->t == PKG_ALPM_ADDED) { noneSelected = 0; }
  }

  if (noneSelected) { 
    fprintf(stderr, "Package %s has multiple providers: please select one\n", pname);
    uint32_t idx = 1; vecforeach(pv, struct package, pkg) { fprintf(stderr, "  %u) %s", idx++, alpm_pkg_get_name(pkg->d)); }
    fprintf(stderr, "\n");
    exit(1);
  }
}

/// TODO: Improve error checking and add it to all functions
void require_package(char *pname, uint8_t base) {
  struct pkgv *pv = (shgeti(pkgdb, pname) >= 0) ? shget(pkgdb, pname) : NULL;
  if (pv == NULL) { fprintf(stderr, "Could not find a provider for %s, not good!\n", pname); return; } /// TODO: List all missing packages

  struct package *p;

  if (base) { /// Forcefully include package pname
    vecforeach(pv, struct package, pkg) {
      if (!strcmp(alpm_pkg_get_name(pkg->d), pname)) {
        p = pkg;
        goto rq_add_provides;
      }
    }
    fprintf(stderr, "Could not find a package with name %s, however these packages provide it\n", pname);
    uint32_t i = 1;
    vecforeach(pv, struct package, pkg) { fprintf(stderr, "  %u) %s", i++, alpm_pkg_get_name(pkg->d)); }
    fprintf(stderr, "\n");
    exit(1);
  }

  vecforeach(pv, struct package, pkg) { if (pkg->t == PKG_ALPM_FIXED) { return; } } /// At least one provider was included
  
  /// TODO: Add config flag to automatically add package if the dependency name is
  ///     : the same as the package name, as i think this is what pacman does
  if (pv->l > 1) { 
    vecforeach(pv, struct package, pkg) {
      if (!strcmp(alpm_pkg_get_name(pkg->d), pname)) {
        p = pkg;
        goto rq_add_provides;
      }
    }
    return; /// No provider was selected and there are multiple choices
  }

  p = pv->v;
rq_add_provides:;
  if (p->t == PKG_ALPM_FIXED) { return; }
  p->t = PKG_ALPM_FIXED;

  alpmforeach(alpm_pkg_get_provides(p->d), dep) {
    size_t dl = shgeti(pkgdb, ((alpm_depend_t*)dep->data)->name);
    vecforeach(pkgdb[dl].value, struct package, pkg) {
      if (pkg->d == p->d) { pkg->t = PKG_ALPM_FIXED; }
    }
  }

  alpmforeach(alpm_pkg_get_depends(p->d), dep) {
    char *dn = ((alpm_depend_t*)dep->data)->name; 
    if (strcmp(dn, pname)) { require_package(dn, 0); }
  }
}

#define ALPMERR alpm_strerror(alpm_errno(alpm))
#define ALPMCLEAN {alpm_trans_release(alpm); alpm_release(alpm);exit(1);}
#define EALPM(cmd, res, args...) if ((cmd) < 0) { fprintf(stderr, res ": %s!\n", ##args, ALPMERR); ALPMCLEAN; }
void pacman_trans(alpm_list_t *alpmPackages, uint32_t flags) { /// Pacman supports trans rights
  EALPM(alpm_trans_init(alpm, 0), "Could not init alpm transaction, are you running as root?");
  alpmforeach(alpmPackages, pkg) { 
    fprintf(stdout, "Adding %u %s %p\n", flags, alpm_pkg_get_name(pkg->data), pkg->data);
    fflush(stdout);
    EALPM(alpm_add_pkg(alpm, pkg->data), "Could not add package %s to the transaction", alpm_pkg_get_name(pkg->data)); 
  }

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
    ALPMCLEAN;
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
    ALPMCLEAN;
  }*/

  EALPM(alpm_trans_release(alpm), "Could not release the pacman transaction!\n");
}

void get_removable_packages() {
  shforeach(installedPackages, i) {
    if (shgeti(requiredPackages, installedPackages[i].key) < 0) {
      vecforeach(ce->pc->aurPkgs, char*, apkg) { if (!strcmp(installedPackages[i].key, *apkg)) { goto rp_fin; } }
      shput(removablePackages, installedPackages[i].key, installedPackages[i].value);
    }
    rp_fin:;
  }
}

void print_packages_status() {
  size_t cl;
  cl = shlenu(requiredPackages);
  fprintf(stdout, "The following packages will remain installed\n");
  for(size_t i = 0; i < cl; ++i) {
    if (shgeti(installedPackages, requiredPackages[i].key) >= 0) {
      fprintf(stdout, "%s ", requiredPackages[i].key);
    }
  }
  fprintf(stdout, "\n\nThe following new packages will be installed\n");
  for(size_t i = 0; i < cl; ++i) {
    if (shgeti(installedPackages, requiredPackages[i].key) < 0) {
      fprintf(stdout, "%s ", requiredPackages[i].key);
    }
  }
  fprintf(stdout, "\n\nThe following packages will be uninstalled\n");
  cl = shlenu(removablePackages);
  for(size_t i = 0; i < cl; ++i) {
    fprintf(stdout, "%s ", removablePackages[i].key);
  }
  fprintf(stdout, "\n");
}

uint8_t aur_checkexists(char *path) {
  struct stat s;
  if (stat(path, &s) < 0) { return 0; }
  return 1;
}


char _fbuf[1024]; int32_t _fres;
#define ifm(...) (snprintf(_fbuf, sizeof(_fbuf), __VA_ARGS__),_fbuf)
#define runcmd(...) if ((_fres=system(ifm(__VA_ARGS__)) != 0))
#define aurpath(post) ifm("%s/aur_%s/" post, PANIFICATIE_CACHE, pname)

int32_t aur_require(char *pname);
void aur_adddep(char *pname) {
  if (shgeti(pkgdb, pname) >= 0) {
    fprintf(stdout, "Adding dep from db %s!\n", pname);
    require_package(pname, 0);
  } else {
    fprintf(stdout, "Adding dep from AUR %s!\n", pname);
    aur_require(pname);
  }
}

int32_t aur_add(char *pname) {
  if (!aur_checkexists(aurpath(".SRCINFO"))) {
    fprintf(stderr, "Could not find SRCINFO for %s at %s!\n", pname, _fbuf);
    runcmd("echo rm -rf %s/aur_%s", PANIFICATIE_CACHE, pname) {
      fprintf(stderr, "Could not run %s failed with %i!\n", _fbuf, _fres); return 1;
    }

    return 1;
  }

  struct strEntry *se = conf_read_eq(aurpath(".SRCINFO"));

  if (shgeti(se, "depends") >= 0) { struct strv *ce = shget(se, "depends"); vecforeach(ce, char*, str) { aur_adddep(*str); } }
  if (shgeti(se, "makedepends") >= 0) { struct strv *ce = shget(se, "makedepends"); vecforeach(ce, char*, str) { aur_adddep(*str); } } /// TODO: Maybe not always require makedeps

  conf_free_eq(se);

  return 0;
}

int32_t aur_update(char *pname) {
  runcmd("cd %s/aur_%s && git stash && git pull", PANIFICATIE_CACHE, pname) {
    fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
    TODO("Handle this error");
  }

  return aur_add(pname);
}

int32_t aur_firstinstall(char *pname) {
  if (aur_checkexists(aurpath("."))) {
    fprintf(stderr, "Repository %s at %s already exists yet is not in the database, removing!\n", pname, _fbuf);
    runcmd("echo rm -rf %s/aur_%s", PANIFICATIE_CACHE, pname) {
      fprintf(stderr, "Could not run %s failed with %i!\n", _fbuf, _fres); return 1;
    }
  }

  runcmd("cd %s && git clone --depth=1 https://aur.archlinux.org/%s aur_%s", PANIFICATIE_CACHE, pname, pname) {
    fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
    fprintf(stderr, "Removing %s/%s!\n", PANIFICATIE_CACHE, pname);
    runcmd("echo rm -rf %s/aur_%s", PANIFICATIE_CACHE, pname) {
      fprintf(stderr, "Could not run %s failed with %i!\n", _fbuf, _fres);
    }
    return 1;
  }
  return aur_add(pname);
}

int32_t aur_require(char *pname) {
  if (shgeti(ce->pdc->entries, pname) >= 0) { // TODO: Handle more specificity
    return aur_update(pname);
  } else {
    return aur_firstinstall(pname);
  }
}

void parse_apkgs() {
  if (!aur_checkexists(PANIFICATIE_CACHE)) {
    fprintf(stderr, "Cannot enter %s for it does not exist!\n", PANIFICATIE_CACHE);
    return;
  }

  vecforeach(ce->pc->aurPkgs, char *, pkg) { aur_require(*pkg); }
}

void pacman_install() {
  alpmforeach(alpm_db_get_pkgcache(alpm_get_localdb(alpm)), pkg) { shput(installedPackages, alpm_pkg_get_name(pkg->data), pkg->data); }

  vecforeach(ce->pc->pacmanPkgs, char*, pname) { require_package(*pname, 1); } /// First pass is to resolve multiple
  parse_apkgs();
  vecforeach(ce->pc->pacmanPkgs, char*, pname) { check_add_package(*pname); } /// Providers for the same dependency

  get_removable_packages(); 

  //print_packages_status();

  alpm_list_t *insPackages = NULL;
  shforeach(requiredPackages, i) { insPackages = alpm_list_add(insPackages, requiredPackages[i].value); }

  alpm_list_t *remPackages = NULL;
  shforeach(removablePackages, i) { remPackages = alpm_list_add(remPackages, removablePackages[i].value); }

  alpm_list_t *conflicts = alpm_checkconflicts(alpm, insPackages);
  alpmforeach(conflicts, conf) { /// TODO: Print conflict waterfall
    fprintf(stderr, "Package %s conflicts with %s due to [%s]! Aborting!\n", 
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package1),
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package2),
        alpm_dep_compute_string(((alpm_conflict_t*)conf->data)->reason));
    exit(1);
  }

  //pacman_trans(remPackages, ALPM_TRANS_FLAG_RECURSE | ALPM_TRANS_FLAG_CASCADE); /// Remove everything extra
  //pacman_trans(insPackages, 0 | ALPM_TRANS_FLAG_DOWNLOADONLY); /// Install everything needed
  alpm_list_free(insPackages);
  alpm_list_free(remPackages);
}

void free_package(struct package *pkg) { 
  switch (pkg->t) {
    default:
  }
}

void pacman_freedb() {
  size_t np = shlenu(pkgdb);
  for(uint32_t i = 0; i < np; ++i) {
    vecforeach(pkgdb[i].value, struct package, pkg) { free_package(pkg); }
    vecfree(pkgdb[i].value);
  }

  shfree(removablePackages);
  shfree(installedPackages);
  shfree(requiredPackages);
  shfree(pkgdb);
  ENEG(alpm_release(alpm), "Could not release alpm!");
}
