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

char *configFile = CONFIG_PATH;

void help() {
  fprintf(stdout, "Usage: panificatie [options]\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "  -h --help: Print this help message\n");
  fprintf(stdout, "  -c --config: Set config file [default %s]\n", CONFIG_PATH);
  fprintf(stdout, "  -v --verbose: Set verbosity\n");  /// TODO: Add priviledge escalation options
}

#define seq(_i, _s) if (!strcmp(argv[_i], _s))
void parseArgs(int argc, char **argv, struct cenv *__restrict ce) {
  for(int32_t i = 1; i < argc; ++i) {
    seq(i, "-h") { goto arg_help; }
    else seq(i, "--help") { arg_help:; help(); exit(0); }
    else seq(i, "-c") { goto arg_config; }
    else seq(i, "--config") { arg_config:;
      if (i != argc - 1) { configFile = argv[i + 1]; ++i; } }
    else seq(i, "-v") { goto arg_verbose; }
    else seq(i, "--verbose") { arg_verbose:; ce->debug = 1; }
    else {
      help();
      exit(1);
    }
  }
}

int main(int argc, char **argv) {
  struct cenv ce = {0};
  parseArgs(argc, argv, &ce);
  ce.pc = conf_read_panix(configFile);

  pacman_set_cenv(&ce);

  pacman_init();
  pacman_install();
  
  conf_free_config(ce.pc);
  pacman_freedb();
  return 0;
}

