#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"
#include "src/util.h"

#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>

#define OBJF ".obj/"
#define SRCF "src/"

#define TARGET "pan"

time_t __attribute((pure)) max(time_t o1, time_t o2) { return o1 > o2 ? o1 : o2; }
#define IMAX(a, b) { (a) = max(a, b); }

const char *CCFLAGS[] = { 
  "-Wall", "-Og", "-ggdb3",
  "-lalpm", "-D_FILE_OFFSET_BITS=64", // ??????????? Be able to write files bigger than 2Gb?? Idk
};

void add_flags(Nob_Cmd *__restrict cmd) { int32_t i; for(i = 0; i < ARRAY_LEN(CCFLAGS); ++i) { nob_cmd_append(cmd, CCFLAGS[i]); } }

struct strv *get_files(char *fname) {
  DIR *sdir; struct dirent *ce;
  uint32_t nlen;
  struct strv *__restrict sv; veci(strv, sv);
  ENULL((sdir = opendir(fname)), "Could not open directory %s", fname);
  while ((ce = readdir(sdir))) {
    if (ce->d_type != DT_REG) { continue ; }
    nlen = strlen(ce->d_name);
    if (ce->d_name[nlen - 1] != 'c') { continue ; }

    vecp(sv, strdup(ce->d_name));
  }
  return sv;
}

time_t gmtime(char *__restrict fname) {
  struct stat s;
  if (stat(fname, &s) < 0) { return 0; }
  return s.st_mtim.tv_sec * 1000 + s.st_mtim.tv_nsec / 1000000;
}

time_t comp_object(Nob_Cmd *__restrict cmd, char *__restrict fname) {
  uint32_t sl = strlen(fname);
  char *oname = alloca(sl + strlen(OBJF)); 
  sprintf(oname, OBJF"%s", fname);
  oname[strlen(oname) - 1] = 'o';
  char *cname = alloca(sl + strlen(SRCF));
  sprintf(cname, SRCF"%s", fname);
  time_t ct = gmtime(cname);

  if (ct > gmtime(oname)) {
    cmd->count = 0;
    nob_cc(cmd);

    nob_cmd_append(cmd);
    svecforeach(CCFLAGS, const char*, cf) { nob_cmd_append(cmd, *cf); }

    nob_cmd_append(cmd, "-c", "-o", oname, cname);

    if (!nob_cmd_run_sync(*cmd)) { fprintf(stderr, "Could not build %s!\n", fname); exit(1); }
  }
  return ct;
}

void rebuild() {
  Nob_Cmd cmd = {0};

  nob_minimal_log_level = NOB_NO_LOGS;
  EZERO(nob_mkdir_if_not_exists(OBJF), "Could not create %s", OBJF)
  nob_minimal_log_level = NOB_INFO;

  struct strv *__restrict files = get_files(SRCF);

  time_t maxt = 0;
  vecforeach(files, char*, cv) { 
    IMAX(maxt, comp_object(&cmd, *cv));
  }

  if (maxt >= gmtime(TARGET)) {
    cmd.count = 0;
    nob_cc(&cmd);
    svecforeach(CCFLAGS, const char*, cf) { nob_cmd_append(&cmd, *cf); }
    nob_cmd_append(&cmd, "-o", TARGET);
    vecforeach(files, char*, cv) { 
      uint32_t sl = strlen(*cv);
      char *oname = alloca(sl + strlen(OBJF)); 
      sprintf(oname, OBJF"%s", *cv);
      oname[strlen(oname) - 1] = 'o';
      nob_cmd_append(&cmd, oname);
    }
    if (!nob_cmd_run_sync(cmd)) { fprintf(stderr, "Could not link %s!\n", TARGET); exit(1); }
  }

  vecforeach(files, char*, cv) { free(*cv); }
  vecfree(files);
}

void run() {
  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "./"TARGET);
  if (!nob_cmd_run_sync(cmd)) { fprintf(stderr, "Could not run %s!\n", TARGET); exit(1); }
}

void debug() {
  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "gdb", "-q", "./"TARGET);
  if (!nob_cmd_run_sync(cmd)) { fprintf(stderr, "Could not run %s!\n", TARGET); exit(1); }
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  rebuild();

  if (argc > 1) {
    if (!strcmp(argv[1], "run")) {
      run();
    } else if (!strcmp(argv[1], "debug")) {
      debug();
    }
  }

  return 0;
}
