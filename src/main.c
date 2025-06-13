#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "util.h"
#include "pacman.h"
#include "conf_parser.h"

int main(void) {
  struct cenv ce;
  ce.pdc = conf_read_pdb(PANIFICATIE_CACHE_FILE);
  ce.pc = conf_read_panix(CONFIG_PATH);

  pacman_set_cenv(&ce);

  pacman_initdb();
  pacman_install();
  
  conf_free_pdb(ce.pdc);
  conf_free_config(ce.pc);
  pacman_freedb();
  return 0;
}

