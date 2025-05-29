#include "conf.h"

char *gtrim(char *l) {
  char *r = l;
  while (isspace(*r) && *r) { ++r; }
  return r;
}

char *gspace(char *l) {
  char *r = l;
  while (!isspace(*r) && *r) { ++r; }
  return r;
}

char *gchar(char *l, char c) {
  char *r = l;
  while ((*r != c) && *r) { ++r; }
  return r;
}

void read_file(struct panix_conf *pc, char *fname) {
  FILE *f = fopen(fname, "r");
  ENULL(f, "Could not open file %s\n", fname);

  char *cl = NULL;
  size_t cn = 0;
  while (getline(&cl, &cn, f) >= 0) {
    char *cp = gtrim(cl);
    if (*cp == '[') {
      *gchar(cp, ']') = '\0';
      if (strncmp(cp+1, "options", strlen("options"))) { vecp(pc->repos, strdup(cp+1)); }
    } else if (!strncmp(cp, "Include", strlen("Include"))) {
      cp = gchar(cp, '=');
      if (*cp == '\0') { continue; }
      cp = gtrim(cp + 1);
      *gspace(cp) = '\0';
      read_file(pc, cp);
    }
  }
  free(cl);
  ENEZ(ferror(f), "Getline for file %s failed!", fname);
  ENEZ(fclose(f), "Could not close %s!", fname);
}

struct panix_conf *panix_read_conf(char *fname) {
  struct panix_conf *conf = malloc(sizeof(struct panix_conf));
  veci(strv, conf->repos);
  read_file(conf, fname);
  return conf;
}

void panix_free_conf(struct panix_conf *conf) { 
  vecforeach(conf->repos, char*, s) { free(*s); }
  vecfree(conf->repos);
  free(conf);
}

