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

void alpm_log_callback(void *ctx, alpm_loglevel_t level, const char *fmt, va_list args) {
  return;
  char logf[256];
  snprintf(logf, 256, "Log: %u %s", level, fmt);
  vfprintf(stdout, logf, args);
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

///**
// * Type of question.
// * Unlike the events or progress enumerations, this enum has bitmask values
// * so a frontend can use a bitmask map to supply preselected answers to the
// * different types of questions.
// */
//typedef enum _alpm_question_type_t {
//	/** Should target in ignorepkg be installed anyway? */
//	ALPM_QUESTION_INSTALL_IGNOREPKG = (1 << 0),
//	/** Should a package be replaced? */
//	ALPM_QUESTION_REPLACE_PKG = (1 << 1),
//	/** Should a conflicting package be removed? */
//	ALPM_QUESTION_CONFLICT_PKG = (1 << 2),
//	/** Should a corrupted package be deleted? */
//	ALPM_QUESTION_CORRUPTED_PKG = (1 << 3),
//	/** Should unresolvable targets be removed from the transaction? */
//	ALPM_QUESTION_REMOVE_PKGS = (1 << 4),
//	/** Provider selection */
//	ALPM_QUESTION_SELECT_PROVIDER = (1 << 5),
//	/** Should a key be imported? */
//	ALPM_QUESTION_IMPORT_KEY = (1 << 6)
//} alpm_question_type_t;
//
///** A question that can represent any other question. */
//typedef struct _alpm_question_any_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer */
//	int answer;
//} alpm_question_any_t;
//
///** Should target in ignorepkg be installed anyway? */
//typedef struct _alpm_question_install_ignorepkg_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: whether or not to install pkg anyway */
//	int install;
//	/** The ignored package that we are deciding whether to install */
//	alpm_pkg_t *pkg;
//} alpm_question_install_ignorepkg_t;
//
///** Should a package be replaced? */
//typedef struct _alpm_question_replace_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: whether or not to replace oldpkg with newpkg */
//	int replace;
//	/** Package to be replaced */
//	alpm_pkg_t *oldpkg;
//	/** Package to replace with.*/
//	alpm_pkg_t *newpkg;
//	/** DB of newpkg */
//	alpm_db_t *newdb;
//} alpm_question_replace_t;
//
///** Should a conflicting package be removed? */
//typedef struct _alpm_question_conflict_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: whether or not to remove conflict->package2 */
//	int remove;
//	/** Conflict info */
//	alpm_conflict_t *conflict;
//} alpm_question_conflict_t;
//
///** Should a corrupted package be deleted? */
//typedef struct _alpm_question_corrupted_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: whether or not to remove filepath */
//	int remove;
//	/** File to remove */
//	const char *filepath;
//	/** Error code indicating the reason for package invalidity */
//	alpm_errno_t reason;
//} alpm_question_corrupted_t;
//
///** Should unresolvable targets be removed from the transaction? */
//typedef struct _alpm_question_remove_pkgs_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: whether or not to skip packages */
//	int skip;
//	/** List of alpm_pkg_t* with unresolved dependencies */
//	alpm_list_t *packages;
//} alpm_question_remove_pkgs_t;
//
///** Provider selection */
//typedef struct _alpm_question_select_provider_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: which provider to use (index from providers) */
//	int use_index;
//	/** List of alpm_pkg_t* as possible providers */
//	alpm_list_t *providers;
//	/** What providers provide for */
//	alpm_depend_t *depend;
//} alpm_question_select_provider_t;
//
///** Should a key be imported? */
//typedef struct _alpm_question_import_key_t {
//	/** Type of question */
//	alpm_question_type_t type;
//	/** Answer: whether or not to import key */
//	int import;
//	/** UID of the key to import */
//	const char *uid;
//	/** Fingerprint the key to import */
//	const char *fingerprint;
//} alpm_question_import_key_t;

void alpm_question_callback(void *ctx, alpm_question_t *question) { /// TODO: Fill out excel spreadsheet
  fprintf(stdout, "Got question:\n");
  if (question->type & ALPM_QUESTION_INSTALL_IGNOREPKG) {
  fprintf(stdout, " INSTALL_IGNOREPKG");
  } else if (question->type & ALPM_QUESTION_REPLACE_PKG) {
  fprintf(stdout, " REPLACE_PKG");
  } else if (question->type & ALPM_QUESTION_CONFLICT_PKG) {
  fprintf(stdout, " CONFLICT_PKG");
  } else if (question->type & ALPM_QUESTION_CORRUPTED_PKG) {
  fprintf(stdout, " CORRUPTED_PKG");
  } else if (question->type & ALPM_QUESTION_REMOVE_PKGS) {
  fprintf(stdout, " REMOVE_PKGS");
  } else if (question->type & ALPM_QUESTION_SELECT_PROVIDER) {
  alpm_question_select_provider_t *q = &question->select_provider;
  fprintf(stdout, "Multiple Providers for %s!\n", q->depend->name);
  int32_t ci = 1;
  alpmforeach(q->providers, prov) { 
    fprintf(stdout, "  %u) %s", ci++, alpm_pkg_get_name(prov->data));
  }
  int32_t cc = 1;
  do {
    fscanf(stdin, "%i", &cc);
  } while (!(1 <= cc && cc <= ci));
  q->use_index = cc;
  

  } else if (question->type & ALPM_QUESTION_IMPORT_KEY) {
  fprintf(stdout, " IMPORT_KEY");
  }
  fprintf(stdout, "\n");
}

void alpm_event_callback(void *ctx, alpm_event_t *event) { /// TODO: Event cb
  fprintf(stdout, "Alpm event callback!\n");
}

void alpm_progress_callback(void *ctx, alpm_progress_t progress, const char *pkg, int percent, size_t howmany, size_t current) {
  fprintf(stdout, "Alpm progress: %u %s %u%% %lu/%lu\n", progress, pkg, percent, current, howmany);
}



void init_packagedb() {
  alpm_errno_t er = 0;
  alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

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

  alpm_option_set_logcb(alpm, alpm_log_callback, NULL);
  alpm_option_set_dlcb(alpm, alpm_download_callback, NULL);
  alpm_option_set_eventcb(alpm, alpm_event_callback, NULL);
  alpm_option_set_progresscb(alpm, alpm_progress_callback, NULL);
  alpm_option_set_questioncb(alpm, alpm_question_callback, NULL);
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
