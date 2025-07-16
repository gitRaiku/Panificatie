#include "pacman.h"

char _fbuf[1024]; int32_t _fres;
#define ifm(...) (snprintf(_fbuf, sizeof(_fbuf), __VA_ARGS__),_fbuf)
#define runcmd(...) if ((_fres=system(ifm(__VA_ARGS__)) != 0)) // Use sth better than system
#define aurpath(post) ifm("%s/aur_%s/" post, PANIFICATIE_CACHE, pname)

#define shforeach(shm, res) for (size_t sl__shm ##res = shlenu(shm), res = 0; res < sl__shm ##res; ++res)

DEF_VEC(char, charv)

struct cenv *ce;
void pacman_set_cenv(struct cenv *_ce) { ce = _ce; }

alpm_handle_t *alpm;
alpm_list_t *dblist;

struct pacEntry *installedPackages = NULL;
struct depEntry *pkgdb = NULL;
struct pacEntry *removablePackages = NULL;

struct pacEntry *requiredPackages = NULL;
struct pacEntry *installablePackages = NULL;
struct alpmpkgv *pacmanGroupPkgs = NULL;
struct strEntry *aurInstallablePackages = NULL;

void debug_pname(const char *const pname, uint8_t aur) {
  if (ce->debug) {
    for(uint32_t i = 0; i < ce->curIndent; ++i) { fputs("  ", stdout); }
    fprintf(stdout, "%s%s%s\n", aur ? "Aur[" : "", pname, aur ? "]" : "");
    ++ce->curIndent;
  }
}

void debug_exit() { if (ce->debug && ce->curIndent) { ce->curIndent--; } }

uint8_t checkexists(char *path) {
  struct stat s;
  if (stat(path, &s) < 0) { return 0; }
  return 1;
}

uint8_t pacman_get_answer(uint8_t autoAnswer) { /// TODO: Put all questions at the begining
  if (autoAnswer == 1) { fprintf(stdout, "Answer: y/n\ny\n"); return 1; }
  if (autoAnswer == 2) { fprintf(stdout, "Answer: y/n\nn\n"); return 0; }

  char answ[10];
  while (1) { 
    fprintf(stdout, "Answer: y/n\n"); /// TODO: Getline default Y
                                      /// TODO: Config file
    fscanf(stdin, "%2s", answ);
    if (answ[0] == 'y' || answ[0] == 'Y') {
      return 1;
    } else if (answ[0] == 'n' || answ[0] == 'N') {
      fprintf(stdout, "Understood.\n");
      return 0;
    }
  }
}


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

void pacman_init() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

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

  /*
  if (!checkexists(PANIFICATIE_KEYRING_FILE)) { /// Create the keyring file if it does not exist.
    system("gpg --no-default-keyring --keyring " PANIFICATIE_KEYRING_FILE " --fingerprint");
  }
  */
}

uint8_t check_pkgv_type(struct pkgv *pv, enum packageType pt) {
  vecforeach(pv, struct package, pkg) { if (pkg->t == pt) { return 1; } }
  return 0;
}

void check_add_package(const char *pname) {
  struct pkgv *pv = (shgeti(pkgdb, pname) >= 0) ? shget(pkgdb, pname) : NULL;
  if (pv == NULL) { 
    alpm_list_t *grp = alpm_find_group_pkgs(dblist, pname);
    alpmforeach(grp, pkg) { alpm_list_free(grp); return; }
    alpm_list_free(grp); 
    fprintf(stderr, "Could not find package %s, not good!\n", pname); exit(1); 
  }
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
void pacman_require(const char *pname, uint8_t base) {
  struct pkgv *pv = (shgeti(pkgdb, pname) >= 0) ? shget(pkgdb, pname) : NULL;
  uint8_t raisedDebugIndent = 0;

  if (ce->debug == 1) {
    if (pv != NULL) {
      vecforeach(pv, struct package, pkg) { if (pkg->t == PKG_ALPM_FIXED) { goto pacreq_dbgd; } }
      raisedDebugIndent = 1;
      debug_pname(pname, 0);
    }
  } else if (ce->debug == 2) {
    raisedDebugIndent = 1;
    debug_pname(pname, 0);
  }

pacreq_dbgd:;
  if (pv == NULL) { 
    if (base) { /// Check maybe a group
      alpm_list_t *grp = alpm_find_group_pkgs(dblist, pname);
      if (grp != NULL) {
        alpmforeach(grp, pkg) {
          vecp(pacmanGroupPkgs, pkg->data);
          pacman_require(alpm_pkg_get_name(pkg->data), 0);
        }
        alpm_list_free(grp);
      } else {
        fprintf(stderr, "Could not find package %s, skipping!\n", pname);
      }
    } else { /// TODO: List all missing packages
      fprintf(stderr, "Could not find a provider for %s, not good!\n", pname); 
    }
    if (raisedDebugIndent) { debug_exit(); }
    return; 
  } 

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
    if (raisedDebugIndent) { debug_exit(); }
    exit(1);
  }

  vecforeach(pv, struct package, pkg) { if (pkg->t == PKG_ALPM_FIXED) { if (raisedDebugIndent) { debug_exit(); } return; } } /// At least one provider was included
  
  /// TODO: Add config flag to automatically add package if the dependency name is
  ///     : the same as the package name, as i think this is what pacman does
  if (pv->l > 1) { 
    vecforeach(pv, struct package, pkg) {
      if (!strcmp(alpm_pkg_get_name(pkg->d), pname)) {
        p = pkg;
        goto rq_add_provides;
      }
    }
    if (raisedDebugIndent) { debug_exit(); }
    return; /// No provider was selected and there are multiple choices
  }

  p = pv->v;
rq_add_provides:;
  if (p->t == PKG_ALPM_FIXED) { return; }
  p->t = PKG_ALPM_FIXED;
  if (shgeti(requiredPackages, pname) <= 0) {
    shput(requiredPackages, strdup(pname), p->d);
  }

  alpmforeach(alpm_pkg_get_provides(p->d), dep) {
    size_t dl = shgeti(pkgdb, ((alpm_depend_t*)dep->data)->name);
    vecforeach(pkgdb[dl].value, struct package, pkg) {
      if (pkg->d == p->d) { pkg->t = PKG_ALPM_FIXED; }
    }
  }

  alpmforeach(alpm_pkg_get_depends(p->d), dep) {
    char *dn = ((alpm_depend_t*)dep->data)->name; 
    if (strcmp(dn, pname)) { pacman_require(dn, 0); }
  }
  if (raisedDebugIndent) { debug_exit(); }
}

int gdbgeti(char *str) {
  return shgeti(installedPackages, str);
}

void get_installable_removable_packages() {
  shforeach(installedPackages, i) {
    // if (alpm_pkg_get_reason(installedPackages[i].value) != ALPM_PKG_REASON_EXPLICIT) { continue ; } /// TODO: Make this a flag
    if (shgeti(requiredPackages, installedPackages[i].key) < 0) {
      vecforeach(ce->pc->aurPkgs, char*, apkg) { if (!strcmp(installedPackages[i].key, *apkg)) { goto rp_fin; } }
      shput(removablePackages, installedPackages[i].key, installedPackages[i].value);
    }
    rp_fin:;
  }

  shforeach(requiredPackages, i) {
    if (shgeti(installedPackages, alpm_pkg_get_name(requiredPackages[i].value)) < 0) {
      shput(installablePackages, alpm_pkg_get_name(requiredPackages[i].value), requiredPackages[i].value);
    }
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

char *str_find_next(char *str, char c) {
  while (*str != '\0' && *str != c) { ++str; }
  if (*str == '\0') { return NULL; }
  return str;
}

void aur_adddep(char *pname) {
  char *ns = NULL; /// Get rid of version specifiers TODO: Handle them
  if ((ns = str_find_next(pname, '>')) != NULL) { *ns = '\0'; }
  if ((ns = str_find_next(pname, '=')) != NULL) { *ns = '\0'; }
  if (shgeti(pkgdb, pname) >= 0) { pacman_require(pname, 0); } 
  else { aur_require(pname); }
}

int32_t aur_add(char *pname) {
  if (!checkexists(aurpath(".SRCINFO"))) {
    fprintf(stderr, "Could not find SRCINFO for %s at %s!\n", pname, _fbuf);
    runcmd("echo rm -rf %s/aur_%s", PANIFICATIE_CACHE, pname) {
      fprintf(stderr, "Could not run %s failed with %i!\n", _fbuf, _fres); return 1;
    }

    return 1;
  }

  if (shgeti(aurInstallablePackages, pname) >= 0) { return 0; }

  struct vstrEntry *se = conf_read_eq(aurpath(".SRCINFO"));

  if (shgeti(se, "depends") >= 0) { 
    struct strv *ce = shget(se, "depends"); 
    vecforeach(ce, char*, str) { aur_adddep(*str); } 
  }

  if (shgeti(se, "makedepends") >= 0) { 
    struct strv *ce = shget(se, "makedepends"); 
    vecforeach(ce, char*, str) { aur_adddep(*str); } 
  }

  if (shgeti(se, "pkgver") >= 0 && shgeti(se, "pkgrel") >= 0) { /// This is never to be optimized
    char *pkgver = shget(se, "pkgver")->v[0];
    char *pkgrel = shget(se, "pkgrel")->v[0];
    shput(aurInstallablePackages, strdup(pname), 
        strdup(ifm("%s-%s", pkgver, pkgrel)));
  } else { return 1; }

  conf_free_eq(se);

  return 0;
}

int32_t aur_update(char *pname) {
  if (ce->update && pacman_get_answer(ce->autoAurUpdate)) {
    runcmd("cd %s/aur_%s && git stash && git pull", PANIFICATIE_CACHE, pname) {
      fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
      return 1;
    }
  }

  return aur_add(pname);
}

int32_t aur_firstinstall(char *pname) {
  if (checkexists(aurpath("."))) {
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
  debug_pname(pname, 1); 

  int32_t r;
  if (checkexists(aurpath("."))) {
    r = aur_update(pname);
  } else {
    r = aur_firstinstall(pname);
  }

  debug_exit();
  return r;


  if (shgeti(installedPackages, pname) >= 0) { // TODO: Handle more specificity
    r = aur_update(pname);
  } else {
    r = aur_firstinstall(pname);
  }
}

int parse_apkgs() {
  if (!checkexists(PANIFICATIE_CACHE)) {
    fprintf(stderr, "Cannot enter %s for it does not exist!\n", PANIFICATIE_CACHE);
    return 1;
  }

  vecforeach(ce->pc->aurPkgs, char *, pkg) { if (aur_require(*pkg)) { return 1; } }
  return 0;
}

#define GNUPGHOME ifm(PANIFICATIE_CACHE"/.gnupg-%i",getuid())
int32_t aur_make(char *pname) {
  runcmd("cd %s/aur_%s && yes | GNUPGHOME="PANIFICATIE_CACHE"/.gnupg-%i makepkg -sci", PANIFICATIE_CACHE, pname, getuid()) {
    fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
    return 1;
  }

  return 0;
}

int aur_ver_greater(char *v1, char *v2) { /// TODO: Implement
  return 0;
}

void aur_initgpg() {
  if (!checkexists(GNUPGHOME)) {
    ENEZ(mkdir(GNUPGHOME, 0700), "Could not create gpg directory at %s", GNUPGHOME);
  }
  FILE *__restrict gpgconf = fopen(ifm(PANIFICATIE_CACHE"/.gnupg-%i/gpg.conf",getuid()), "w");
  ENULL(gpgconf, "Could not open gpg conf at %s", ifm(PANIFICATIE_CACHE"/.gnupg-%i/gpg.conf",getuid()));
  svecforeach(gnupg_keyservers, const char* const, ksrv) {
    fprintf(gpgconf, "keyserver \"%s\"\n", *ksrv);
  }
  fclose(gpgconf);
}

void aur_makeall() {
  if (!checkexists(PANIFICATIE_CACHE)) {
    fprintf(stderr, "Cannot enter %s for it does not exist!\n", PANIFICATIE_CACHE);
    return;
  }

  aur_initgpg();

  shforeach(aurInstallablePackages, i) {
    char *pname = aurInstallablePackages[i].key;
    const char *oldver = "";
    if (shgeti(installedPackages, pname) >= 0) { oldver = alpm_pkg_get_version(shget(installedPackages, pname)); }

    const char *newver = shget(aurInstallablePackages, pname);
    //fprintf(stdout, "Package %s: cache[%s] installed[%s]\n", pname, oldver, newver);
    if (*oldver == '\0' || (alpm_pkg_vercmp(newver, oldver) == 1)) {
      fprintf(stdout, "Do you want to install %s cached[%s] installed[%s]?\n", pname, oldver, newver);

      if (pacman_get_answer(ce->autoAurInstall)) {
        struct vstrEntry *se = conf_read_eq(ifm("%s/aur_%s/.SRCINFO", PANIFICATIE_CACHE, pname));

        if (shgeti(se, "validpgpkeys") >= 0) { 
          struct strv *ce = shget(se, "validpgpkeys"); 
          vecforeach(ce, char*, str) { 
            runcmd("GNUPGHOME="PANIFICATIE_CACHE"/.gnupg-%i gpg --list-keys %s &> /dev/null", getuid(), *str) { // Key not found; 
              fprintf(stdout, "GNUPGHOME="PANIFICATIE_CACHE"/.gnupg-%i gpg --recv-keys %s\n", getuid(), *str);
                runcmd("GNUPGHOME="PANIFICATIE_CACHE"/.gnupg-%i gpg --recv-keys %s", getuid(), *str) { 
                fprintf(stderr, "Could not import gpg key %s!\n", *str);
              }
            }
          } 
        }

        conf_free_eq(se);

        aur_make(pname);
      }
    }
  }
}

void charv_addstr(struct charv *cv, const char *str) {
  while (*str) { vecp(cv, *str); ++str; }
}

void pacman_update_repos() {
  char updcmd[] = "sudo pacman -Sy";
  fprintf(stdout, "Do you want to run: %s\n", updcmd);

  if (pacman_get_answer(ce->autoPacmanUpdate)) {
    runcmd("%s", updcmd) {
      fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
      exit(1);
    }
  }
}



#define COL_RED "\033[31m"
#define COL_GREEN "\033[32m"
#define COL_NONE ""
#define COL_STOP "\033[0m"

void pacman_paccmd(char *cmd, const char *col, struct pacEntry *pkgs) {
  struct charv *chv;
  veci(charv, chv);
  charv_addstr(chv, cmd);

  shforeach(pkgs, i) { 
    charv_addstr(chv, " ");
    charv_addstr(chv, alpm_pkg_get_name(pkgs[i].value));
  }
  vecp(chv, '\0');

  fprintf(stdout, "Do you want to run:\n");
  fprintf(stdout, "%s%s%s\n", isatty(STDOUT_FILENO) ? col : COL_NONE, chv->v, COL_STOP);

  if (pacman_get_answer((pkgs == installablePackages) ? ce->autoPacmanInstall : ce->autoPacmanRemove)) {
    runcmd("yes | %s", chv->v) {
      fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
      exit(1);
    }
  }

  vecfree(chv);
}

void pacman_read_config() {
  alpmforeach(alpm_db_get_pkgcache(alpm_get_localdb(alpm)), pkg) { shput(installedPackages, alpm_pkg_get_name(pkg->data), pkg->data); }

  veci(alpmpkgv, pacmanGroupPkgs);
  vecforeach(ce->pc->pacmanPkgs, char*, pname) { pacman_require(*pname, 1); } /// First pass is to resolve multiple
  if (parse_apkgs()) { exit(1); } /// TODO: Check conflicts in the added packages               ///
  vecforeach(ce->pc->pacmanPkgs, char*, pname) { check_add_package(*pname); }  /// Providers for the same dependency
  vecforeach(pacmanGroupPkgs, alpm_pkg_t*, pkg) { check_add_package(alpm_pkg_get_name(*pkg)); }
  vecfree(pacmanGroupPkgs);
}

void pacman_install() {
  get_installable_removable_packages(); 

  alpm_list_t *insPackages = NULL;
  shforeach(requiredPackages, i) { insPackages = alpm_list_add(insPackages, requiredPackages[i].value); }

  alpm_list_t *conflicts = alpm_checkconflicts(alpm, insPackages);
  alpmforeach(conflicts, conf) { /// TODO: Print conflict waterfall
    fprintf(stderr, "Package %s conflicts with %s due to [%s]! Aborting!\n", 
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package1),
        alpm_pkg_get_name(((alpm_conflict_t*)conf->data)->package2),
        alpm_dep_compute_string(((alpm_conflict_t*)conf->data)->reason));
    exit(1);
  }

  alpm_list_free(insPackages);


  if (shlenu(installablePackages) == 0 && shlenu(removablePackages) == 0 && shlenu(aurInstallablePackages) == 0) { fprintf(stdout, "There's nothing to do.\n"); }
  if (shlenu(installablePackages) > 0) {
    if (ce->update) {
      pacman_paccmd("sudo pacman -Su", COL_GREEN, installablePackages);
    } else {
      pacman_paccmd("sudo pacman -S", COL_GREEN, installablePackages);
    }
  }
  
  aur_makeall();

  if (shlenu(removablePackages) > 0) {
    pacman_paccmd("sudo pacman -Rns", COL_RED, removablePackages);
  }
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

  shforeach(aurInstallablePackages, i) { free(aurInstallablePackages[i].key); free(aurInstallablePackages[i].value); }
  
  shfree(removablePackages);
  shfree(installablePackages);
  shfree(installedPackages);
  shfree(requiredPackages);
  shfree(aurInstallablePackages);
  shfree(pkgdb);
  ENEG(alpm_release(alpm), "Could not release alpm!");
}
