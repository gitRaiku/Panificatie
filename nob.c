#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"
#include "src/vec.h"

#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>

#define OBJF ".obj/"
#define SRCF "src/"

const char *CCFLAGS[] = { 
  "-Wall", 
  "-Og", 
  "-ggdb3" 
};

DEF_VEC(char*, strv);

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

void comp_object(Nob_Cmd *__restrict cmd, char *__restrict fname) {
  cmd->count = 0;
  nob_cc(cmd);

  nob_cmd_append(cmd);
  svecforeach(CCFLAGS, const char*, cf) { nob_cmd_append(cmd, *cf); }

  uint32_t sl = strlen(fname);
  char *oname = alloca(sl + strlen(OBJF)); 
  sprintf(oname, OBJF"%s", fname);
  oname[strlen(oname) - 1] = 'o';
  char *cname = alloca(sl + strlen(SRCF));
  sprintf(cname, SRCF"%s", fname);

  nob_cmd_append(cmd, "-c", "-o", oname, cname);

  if (!nob_cmd_run_sync(*cmd)) { fprintf(stderr, "Could not build %s!\n", fname); exit(1); }
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(OBJF)) return 1;

    Nob_Cmd cmd = {0};

    struct strv *__restrict files = get_files(SRCF);

    vecforeach(files, char*, cv) { 
      comp_object(&cmd, *cv);
    }

    vecforeach(files, char*, cv) { free(*cv); }
    vecfree(files);

    // Let's append the command line arguments
    //nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"hello", SRC_FOLDER"hello.c");

    // Let's execute the command synchronously, that is it will be blocked until it's finished.
    //if (!nob_cmd_run_sync(cmd)) return 1;
    //cmd.count = 0;

    //nob_cc(&cmd);
    //add_flags(&cmd);
    //nob_cc_output(&cmd, BUILD_FOLDER "foo");
    //nob_cc_inputs(&cmd, SRC_FOLDER "foo.c");

    // nob_cmd_run_sync_and_reset() resets the cmd for you automatically
    //if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    return 0;
}
