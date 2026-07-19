# XDG Base Directory Specification

A practical guide to the [Freedesktop XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html) — the standard Linux / Unix convention for where programs should look for and store user-specific data, configuration, cache, and runtime files.

> **Canonical reference:** *XDG Base Directory Specification*, Version 0.8, 8 May 2021,
> authored by Waldo Bastian, Allison Karlitskaya, Lennart Poettering, and Johannes Löthberg.
> Hosted at <https://specifications.freedesktop.org/basedir-spec/>.
> Freedesktop.org explicitly states it is **not** an official standards body, but the spec has been adopted broadly across the Linux desktop ecosystem (GNOME, KDE, systemd, *etc.*).
>
> This document summarizes and contextualizes the spec; consult the canonical text for any authoritative answer.

---

## Table of contents

1. [Purpose and history](#1-purpose-and-history)
2. [Quick reference (the variables)](#2-quick-reference-the-variables)
3. [Detailed specification](#3-detailed-specification)
   - 3.1 [`$XDG_DATA_HOME`](#31-xdg_data_home--user-specific-data-files)
   - 3.2 [`$XDG_CONFIG_HOME`](#32-xdg_config_home--user-specific-configuration)
   - 3.3 [`$XDG_STATE_HOME`](#33-xdg_state_home--user-specific-state-files)
   - 3.4 [`$XDG_CACHE_HOME`](#34-xdg_cache_home--non-essential-cached-data)
   - 3.5 [`$XDG_RUNTIME_DIR`](#35-xdg_runtime_dir--user-specific-runtime-files)
   - 3.6 [`$XDG_DATA_DIRS` and `$XDG_CONFIG_DIRS`](#36-xdg_data_dirs-and-xdg_config_dirs--system-wide-search-paths)
   - 3.7 [`$HOME/.local/bin` (no `XDG_BIN_HOME`)](#37-homebin--the-user-executable-directory)
4. [Resolution rules and precedence](#4-resolution-rules-and-precedence)
5. [Handling missing paths and permission bits](#5-handling-missing-paths-and-permission-bits)
6. [Setting the variables in practice](#6-setting-the-variables-in-practice)
7. [Shell and C examples](#7-shell-and-c-examples)
8. [Related specifications](#8-related-specifications)
9. [Adoption status](#9-adoption-status)
10. [Bibliography / further reading](#10-bibliography--further-reading)

---

## 1. Purpose and history

Before XDG, Unix programs commonly dropped configuration and data files directly into `$HOME` as "dotfiles" (`~/.vimrc`, `~/.bashrc`, `~/.gnupg/`, `~/.ssh/`, …). Over the years that practice produced a long, unsorted, mostly hidden list of files and folders in every user's home directory, making it hard to:

- back up or sync a user's settings without copying the whole home tree,
- reason about which files are essential configuration vs. throw-away cache vs. runtime state,
- keep multiple distributions / architectures in sync (compiled binaries in the same tree as portable text config),
- implement a clean uniform search order between user-level and system-level resources.

In **2003**, Havoc Pennington and others (initially within the Haddon project, then adopted by [freedesktop.org](https://freedesktop.org/)) published the *XDG Base Directory Specification* — a small, tightly-scoped document that defines a handful of environment variables, each pointing at one base directory for a specific *kind* of file. The work has been quietly maintained ever since; the current canonical version is **0.8** (May 2021), which added `$XDG_STATE_HOME`.

The design is intentionally minimal: just eight environment variables (seven directories and one well-known path). Adopting it gives a program a clean, predictable place for each class of file, and gives users one obvious knob to move each class around (e.g. on a separate partition, a tmpfs, a network mount, *etc.*).

---

## 2. Quick reference (the variables)

| Variable                   | Scope                      | Default (if unset / empty) | Introduced |
|----------------------------|----------------------------|----------------------------|------------|
| `$XDG_DATA_HOME`           | user data files            | `$HOME/.local/share`       | 2003       |
| `$XDG_CONFIG_HOME`         | user configuration files   | `$HOME/.config`            | 2003       |
| `$XDG_CACHE_HOME`          | non-essential cached data  | `$HOME/.cache`             | 2003       |
| `$XDG_RUNTIME_DIR`         | runtime sockets, pipes, *etc.* | system-dependent (`/run/user/$UID` on systemd) | 2003 |
| `$XDG_DATA_DIRS`           | ordered *search list* (data)     | `/usr/local/share:/usr/share` | 2003   |
| `$XDG_CONFIG_DIRS`         | ordered *search list* (config)    | `/etc/xdg`                  | 2003       |
| `$XDG_STATE_HOME`          | user state data (logs, history) | `$HOME/.local/state`       | v0.8 (2021)|
| `$HOME/.local/bin`        | user executables (no env var) | n/a                       | 2003       |

Mapping to FHS-style analogues makes the intent memorable:

| XDG variable / path | Analogue in `/` (FHS) | Contains                          |
|----------------------|------------------------|-----------------------------------|
| `$XDG_DATA_HOME`     | `/usr/share`           | persistent user data              |
| `$XDG_CONFIG_HOME`   | `/etc`                 | user-edited configuration         |
| `$XDG_STATE_HOME`    | `/var/lib`             | logs, recent files, undo history  |
| `$XDG_CACHE_HOME`    | `/var/cache`           | regeneratable cache               |
| `$XDG_RUNTIME_DIR`   | `/run`                 | sockets, named pipes, lock files  |
| `$HOME/.local/bin`   | `/usr/local/bin`       | user-installed executables        |

---

## 3. Detailed specification

> All paths set in these variables **must be absolute**. If an implementation encounters a relative path in any of them it should consider the path invalid and ignore it. (Spec §2.)

### 3.1 `$XDG_DATA_HOME` — user-specific data files

```text
If $XDG_DATA_HOME is either not set or empty,
a default equal to $HOME/.local/share should be used.
```

User-specific *data* files: anything a program creates that the user might want to keep across reboots and migrates with the program — databases, user-installed artwork, icon themes, fonts, GIMP plug-ins saved palettes, *etc.*

Examples:

- `$XDG_DATA_HOME/gvfs/metadata/` — GVFS per-file metadata.
- `$XDG_DATA_HOME/flatpak/` — Flatpak user installs.
- `$XDG_DATA_HOME/Steam/` — Steam per-user Steam data (root).
- `$XDG_DATA_HOME/applications/` — user `.desktop` files.

This directory should generally be backed up.

### 3.2 `$XDG_CONFIG_HOME` — user-specific configuration

```text
If $XDG_CONFIG_HOME is either not set or empty,
a default equal to $HOME/.config should be used.
```

User-specific *configuration* files: settings, preferences, dotfiles that are actually configuration. This directory should also typically be backed up.

Examples:

- `$XDG_CONFIG_HOME/gtk-3.0/settings.ini`
- `$XDG_CONFIG_HOME/KDE/kdeglobals`
- `$XDG_CONFIG_HOME/user-dirs.dirs` — generated by `xdg-user-dirs-update`
- `$XDG_CONFIG_HOME/pulse/client.conf`

### 3.3 `$XDG_STATE_HOME` — user-specific state files

```text
If $XDG_STATE_HOME is either not set or empty,
a default equal to $HOME/.local/state should be used.

$XDG_STATE_HOME contains state data that should persist between
(application) restarts, but that is not important or portable enough
to the user that it should be stored in $XDG_DATA_HOME. It may contain:
  – actions history (logs, history, recently used files, …)
  – current state of the application that can be reused on a restart
    (view, layout, open files, undo history, …)
```

Added in v0.8 to give programs a logical home for things that used to be dumped into `$HOME/.cache` even though they are not *cache*. State is persistent between program runs but is *less valuable than data* — for instance, undo history is useful but regenerable; an action log is interesting but rebuilding it would be acceptable.

Examples:

- `$XDG_STATE_HOME/gnome-shell/extensions/`
- `$XDG_STATE_HOME/bash/history`
- `$XDG_STATE_HOME/vim/undo/`
- `$XDG_STATE_HOME/nvim/undo/` *(coming via `set undodir`)*
- `$XDG_STATE_HOME/procps/uptime`

This is the newest of the variables: software support is still catching up, and many tools still keep "state-shaped" things in `$XDG_CACHE_HOME`.

### 3.4 `$XDG_CACHE_HOME` — non-essential cached data

```text
If $XDG_CACHE_HOME is either not set or empty,
a default equal to $HOME/.cache should be used.
```

Anything that **can be regenerated** — thumbnails, compiled bytecode, rendered previews, browser caches, fontconfig caches, *etc.* This directory **may be safely deleted at any time** by the user (or by a janitor like `tmpreaper`, `systemd-tmpfiles`, or BleachBit) without losing data the user cares about.

Examples:

- `$XDG_CACHE_HOME/thumbnails/normal/` and `large/`
- `$XDG_CACHE_HOME/fontconfig/`
- `$XDG_CACHE_HOME/pip/http/`
- `$XDG_CACHE_HOME/yarn/`

### 3.5 `$XDG_RUNTIME_DIR` — user-specific runtime files

```text
$XDG_RUNTIME_DIR defines the base directory relative to which
user-specific non-essential runtime files and other file objects
(such as sockets, named pipes, …) should be stored.

• The directory MUST be owned by the user, and they MUST be the only
  one having read and write access to it. Its Unix access mode MUST
  be 0700.
• The lifetime of the directory MUST be bound to the user being
  logged in. It MUST be created when the user first logs in and if
  the user fully logs out the directory MUST be removed.
  If the user logs in more than once they should get pointed to the
  same directory. Files in the directory MUST not survive reboot
  or a full logout/login cycle.
• The directory MUST be on a local file system and not shared with
  any other system. The directory MUST be fully-featured by the
  standards of the operating system.
  More specifically, on Unix-like operating systems AF_UNIX sockets,
  symbolic links, hard links, proper permissions, file locking,
  sparse files, memory mapping, file change notifications,
  a reliable hard link count must be supported.
• Files in this directory MAY be subjected to periodic clean-up.
  To ensure that your files are not removed, they should have their
  access time timestamp modified at least once every 6 hours of
  monotonic time or the 'sticky' bit should be set on the file.
• If $XDG_RUNTIME_DIR is not set applications should fall back to a
  replacement directory with similar capabilities and print a warning
  message. Applications should use this directory for communication
  and synchronization purposes and should not place larger files in
  it, since it might reside in runtime memory and cannot necessarily
  be swapped out to disk.
```

In practice the spec leaves the *creation* of this directory to the session manager / display manager / PAM stack. On current Linux distributions:

- **systemd** (the common case): `pam_systemd(8)` creates and exports `$XDG_RUNTIME_DIR=/run/user/$UID`. This is essentially a per-user tmpfs.
- **ConsoleKit** systems: similar `/run/user/$UID` via `pam_console`.
- Without `pam_systemd`: the variable is often *unset*, so well-behaved programs fall back to e.g. `TMPDIR` or warn the user.

This is the **only** XDG variable the spec expects the system to set up by default; the others are the user's responsibility.

Examples:

- `$XDG_RUNTIME_DIR/dbus-1` — per-user D-Bus socket directory.
- `$XDG_RUNTIME_DIR/pulse/` — PulseAudio / PipeWire runtime state.
- `$XDG_RUNTIME_DIR/wayland-0` — Wayland compositor socket.
- `$XDG_RUNTIME_DIR/nvim.<pid>.sock` — Neovim IPC.

### 3.6 `$XDG_DATA_DIRS` and `$XDG_CONFIG_DIRS` — system-wide search paths

```text
$XDG_DATA_DIRS defines the preference-ordered set of base directories
to search for data files in addition to the $XDG_DATA_HOME base directory.
  – paths separated by ':' (same separator as $PATH)
  – default: /usr/local/share:/usr/share

$XDG_CONFIG_DIRS defines the preference-ordered set of base directories
to search for configuration files in addition to the $XDG_CONFIG_HOME
base directory.
  – paths separated by ':'
  – default: /etc/xdg
```

These are not write locations; they are *search paths*. Programs walk them in order. The **first directory listed is the most important**; user overrides in `$XDG_DATA_HOME` (resp. `$XDG_CONFIG_HOME`) outrank anything in `*_DIRS`.

Typical use:

- `update-desktop-database` walks `$XDG_DATA_DIRS/applications/` to populate the menu cache.
- The icon theme loader walks `$HOME/.icons`, then `$XDG_DATA_DIRS`/icons, etc. (different but related spec).
- GSettings backend `dconf` resolves schemas via `$XDG_DATA_DIRS/glib-2.0/schemas/`.

A distributor that wants their own defaults before the upstream `/usr/share` can prepend to the system-level defaults in `/etc/profile.d/` or login-manager defaults.

### 3.7 `$HOME/.local/bin` — the user executable directory

> **There is no `$XDG_BIN_HOME`.** The spec simply notes that user-specific executables may be stored in `$HOME/.local/bin`, and recommends distributions add this directory to `$PATH`.

`$HOME/.local` is intended to be analogous to `/usr/local`; the convention `…/bin`, `…/share`, `…/state`, `…/lib` mimics FHS, but flat inside the user's home.

**Caveat from the spec:** *Since `$HOME` might be shared between systems of different architectures, installing compiled binaries to `$HOME/.local/bin` could cause problems when used on systems of differing architectures.* Stick to source-level tools (shell scripts, Python, Go tooling) here, or make sure the directory is per-architecture (e.g. `$HOME/.local/x86_64-linux-gnu/bin`).

---

## 4. Resolution rules and precedence

The spec codifies several generic behaviours. Concrete rules implemented in conformant software:

1. **Absolute paths only.** Anything non-absolute in an `XDG_*` variable is ignored; the default is used.
2. **Defaults only when unset or empty.** A variable set to an empty string is treated exactly the same as an unset variable.
3. **User dir outranks system dirs.** For data: `$XDG_DATA_HOME` is more important than any single directory in `$XDG_DATA_DIRS`. For config: `$XDG_CONFIG_HOME` > any single directory in `$XDG_CONFIG_DIRS`.
4. **Order inside `_DIRS` lists is preference order.** Earlier = more important. When the same information is present in multiple directories, the spec leaves the merge / shadow rule to *the referencing spec*, but the "first wins" convention is by far the most common.
5. **Canonicalisation.** `$HOME` should be expanded without resolving symlinks (so that `~` keeps working as a token). Paths inside `*_HOME` should *not* contain `..` or `.` segments when stored as a literal.
6. **Skip inaccessible entries.** If a directory in any of the paths cannot be read, the program should silently skip it rather than abort.

Resolution algorithm frequently seen in the wild (in pseudocode):

```text
# For a single-base lookup (write target):
data_home = if XDG_DATA_HOME absolute? XDG_DATA_HOME : $HOME/.local/share

# For a search (data lookup at relative path 'foo/bar'):
for each in [data_home, *XDG_DATA_DIRS]:
    try  $each/foo/bar
    stop on first match that exists and is readable

# Equivalently for config.
```

---

## 5. Handling missing paths and permission bits

A few additional rules from the spec that programmes are *required* to handle gracefully:

- **Writing a file**: when the destination directory does not exist, the application should attempt to create it with mode `0700`. If the directory already exists, its mode must **not** be changed.
- **Permission refusal during write** (directory missing *and* cannot be created, *or* any other I/O failure): the application may present an error to the user but should not crash.
- **Reading a file**: if a particular directory is inaccessible (non-existent, missing file, no permission), the application should silently skip it. Only after *all* candidate directories have failed should it raise an error.
- **`XDG_RUNTIME_DIR` safety net**: programs that need it (e.g. for `AF_UNIX` sockets) and find it unset must fall back to a directory with similar properties (typically `TMPDIR` or `/tmp`) **and emit a warning**.

Practical permissions table for the default tree:

```
$HOME/.local/share/   0700   $XDG_DATA_HOME
$HOME/.config/        0700   $XDG_CONFIG_HOME
$HOME/.cache/         0700   $XDG_CACHE_HOME
$HOME/.local/state/   0700   $XDG_STATE_HOME
$HOME/.local/bin/     0755   user executables
$XDG_RUNTIME_DIR      0700   owned by user (system-managed)
```

The spec explicitly says that `$XDG_CACHE_HOME` may be recreated with mode `0700` only when non-existent; existing permissions must be preserved. This avoids fighting `chmod`s done by over-eager installers.

---

## 6. Setting the variables in practice

The spec defines what the variables **do**, not where they should be **set**. Three rules of thumb:

- **`$XDG_RUNTIME_DIR`** — leave it to `pam_systemd` (or your distribution's equivalent). Touching it manually is rarely useful and often breaks PipeWire, Chromium's screen sharing, *etc.*
- **The other six** — set them yourself, system-wide or per-user, as you prefer. The defaults are sane, so the only good reason to set them is to **relocate** that class of files (to an SSD vs. HDD, to an encrypted volume, to a tmpfs, *etc.*).

Common system-wide locations to set them:

| Scope                | File                              | Notes                          |
|----------------------|-----------------------------------|--------------------------------|
| System default       | `/etc/environment`                | set with simple `KEY=VALUE` lines (no `export`) |
| Login shell default  | `/etc/profile.d/xdg.sh`           | must `export` the variables    |
| Per user, interactive login | `~/.bash_profile` / `~/.profile` | `export XDG_DATA_HOME=…`     |
| Per user, GUI login (no shell) | `~/.config/environment.d/` (loaded by `systemd --user` since v254) | preferred modern location |
| Per session, dynamic | `pam_env(8)` via `/etc/security/pam_env.conf` | works for graphical sessions too |

A typical `/etc/profile.d/xdg.sh`:

```sh
# Move the XDG base dirs off the home directory (e.g. onto btrfs subvolumes).
export XDG_DATA_HOME="/data/user/${USER}/share"
export XDG_CONFIG_HOME="/data/user/${USER}/config"
export XDG_CACHE_HOME="/data/user/${USER}/cache"
export XDG_STATE_HOME="/data/user/${USER}/state"
# XDG_DATA_DIRS and XDG_CONFIG_DIRS are usually best left at the system default.
export PATH="$HOME/.local/bin:$PATH"
```

And a tiny drop-in for the runtime dir warning that some unprivileged sessions produce:

```sh
# Provide a fallback only if systemd missed it.
if [ -z "${XDG_RUNTIME_DIR}" ]; then
    XDG_RUNTIME_DIR="/tmp/xdg-runtime-${USER}"
    mkdir -p -m 0700 "$XDG_RUNTIME_DIR"
    export XDG_RUNTIME_DIR
fi
```

> **Don't blindly override `XDG_DATA_DIRS` / `XDG_CONFIG_DIRS`** at login: many programmes call into them through `glib`, and an empty value reverts to the spec default. To *prepend*, you must include the default:

```sh
export XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:/opt/mycompany/share"
```

---

## 7. Shell and C examples

### 7.1 Bash — resolving the XDG vars robustly

```bash
# data_dir / config_dir / cache_dir / state_dir — pick up user overrides,
# fall back to the spec defaults, refuse relative paths.
xdg_resolve() {
    local name="$1" default="$2"
    local v="${!name}"
    if [ -n "$v" ] && case "$v" in /*) true ;; *) false ;; esac; then
        printf '%s\n' "$v"
    else
        printf '%s\n' "$default"
    fi
}

export XDG_DATA_HOME=$(xdg_resolve XDG_DATA_HOME   "$HOME/.local/share")
export XDG_CONFIG_HOME=$(xdg_resolve XDG_CONFIG_HOME "$HOME/.config")
export XDG_CACHE_HOME=$(xdg_resolve XDG_CACHE_HOME  "$HOME/.cache")
export XDG_STATE_HOME=$(xdg_resolve XDG_STATE_HOME  "$HOME/.local/state")

# `xdg-user-dir` ships as part of xdg-user-dirs (uses XDG_CONFIG_HOME/user-dirs.dirs).
DESKTOP_DIR="$(xdg-user-dir DESKTOP 2>/dev/null || printf '%s\n' "$HOME/Desktop")"
DOWNLOAD_DIR="$(xdg-user-dir DOWNLOAD 2>/dev/null || printf '%s\n' "$HOME/Downloads")"
```

The official shell-script idiom recommended by the freedesktop wiki for reading `user-dirs.dirs`:

```bash
test -f "${XDG_CONFIG_HOME:-$HOME/.config}/user-dirs.dirs" \
    && . "${XDG_CONFIG_HOME:-$HOME/.config}/user-dirs.dirs"
echo "${XDG_DESKTOP_DIR:-$HOME/Desktop}"
echo "${XDG_DOWNLOAD_DIR:-$HOME}"
```

### 7.2 C — full XDG helper (no library)

```c
/* SPDX-License-Identifier: 0BSD  --  xdg.c
 * A minimal, spec-conformant XDG helper.
 *
 * Build & try:
 *   gcc -O2 -Wall -Wextra -o demo demo.c xdg.c && ./demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

static const char *getenv_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    if (!v || *v == '\0')                     return fallback;
    if (v[0] != '/')                          return fallback;   /* must be absolute */
    return v;
}

const char *xdg_data_home   (void) { static char *p; if(!p) p=strdup(getenv_or("XDG_DATA_HOME",
                                                                             getenv("HOME")?:""));
                                       return p?p:""; }
const char *xdg_config_home (void) { static char *p; if(!p) p=strdup(getenv_or("XDG_CONFIG_HOME",
                                                                             getenv("HOME")?:""));
                                       return p?p:""; }
const char *xdg_cache_home  (void) { static char *p; if(!p) p=strdup(getenv_or("XDG_CACHE_HOME",
                                                                             getenv("HOME")?:""));
                                       return p?p:""; }
const char *xdg_state_home  (void) { static char *p; if(!p) p=strdup(getenv_or("XDG_STATE_HOME",
                                                                             getenv("HOME")?:""));
                                       return p?p:""; }

/* Resolve "~/foo/bar" into the user-config dir. Caller frees. */
char *xdg_user_path(const char *suffix) {
    const char *base = xdg_config_home();
    char *out = NULL;
    if (asprintf(&out, "%s/%s", base, suffix) < 0) return NULL;
    return out;
}

/* Create the leaf directory (mode 0700) if it doesn't already exist.
 * Leaves an existing directory's mode untouched, per spec. */
int xdg_mkdir_p(const char *path) {
    if (!mkdir(path, 0700) || errno == EEXIST) return 0;
    if (errno != ENOENT) return -1;

    /* walk parents */
    char *dup = strdup(path);
    if (!dup) return -1;
    for (char *p = dup + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(dup, 0700) && errno != EEXIST) { free(dup); return -1; }
            *p = '/';
        }
    }
    free(dup);
    return 0;
}
```

### 7.3 Python — `os.environ` idioms

```python
import os
from pathlib import Path

def xdg_path(name: str, default: Path) -> Path:
    val = os.environ.get(name)
    if not val or not os.path.isabs(val):
        return default
    return Path(val)

data   = xdg_path("XDG_DATA_HOME",   Path.home() / ".local" / "share")
config = xdg_path("XDG_CONFIG_HOME", Path.home() / ".config")
cache  = xdg_path("XDG_CACHE_HOME",  Path.home() / ".cache")
state  = xdg_path("XDG_STATE_HOME",  Path.home() / ".local" / "state")

# Python's platformdirs / xdg libraries do exactly this; prefer them in real code.
```

### 7.4 Rust — write a library the right way

```rust
use std::path::PathBuf;

pub fn xdg_home(env: &str, default_suffix: &str) -> PathBuf {
    match std::env::var_os(env) {
        Some(s) if !s.is_empty() => {
            let p = PathBuf::from(s);
            if p.is_absolute() { p } else { fallback(default_suffix) }
        }
        _ => fallback(default_suffix),
    }
}
fn fallback(suffix: &str) -> PathBuf {
    let home = std::env::var_os("HOME").expect("HOME not set");
    PathBuf::from(home).join(suffix)
}

pub fn data_home()   -> PathBuf { xdg_home("XDG_DATA_HOME",   ".local/share") }
pub fn config_home() -> PathBuf { xdg_home("XDG_CONFIG_HOME", ".config") }
pub fn cache_home()  -> PathBuf { xdg_home("XDG_CACHE_HOME",  ".cache") }
pub fn state_home()  -> PathBuf { xdg_home("XDG_STATE_HOME",  ".local/state") }
```

---

## 8. Related specifications

The Base Directory spec is one of a small family maintained at `specifications.freedesktop.org`:

| Spec                                              | What it does                                          |
|---------------------------------------------------|-------------------------------------------------------|
| [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/) | Where files live (this document). |
| [XDG User Directories](https://www.freedesktop.org/wiki/Software/xdg-user-dirs/)         | Well-known user folders: `Desktop`, `Documents`, `Downloads`, `Music`, `Pictures`, `Videos`, `Templates`, `PublicShare`. Tool: `xdg-user-dirs-update`; config: `$XDG_CONFIG_HOME/user-dirs.dirs`. |
| [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/) | The `.desktop` file format used in `$XDG_DATA_DIRS/applications/`. |
| [Menu Specification](https://specifications.freedesktop.org/menu-spec/)                    | How the categories in `.menu` files combine into the application menu. |
| [Icon Theme Specification](https://specifications.freedesktop.org/icon-theme-spec/)        | Where and how icon lookups happen. |
| [MIME Applications Specification](https://specifications.freedesktop.org/mime-apps-spec/)  | Default applications for MIME types (`$XDG_CONFIG_HOME/mimeapps.list`). |
| [Recent File Specification](https://specifications.freedesktop.org/recent-file-spec/)      | `$XDG_DATA_HOME/RecentDocuments/`. |
| [Thumbnail Specification](https://specifications.freedesktop.org/thumbnail-spec/)          | `$XDG_CACHE_HOME/thumbnails/`. |
| [Autostart Specification](https://specifications.freedesktop.org/autostart-spec/)          | `$XDG_CONFIG_HOME/autostart/*.desktop`. |
| [Portal Specification](https://flatpak.github.io/xdg-desktop-portal/)                      | Sandbox-friendly IPC; uses `$XDG_STATE_HOME`. |

Practically: **don't import just one of these**. If a programme reads `.desktop` files (e.g. via GLib's `GDesktopAppInfo`), it is almost certainly also expected to honour XDG base directories for icons, MIME defaults, and recent files.

`xdg-user-dirs` deserves its own snippet, because it overlaps with — but is not the same as — the Base Directory spec:

```ini
# $XDG_CONFIG_HOME/user-dirs.dirs  -- created by xdg-user-dirs-update
XDG_DESKTOP_DIR="$HOME/Desktop"
XDG_DOWNLOAD_DIR="$HOME/Downloads"
XDG_TEMPLATES_DIR="$HOME/Templates"
XDG_PUBLICSHARE_DIR="$HOME/Public"
XDG_DOCUMENTS_DIR="$HOME/Documents"
XDG_MUSIC_DIR="$HOME/Music"
XDG_PICTURES_DIR="$HOME/Pictures"
XDG_VIDEOS_DIR="$HOME/Videos"
```

---

## 9. Adoption status

The XDG Base Directory Specification has been broadly adopted across the Linux desktop and is the conventional way for new programs to lay out user files. **Adoption is far from universal**:

- **Most modern CLI and GUI tools** (Neovim with `stdpath('config')`, kitty, fish, fzf, ripgrep, mpv, VLC, Transmission, VS Code's portable mode, …) honour it.
- **Some legacy tools** still hardcode `~/.progname` for both config and data (notably GNU `bash`, `less` historically, `wget` historically, `git` historically). Bug reports about adding XDG support are common — Firefox famously only added full support in **Firefox 147**, closing a 21-year-old Bugzilla bug.
- **System services and distros**:
  - GNOME, KDE, XFCE, Cinnamon, MATE: full support.
  - systemd-based distros (Debian, Fedora, Ubuntu, Arch, openSUSE, …): full support via `pam_systemd(8)` for `$XDG_RUNTIME_DIR`.
- **Mobile / macOS / Windows**: the spec is irrelevant. macOS apps follow Apple's `NSLibraryDirectory` convention, Windows apps follow `SHGetKnownFolderPath`. Portability libraries (Electron, Qt, GTK, glib) translate between them.

The ArchWiki maintains a live tracking table at <https://wiki.archlinux.org/title/XDG_Base_Directory#Support>, listing each tool, its legacy path, when it adopted XDG, and workarounds (e.g. shell aliases) for tools that haven't. Reviewing it before adding a hard dependency on XDG-only paths to a script is good practice.

A useful sanity check on any GNU/Linux system:

```sh
$ env | grep ^XDG
XDG_SESSION_TYPE=wayland
XDG_SESSION_CLASS=user
XDG_SESSION_ID=4
XDG_RUNTIME_DIR=/run/user/1000
XDG_CONFIG_HOME=/home/me/.config
XDG_DATA_HOME=/home/me/.local/share
XDG_CACHE_HOME=/home/me/.cache
XDG_STATE_HOME=/home/me/.local/state

$ xdg-user-dir DESKTOP
/home/me/Desktop
```

---

## 10. OmniNet adoption

OmniNet uses the following XDG layout on Linux/Unix systems:

| Kind | Variable | Default | OmniNet path |
|------|----------|---------|--------------|
| Configuration | `$XDG_CONFIG_HOME` | `~/.config` | `omninet/config.json` |
| Data | `$XDG_DATA_HOME` | `~/.local/share` | `omninet/` (database, `certs/`, `servers/`) |
| State | `$XDG_STATE_HOME` | `~/.local/state` | `omninet/omninet.log` (rotated) |
| Cache | `$XDG_CACHE_HOME` | `~/.cache` | `omninet/` |
| Runtime | `$XDG_RUNTIME_DIR` | `/run/user/$UID` | `omninet/` (PID files) |

Key behaviours:

- Directories are created on demand with mode `0700`.
- Relative values in `XDG_*` variables are ignored and the default is used.
- If `XDG_RUNTIME_DIR` is unset, OmniNet falls back to a user-specific directory under `$TMPDIR` or `/tmp` and prints a warning.
- Existing `~/.omninet` installations continue to work (legacy mode). To migrate, run:

  ```bash
  ./build/tools/omninet-init --migrate-xdg
  ```

- The legacy override `OMNINET_DATA_DIR` is still honored and forces the single-tree layout.

---

## 11. Bibliography / further reading

- **Canonical:** *XDG Base Directory Specification*, Version 0.8, 8 May 2021. <https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html>. Authors: Waldo Bastian, Allison Karlitskaya, Lennart Poettering, Johannes Löthberg.
- ArchWiki: *XDG Base Directory*. <https://wiki.archlinux.org/title/XDG_Base_Directory> — best maintained index of which apps support the spec, plus adoption status.
- ArchWiki: *XDG user directories*. <https://wiki.archlinux.org/title/XDG_user_directories> — companion spec.
- Gentoo Wiki: *XDG*. <https://wiki.gentoo.org/wiki/XDG>.
- *xdg-user-dirs* (Freedesktop wiki). <https://www.freedesktop.org/wiki/Software/xdg-user-dirs/>.
- Ctrl.blog — *XDG Base Directory shell scripting tutorial*. <https://www.ctrl.blog/entry/xdg-basedir-scripting.html>.
- *Release notes for Firefox 147* (Phoronix, 2025). <https://www.phoronix.com/news/Firefox-147-XDG-Base-Directory> — useful as a "where adoption is going" data point.
- Bugzilla Mozilla bug 259356 — "Support for the Freedesktop.org XDG Base Directory specification". <https://bugzilla.mozilla.org/show_bug.cgi?id=259356> — long-running reference thread on the specification itself.
- *Making Bash compliant with the XDG Base Directory Specification*. <https://hiphish.github.io/blog/2020/12/27/making-bash-xdg-compliant/>.
- evybongers.nl — *XDG basedir: cleaning the clutter in my homedir*. <https://evybongers.nl/post/the-journey-to-xdg-basedir/>.
- Andrew Fyfe / Reddit discussion, *"New XDG_STATE_HOME in XDG Base Directory Spec"*. <https://www.reddit.com/r/linux/comments/ny34vs/new_xdg_state_home_in_xdg_base_directory_spec/> — on the v0.8 addition.
- HN thread "The XDG base directory specification was initially introduced in 2003". <https://news.ycombinator.com/item?id=34733353> — community reflections twenty years on.

> Author note: This document is a *summary* for use when designing or reviewing software; it deliberately tracks the spec rather than substituting for it. When the spec changes (bumped from 0.8), re-check the canonical text first.
