#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <alpm.h>
#include "config.h"
#include "util.h"
#include "conf.h"

#define alpmforeach(_list, res) for (alpm_list_t *res = (_list); res; res = res->next) 

void print_pkg(alpm_handle_t *alpm, alpm_pkg_t *pkg) {
  fprintf(stdout, "%s\n", alpm_pkg_get_name(pkg));
  alpmforeach(alpm_pkg_get_depends(pkg), i) {
    fprintf(stdout, "  %s\n", ((alpm_depend_t*)i->data)->name);
  }
  fprintf(stdout, "\n");
}

int main(void) {
  alpm_errno_t er = 0;
  alpm_handle_t *alpm = alpm_initialize(ALPM_ROOT, ALPM_DBPATH, &er);
  ENEZ(er, "Could not initialize alpm %s", alpm_strerror(er));

  struct panix_conf *pc = panix_read_conf(CONFIG_PATH);

  vecforeach(pc->repos, char*, s) {
    fprintf(stdout, "%s\n", *s);
    alpm_register_syncdb(alpm, *s, 0);
  }

  alpm_list_t *dblist, *packages = NULL;
  dblist = alpm_get_syncdbs(alpm);

  alpmforeach(dblist, i) {
    packages = alpm_list_join(packages, alpm_list_copy(alpm_db_get_pkgcache(i->data)));
  }

  alpmforeach(packages, i) {
    print_pkg(alpm, i->data);
  }
  alpm_list_free(packages);

  panix_free_conf(pc);
  ENEG(alpm_release(alpm), "Could not release alpm!");
  
  return 0;
}

