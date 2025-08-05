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
  if (!strcmp(pname, "pacman") || !strcmp(pname, "pac")) { pacman_require(cpkg, 1); }
  else if (!strcmp(pname, "aur")) { 
    aur_require(cpkg); } 
  else { fprintf(stderr, "\033[31mUnknown package type %s for %s!\033[0m\nOnly known sources are pacman, aur e.g. pacman#gcc.\n", pname, cpkg); return; }
}

void help() {
  fprintf(stdout, "Usage: panificatie [run] [options] [source#package]\n");
  fprintf(stdout, "Verbs:\n");
  fprintf(stdout, "  run: Ignores the config file, letting you install temporary packages\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "  -c --config : Set config file\n");
  fprintf(stdout, "  -i --ignore : Add ignored package\n");
  fprintf(stdout, "  -u --update : Update packages\n");
  fprintf(stdout, "  -v --verbose: Make verbose logs\n");
  fprintf(stdout, "  -d --debug  : Make debug logs\n");
  fprintf(stdout, "  -n --no     : Automatically reject everything\n");
  fprintf(stdout, "  -y --yes    : Automatically accept everything\n");
  fprintf(stdout, "  -h --help   : Print this message\n");
}

#define seq(_i, _s) if (!strcmp(argv[_i], _s))
#define carg(_short, _long, _code) else seq (i, "-" #_short) { goto arg_ ##_long; } else seq(i, "--" #_long) { arg_ ##_long:; _code; }
void parse_args(int argc, char **argv, struct cenv *__restrict ce) {
  if (argc < 1) { help(); exit(0); }

  for(int32_t i = 1; i < argc; ++i) {
    if (0) {}
    carg(c, config, if (i != argc - 1) { ce->configFile = argv[i + 1]; ++i; })
    carg(i, ignore, if (i != argc - 1) { shput(ce->ignoredPkgs, argv[i + 1], argv[i + 1]); })
    carg(u, update, ce->update = 1)
    carg(v, verbose, ce->debug = 1)
    carg(d, debug, ce->debug = 2)
    carg(n, no, ce->autoPacmanUpdate = ce->autoPacmanInstall = ce->autoPacmanRemove = ce->autoAurUpdate = ce->autoAurInstall = 2;)
    carg(y, yes, ce->autoPacmanUpdate = ce->autoPacmanInstall = ce->autoPacmanRemove = ce->autoAurUpdate = ce->autoAurInstall = 1;)

    carg(h, help, help(); exit(0); )
    else {
      if (ce->rebrun == 1 && !strcmp(argv[i], "run")) {
        ce->rebrun = 2; continue;
      } else {
        vecp(ce->insPackages, argv[i]);
      }
    }
  }
}

void cenv_create(struct cenv *__restrict ce) {
  memset(ce, 0, sizeof(*ce));
  ce->configFile = CONFIG_PATH;
  veci(strv, ce->insPackages);
  ce->rebrun = 1;
  ce->autoPacmanUpdate = AUTO_PACMAN_UPDATE_REPO_CONFIRM;
  ce->autoPacmanInstall = AUTO_PACMAN_INSTALL_CONFIRM;
  ce->autoPacmanRemove = AUTO_PACMAN_REMOVE_CONFIRM;
  ce->autoAurUpdate = AUTO_AUR_UPDATE_CONFIRM;
  ce->autoAurInstall = AUTO_AUR_INSTALL_CONFIRM;
  ce->exitOnFail = EXIT_ON_FAIL;
}

void cenv_destroy(struct cenv *__restrict ce) {
  vecfree(ce->insPackages);
  conf_free_config(ce->pc);
  shfree(ce->ignoredPkgs);
}

int main(int argc, char **argv) {
  if (getuid() == 0) { fprintf(stderr, "Panificatie needs to not be run as root!\n"); return 1; }

  struct cenv ce = {0};
  cenv_create(&ce);
  pacman_set_cenv(&ce);
  conf_set_cenv(&ce);
  parse_args(argc, argv, &ce);
  ce.pc = conf_read_panix(ce.configFile);

  if (ce.update) {
    pacman_update_repos();
  }

  pacman_init();

  if (ce.rebrun & 1) {
    pacman_read_config();
  }

  vecforeach(ce.insPackages, strview, pkg) {
    parse_cmd_package(*pkg);
  }

  pacman_install();
  
  pacman_freedb();
  cenv_destroy(&ce);
  return 0;
}

