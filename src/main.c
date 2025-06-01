#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "util.h"
#include "package.h"
#include "conf_parser.h"

int main(void) {
  // init_packagedb();
  parse_config(CONFIG_PATH);
  

  return 0;
}

