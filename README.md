# Panificatie

A worse nix for a better system (arch linux)

# Features
All your installed packages will be configured in ``/etc/panificatie.conf``

Running
```
# pani
```
alone will try to rebuild your system.

```
# pani pacman#foot aur#anki-bin
```
will build your system while also installing ``foot`` and ``anki`` until the next rebuild.

```
# pani run pacman#foot
```
will install ``foot`` without installing or removing packages not inside the config.

An example config can be found in ``etc/panificatie.conf``.

# Building
To prepare the build system you must first run
```
cc -o nob nob.c
```
and then run
```
./nob
```
to build the project.

# Installation

```
./nob install
```

Should install the binary to ``/usr/local/bin/``.

# Requirements

A C Compiler, pacman, git and coreutils.

# Todo

Add support for git and arbitrary packages
Add support for configuration of packages
Improve UX
