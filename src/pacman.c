#include "pacman.h"

#define shforeach(shm, res) for (size_t sl__shm ##res = shlenu(shm), res = 0; res < sl__shm ##res; ++res)

struct cenv *ce;
void pacman_set_cenv(struct cenv *_ce) { ce = _ce; }

alpm_handle_t *alpm;
alpm_list_t *dblist;

struct pacEntry *installedPackages = NULL;

struct depEntry *pkgdb = NULL;
struct pacEntry *requiredPackages = NULL;
struct pacEntry *installablePackages = NULL;
struct pacEntry *removablePackages = NULL;
struct alpmpkgv *pacmanGroupPkgs = NULL;
struct pdbEntry *aurInstallablePackages = NULL;

struct pdbEntry *pdb = NULL;


uint8_t checkexists(char *path) {
  struct stat s;
  if (stat(path, &s) < 0) { return 0; }
  return 1;
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
void require_package(const char *pname, uint8_t base) {
  struct pkgv *pv = (shgeti(pkgdb, pname) >= 0) ? shget(pkgdb, pname) : NULL;
  if (pv == NULL) { 
    if (base) { /// Check maybe a group
      alpm_list_t *grp = alpm_find_group_pkgs(dblist, pname);
      alpmforeach(grp, pkg) {
        vecp(pacmanGroupPkgs, pkg->data);
        require_package(alpm_pkg_get_name(pkg->data), 0);
      }
      alpm_list_free(grp);
    } else { /// TODO: List all missing packages
      fprintf(stderr, "Could not find a provider for %s, not good!\n", pname); 
    }
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
    if (strcmp(dn, pname)) { require_package(dn, 0); }
  }
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

char _fbuf[1024]; int32_t _fres;
#define ifm(...) (snprintf(_fbuf, sizeof(_fbuf), __VA_ARGS__),_fbuf)
#define runcmd(...) if ((_fres=system(ifm(__VA_ARGS__)) != 0)) // Use sth better than system
#define aurpath(post) ifm("%s/aur_%s/" post, PANIFICATIE_CACHE, pname)

int32_t aur_require(char *pname);

char *str_find_next(char *str, char c) {
  while (*str != '\0' && *str != c) { ++str; }
  if (*str == '\0') { return NULL; }
  return str;
}

void aur_adddep(char *pname) {
  char *ns = NULL; /// Get rid of version specifiers TODO: Handle them
  if ((ns = str_find_next(pname, '>')) != NULL) { *ns = '\0'; }
  if ((ns = str_find_next(pname, '=')) != NULL) { *ns = '\0'; }
  if (shgeti(pkgdb, pname) >= 0) {
    //fprintf(stdout, "Adding dep from db %s!\n", pname);
    require_package(pname, 0);
  } else {
    //fprintf(stdout, "Adding dep from AUR %s!\n", pname);
    aur_require(pname);
  }
}

int32_t aur_add(char *pname) {
  if (!checkexists(aurpath(".SRCINFO"))) {
    fprintf(stderr, "Could not find SRCINFO for %s at %s!\n", pname, _fbuf);
    runcmd("echo rm -rf %s/aur_%s", PANIFICATIE_CACHE, pname) {
      fprintf(stderr, "Could not run %s failed with %i!\n", _fbuf, _fres); return 1;
    }

    return 1;
  }

  if (shgeti(ce->pdc->entries, pname) >= 0) {
    char *cs = shget(ce->pdc->entries, pname);
    if (strlen(cs) > 2 && cs[0] == cs[1] && cs[1] == '%') { return 0; } /// Marked as parsed
  }

  struct strEntry *se = conf_read_eq(aurpath(".SRCINFO"));

  if (shgeti(se, "depends") >= 0) { 
    struct strv *ce = shget(se, "depends"); 
    vecforeach(ce, char*, str) { 
      //fprintf(stdout, "Adding dep %s -> %s\n", pname, *str);
      aur_adddep(*str); 
    } 
  }
  if (shgeti(se, "makedepends") >= 0) { 
    struct strv *ce = shget(se, "makedepends"); 
    vecforeach(ce, char*, str) { 
      //fprintf(stdout, "Adding makedep %s -> %s\n", pname, *str);
      aur_adddep(*str); 
    } 
  } /// TODO: Maybe not always require makedeps

  if (shgeti(se, "pkgver") >= 0 && shgeti(se, "pkgrel") >= 0) { /// This is never to be optimized
    char verstr[256] = {0};
    char *pkgver = shget(se, "pkgver")->v[0];
    char *pkgrel = shget(se, "pkgrel")->v[0];
    strncpy(verstr, ifm("%s___%s", pkgver, pkgrel), sizeof(verstr));
    shput(aurInstallablePackages, strdup(pname), strdup(verstr));
  } else { return 1; }

  conf_free_eq(se);

  return 0;
}

int32_t aur_update(char *pname) {
  /* TODO: Only update if update flag
  runcmd("cd %s/aur_%s && git stash && git pull", PANIFICATIE_CACHE, pname) {
    fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
    TODO("Handle this error");
  }
  */

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
  if (checkexists(aurpath("."))) {
    return aur_update(pname);
  } else {
    return aur_firstinstall(pname);
  }


  if (shgeti(ce->pdc->entries, pname) >= 0) { // TODO: Handle more specificity
    return aur_update(pname);
  } else {
    return aur_firstinstall(pname);
  }
}

void parse_apkgs() {
  if (!checkexists(PANIFICATIE_CACHE)) {
    fprintf(stderr, "Cannot enter %s for it does not exist!\n", PANIFICATIE_CACHE);
    return;
  }

  vecforeach(ce->pc->aurPkgs, char *, pkg) { aur_require(*pkg); }
}

#define GNUPGHOME ifm(PANIFICATIE_CACHE"/.gnupg-%i",getuid())
int32_t aur_make(char *pname) {
  runcmd("cd %s/aur_%s && yes | GNUPGHOME="PANIFICATIE_CACHE"/.gnupg-%i makepkg -sci", PANIFICATIE_CACHE, pname, getuid()) {
    fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
    return 1;
  }

  return 0;
}

uint8_t pacman_get_answer() {

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

void aur_updateconf() {
  FILE *__restrict cacheFile = fopen(PANIFICATIE_CACHE_FILE, "w");
  ENULL(cacheFile, "Could not open %s", PANIFICATIE_CACHE_FILE);

  shforeach(ce->pdc->entries, i) {
    fprintf(cacheFile, "%s=%s\n", ce->pdc->entries[i].key, ce->pdc->entries[i].value);
  }

  fflush(cacheFile);
  fclose(cacheFile);
}

void aur_makeall() {
  if (!checkexists(PANIFICATIE_CACHE)) {
    fprintf(stderr, "Cannot enter %s for it does not exist!\n", PANIFICATIE_CACHE);
    return;
  }

  aur_initgpg();

/*
  shforeach(ce->pdc->entries, i) {
    char *cc = ce->pdc->entries[i].value;
    if (strlen(cc) > 2 && cc[0] == cc[1] && cc[1] == '%') {
      fprintf(cacheFile, "%s = %s\n", ce->pdc->entries[i].key, ce->pdc->entries[i].value + 2); /// Get rid of the %% from marking it as built
    }
  }*/

  shforeach(aurInstallablePackages, i) {
    int entpkg = shgeti(ce->pdc->entries, aurInstallablePackages[i].key);
    int inspkg = shgeti(aurInstallablePackages, aurInstallablePackages[i].key); /// Package not previously installed, or installed on lower version
    ENEG(inspkg, "Package %s trying to be installed, but not processed internally!", aurInstallablePackages[i].key);
    fprintf(stdout, "Package %s: cache[", aurInstallablePackages[i].key);
    if (entpkg >= 0) { fprintf(stdout, "%s", ce->pdc->entries[entpkg].value); }
    fprintf(stdout, "] installed[%s]\n", aurInstallablePackages[inspkg].value);
    if (entpkg < 0 || aur_ver_greater(aurInstallablePackages[inspkg].value, ce->pdc->entries[entpkg].value)) {
      fprintf(stdout, "Do you want to install %s cached[%s] installed[%s]?\n",
        aurInstallablePackages[i].key,
        (entpkg >= 0) ? ce->pdc->entries[entpkg].value : "",
        aurInstallablePackages[inspkg].value);

      if (pacman_get_answer()) {
        struct strEntry *se = conf_read_eq(ifm("%s/aur_%s/.SRCINFO", PANIFICATIE_CACHE, aurInstallablePackages[inspkg].key));

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

        if (!aur_make(aurInstallablePackages[inspkg].key)) { 
          if (entpkg < 0) {
            shput(ce->pdc->entries, strdup(aurInstallablePackages[inspkg].key), aurInstallablePackages[inspkg].value);
          }
          aur_updateconf();
        } else { break; }
      }
    }
  }
}


DEF_VEC(char, charv)

void charv_addstr(struct charv *cv, const char *str) {
  while (*str) { vecp(cv, *str); ++str; }
}

void pacman_paccmd(char *cmd, struct pacEntry *pkgs) {
  struct charv *chv;
  veci(charv, chv);
  charv_addstr(chv, cmd);

  shforeach(pkgs, i) { 
    charv_addstr(chv, " ");
    charv_addstr(chv, alpm_pkg_get_name(pkgs[i].value));
  }
  vecp(chv, '\0');

  fprintf(stdout, "Do you want to run:\n");
  fprintf(stdout, "%s\n", chv->v);

  if (pacman_get_answer()) {
    runcmd("%s", chv->v) {
      fprintf(stderr, "Running %s failed with %i!\n", _fbuf, _fres);
      exit(1);
    }
  }

  vecfree(chv);
}

void pacman_install() {
  alpmforeach(alpm_db_get_pkgcache(alpm_get_localdb(alpm)), pkg) { shput(installedPackages, alpm_pkg_get_name(pkg->data), pkg->data); }

  veci(alpmpkgv, pacmanGroupPkgs);
  vecforeach(ce->pc->pacmanPkgs, char*, pname) { require_package(*pname, 1); } /// First pass is to resolve multiple
  parse_apkgs(); /// TODO: Check conflicts in the added packages               ///
  vecforeach(ce->pc->pacmanPkgs, char*, pname) { check_add_package(*pname); }  /// Providers for the same dependency
  vecforeach(pacmanGroupPkgs, alpm_pkg_t*, pkg) { check_add_package(alpm_pkg_get_name(*pkg)); }
  vecfree(pacmanGroupPkgs);

  get_installable_removable_packages(); 

  // print_packages_status();

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

  if (shlenu(installablePackages) > 0) {
    pacman_paccmd("sudo pacman -S", installablePackages);
  }
  
  aur_makeall();

  if (shlenu(removablePackages) > 0) {
    //pacman_paccmd("sudo pacman -Rns", removablePackages);
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
