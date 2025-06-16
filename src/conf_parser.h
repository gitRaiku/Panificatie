#ifndef CONF_PARSER_H
#define CONF_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "util.h"

#include "stb_c_lexer.h"
#include "stb_ds.h"

struct panix_config *conf_read_panix(char *fname);

struct panix_config {
  struct strv *pacmanPkgs;
  struct strv *aurPkgs;

  char curstr[128];
};

void conf_free_config(struct panix_config *pc);

struct strEntry { char *key; struct strv *value; }; /// TODO: Merge into one but
struct strEntry *conf_read_eq(char *fname);         /// This code is already dumb 
void conf_free_eq(struct strEntry *se);             /// Enough so i'll do it later

struct pdbEntry { char *key; char *value; };
struct pdb {
  struct pdbEntry *entries;
};

struct pdb *conf_read_pdb(char *fname);
void conf_free_pdb(struct pdb *pc);

struct cenv {
  struct pdb *pdc;
  struct panix_config *pc;
};

#endif
