#ifndef CONFIG_H
#define CONFIG_H

#define ALPM_ROOT "/"
#define ALPM_DBPATH "/var/lib/pacman"
#define ALPM_CACHEPATH "/var/cache/pacman/pkg/"
#define PACMAN_CONF "/etc/pacman.conf"
#define CONFIG_PATH "./etc/panificatie.conf"

static const char *const pacman_repositories[] = { "core", "extra", "multilib" };
static const char *const pacman_mirrors[] = { 
  "http://archlinux.mirrors.linux.ro/extra/os/x86_64/",
  "http://archlinux.mirrors.linux.ro/core/os/x86_64/",
  "http://archlinux.mirrors.linux.ro/multilib/os/x86_64/" }; /// TODO: Read these parameters from a config file

#endif

