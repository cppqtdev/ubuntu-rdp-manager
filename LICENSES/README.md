# Licence files shipped with Ubuntu RDP Manager

| Path | Applies to |
|---|---|
| `LicenseRef-Proprietary.txt` | Ubuntu RDP Manager's own source code (placeholder — the copyright holder must insert the final terms) |
| `qt/LGPL-3.0-only.txt`, `qt/GPL-3.0-only.txt` | Qt 6.8.8 libraries, plugins and QML modules bundled under `/opt/ubuntu-rdp-manager` when distributed under the LGPL option (see `docs/licensing.md`) |
| `qt/GPL-2.0-only.txt`, `qt/Qt-GPL-exception-1.0.txt`, `qt/BSD-3-Clause.txt` | Alternative/auxiliary Qt licence texts referenced by Qt's SPDX expressions |
| `qt-sbom/*.spdx` | Qt's own SPDX software bills of materials for the shipped modules (qtbase, qtdeclarative, qtsvg, qtshadertools), listing every third-party component inside Qt (PCRE2, HarfBuzz, libpng, libjpeg, double-conversion, Unicode data, …) with its licence |

xrdp, xorgxrdp, Xfce and FreeRDP are **not** bundled; they are installed from the Ubuntu archive
under their own licences (Apache-2.0, MIT, GPL family) and their copyright files are in
`/usr/share/doc/<package>/copyright`.
