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
  init_packagedb();
  struct panix_config *pc = parse_config(CONFIG_PATH);

  install_pacman(pc->pacmanPkgs, pc->aurPkgs);
  
  free_config(pc);
  free_packagedb();
  return 0;
}

