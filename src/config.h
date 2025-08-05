#ifndef CONFIG_H
#define CONFIG_H

#define ALPM_ROOT "/"
#define ALPM_DBPATH "/var/lib/pacman"
#define PACMAN_CONF "/etc/pacman.conf"
#define PACMAN_LOGFILE "/tmp/panificatie.log"
#define CONFIG_PATH "/etc/panificatie.conf"

/* 1 for autoYes, 0 for ask, 2 for autoNo */
#define AUTO_PACMAN_UPDATE_REPO_CONFIRM 1
#define AUTO_PACMAN_INSTALL_CONFIRM 0
#define AUTO_PACMAN_REMOVE_CONFIRM 0
#define AUTO_AUR_UPDATE_CONFIRM 0
#define AUTO_AUR_INSTALL_CONFIRM 1
#define EXIT_ON_FAIL 1

#define PANIFICATIE_CACHE "/var/cache/panificatie"
static const char *const gnupg_keyservers[] = { "keys.openpgp.org", "hkps://keyserver.ubuntu.com" };

//#define  "./etc/panificatie.conf"

//#GPGDir      = /etc/pacman.d/gnupg/
//#HookDir     = /etc/pacman.d/hooks/

static const char *const pacman_repositories[] = { "core", "extra", "multilib" };
static const char *const pacman_mirrors[] = { 
  "http://archlinux.mirrors.linux.ro/extra/os/x86_64/",
  "http://archlinux.mirrors.linux.ro/core/os/x86_64/",
  "http://archlinux.mirrors.linux.ro/multilib/os/x86_64/" }; /// TODO: Read these parameters from a config file

#endif

