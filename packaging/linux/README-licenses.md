# Linux dependency notices

DEB and Arch packages dynamically link Qt from system packages. The distribution
provides their licence notices, corresponding source and security updates. Qt is
used under LGPL-3.0 (or GPL-3.0 at your option); replacing or debugging modified
LGPL libraries is permitted by the application licence.

The Rust bundle in `licenses/rust/` is generated for the Linux production bridge
graph from the locked dependencies. It includes exact versions, full original
licence/copyright/NOTICE files and their SHA256 inventory.

AppImage bundles Qt instead of requiring system Qt. Its corresponding source,
licences and replacement instructions must be included in the AppDir. Extract
with `--appimage-extract` to inspect or replace dynamically linked Qt libraries.

No package changes user preferences during removal. Preferences use
`$XDG_CONFIG_HOME/VYNX/QR/settings.json` when the variable is an absolute path,
otherwise `$HOME/.config/VYNX/QR/settings.json`. Relative/empty XDG values are
ignored. With neither a usable XDG path nor HOME, loading uses defaults and saving
reports an error. AppImage also uses these paths, not its own directory.
