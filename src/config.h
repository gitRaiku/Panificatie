#ifndef CONFIG_H
#define CONFIG_H

#define ALPM_ROOT "/"
#define ALPM_DBPATH "/var/lib/pacman"
#define PACMAN_CONF "/etc/pacman.conf"
#define CONFIG_PATH "./etc/panificatie.conf"

static const char *const pacman_repositories[] = { "core", "extra", "multilib" };

#endif

