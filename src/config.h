#ifndef CONFIG_H
#define CONFIG_H

#define ALPM_ROOT "/"
#define ALPM_DBPATH "/var/lib/pacman"
#define ALPM_CACHEPATH "/var/cache/pacman/pkg/"
#define PACMAN_CONF "/etc/pacman.conf"
#define PACMAN_LOGFILE "/tmp/panificatie.log"
#define CONFIG_PATH "./etc/panificatie.conf"

#define PANIFICATIE_CACHE "/var/cache/panificatie"
#define PANIFICATIE_CACHE_FILE "/var/cache/panificatie/dbs.cache"

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

