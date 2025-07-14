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

void parse_cmd_package(strview pname) {
  strview cpkg = pname;
  while (*cpkg && *cpkg != '#') { ++cpkg; }
  if (*cpkg == '\0') { return; }
  if (*cpkg == '#') { *cpkg = '\0'; ++cpkg; }
  if (!strcmp(pname, "pacman")) { pacman_require(cpkg, 1); }
  else if (!strcmp(pname, "aur")) { aur_require(cpkg); } 
  else { fprintf(stderr, "\033[31mUnknown package type %s for %s!\033[0m\nOnly known sources are pacman, aur e.g. pacman#gcc.\n", pname, cpkg); return; }
}

void help() {
  fprintf(stdout, "Usage: panificatie [rebuild|run|rebrun] [options] [source#package]\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "  -c --config: Set config file\n");
  fprintf(stdout, "  -u --update: Update packages\n");
  fprintf(stdout, "  -v --verbose: Make verbose logging\n");
  fprintf(stdout, "  -h --help: Print this message\n");
}

#define seq(_i, _s) if (!strcmp(argv[_i], _s))
#define carg(_short, _long, _code) else seq (i, "-" #_short) { goto arg_ ##_long; } else seq(i, "--" #_long) { arg_ ##_long:; _code; }
void parseArgs(int argc, char **argv, struct cenv *__restrict ce) {
  if (argc < 2) { help(); exit(0); }

       if (!strcmp(argv[1], "rebuild")) { ce->rebrun = 1; }
  else if (!strcmp(argv[1], "run"))     { ce->rebrun = 2; }
  else if (!strcmp(argv[1], "rebrun"))  { ce->rebrun = 3; }
  else { fprintf(stderr, "Unrecognised verb %s!\n", argv[1]); help(); exit(1); }

  for(int32_t i = 2; i < argc; ++i) {
    if (0) {}
    carg(c, config, if (i != argc - 1) { ce->configFile = argv[i + 1]; ++i; })
    carg(u, update, ce->update = 1)
    carg(v, verbose, ce->verbose = 1)
    carg(h, help, help(); exit(0); )
    else {
      if (!(ce->rebrun & 2)) {
        fprintf(stderr, "Cannot run package %s when not in run or rebrun mode, skipping!\n", argv[i]);
      } else {
        vecp(ce->insPackages, argv[i]);
      }
    }
  }
}

int main(int argc, char **argv) {
  struct cenv ce = {0};
  ce.configFile = CONFIG_PATH;
  veci(strv, ce.insPackages);
  parseArgs(argc, argv, &ce);
  ce.pc = conf_read_panix(ce.configFile);

  pacman_set_cenv(&ce);

  pacman_init();

  if (ce.rebrun & 1) {
    pacman_read_config();
  }

  if (ce.rebrun & 2) {
    vecforeach(ce.insPackages, strview, pkg) {
      parse_cmd_package(*pkg);
    }
  }

  pacman_install();
  
  vecfree(ce.insPackages);
  conf_free_config(ce.pc);
  pacman_freedb();
  return 0;
}

