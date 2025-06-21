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

char *cacheFile = PANIFICATIE_CACHE_FILE;
char *configFile = CONFIG_PATH;

void help() {
  fprintf(stdout, "Usage: panificatie [options]\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "  -c --config: Set config file\n"); /// TODO: Include default params
  fprintf(stdout, "  -C --cache: Set cache path\n"); /// TODO: Add priviledge escalation options
}

#define seq(_i, _s) if (!strcmp(argv[_i], _s))
void parseArgs(int argc, char **argv) {
  for(int32_t i = 1; i < argc; ++i) {
    seq(i, "-c") { goto arg_config;
    } else seq(i, "--config") { arg_config:;
      if (i != argc - 1) { configFile = argv[i + 1]; ++i; }
    } else seq(i, "-C") { goto arg_cache;
    } else seq(i, "--cache") { arg_cache:;
      TODO("Make cache path redefinable");
      if (i != argc - 1) { cacheFile = argv[i + 1]; ++i; }
    } else {
      help();
      exit(1);
    }
  }
}

int main(int argc, char **argv) {
  parseArgs(argc, argv);
  struct cenv ce;
  ce.pdc = conf_read_pdb(cacheFile);
  ce.pc = conf_read_panix(configFile);

  pacman_set_cenv(&ce);

  pacman_initdb();
  pacman_install();
  
  conf_free_pdb(ce.pdc);
  conf_free_config(ce.pc);
  pacman_freedb();
  return 0;
}

