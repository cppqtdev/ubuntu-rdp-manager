# Licensing and commercial-use analysis

> This is an engineering inventory, not legal advice. Items marked **⚠ review** need confirmation
> by someone qualified before commercial distribution.

## Inventory

| Component | Version | Licence | How it is used | Shipped in our .deb? |
|---|---|---|---|---|
| Ubuntu RDP Manager (this code) | 1.0.0 | **undecided** (placeholder `LicenseRef-Proprietary`) | — | yes |
| Qt Core, Gui, DBus, Network, Concurrent, Qml, Quick, QuickControls2, Svg, OpenGL, Wayland/XCB QPA, plugins | 6.8.8 | `LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only` | dynamically linked shared libraries in `/opt/ubuntu-rdp-manager/lib` | yes |
| Qt Quick Controls FluentWinUI3 style | 6.8.8 | same as Qt (LGPL-3.0-only option) | QML style plugin | yes |
| Third-party code inside Qt (PCRE2, HarfBuzz, libpng, libjpeg, double-conversion, tinycbor, forkfd, Unicode/CLDR data, …) | per Qt SBOM | BSD-2/3, MIT, IJG, Libpng, Unicode-3.0, CC0, Apache-2.0 … | compiled into Qt libraries | yes — listed in `LICENSES/qt-sbom/*.spdx` |
| ICU | 73.2 | Unicode-3.0 (ICU licence) | Qt dependency | yes |
| xrdp | 0.9.24 (Ubuntu) | Apache-2.0 | installed from the Ubuntu archive, run as a separate service | **no** |
| xorgxrdp | 0.9.19 (Ubuntu) | MIT/X11 | idem | **no** |
| Xfce | 4.18 (Ubuntu) | GPL-2.0+/LGPL-2.0+ | idem | **no** |
| FreeRDP 3 | 3.32 (Ubuntu) | Apache-2.0 | optional test client from the archive | **no** |
| polkit, D-Bus, systemd, OpenSSL, GTK 3, Mesa, xcb … | Ubuntu | various (LGPL, MIT, Apache-2.0, …) | system libraries via `Depends:` | **no** |
| Icons in `resources/icons` | — | original work of this project | — | yes |
| ogon | — | AGPL-3.0 | **not used** (evaluated and rejected; see `compatibility.md`) | no |

No AGPL code is used. The application talks to xrdp only through its configuration files,
systemd, logind and its command-line tool `xrdp-dis`; it does not link to xrdp.

## Qt: two possible distribution routes **⚠ review**

The Qt SDK on the development machine (`~/Qt`) was installed under a **Qt commercial licence**
(the installer recorded an Enterprise/Professional licence and the
`QT-ENTERPRISE-LICENSE-AGREEMENT`). Which route applies must be decided:

1. **Commercial Qt licence.** If the organisation's Qt licence covers distribution of this
   application (check the agreement's distribution/deployment terms and whether runtime
   distribution licences are required for this product category), the LGPL obligations below do
   not apply to Qt, but the commercial terms do.
2. **LGPL-3.0-only.** Qt may also be distributed under LGPLv3. The packaging was designed to
   allow this: Qt is dynamically linked and lives in separate, replaceable `.so` files under
   `/opt/ubuntu-rdp-manager/lib`; the application is not code-signed or locked against library
   replacement. Obligations then include:
   * ship the LGPL-3.0 and GPL-3.0 texts (done: `share/doc/licenses/qt/`);
   * state that Qt is used under LGPL-3.0 and provide (or offer for at least three years) the
     complete corresponding source of the Qt version shipped, including any modifications (Qt
     6.8.8 is unmodified; source must still be provided/offered);
   * allow users to replace the Qt libraries (satisfied by the layout) and not restrict reverse
     engineering for debugging such modifications in the licence terms of this application;
   * preserve the third-party notices of components inside Qt (`qt-sbom/*.spdx`).
   Whether the commercial Qt licence agreement permits switching to the LGPL route for a product
   developed with the commercial SDK must also be checked.

The Qt GPL options are not suitable unless the application itself is GPL-licensed.

## Obligations for components installed from Ubuntu

xrdp, xorgxrdp, Xfce and FreeRDP are not redistributed by this package; Ubuntu supplies them
with their own copyright files. If a product bundle later ships them (e.g. an offline installer
or appliance image), Apache-2.0 requires including its licence and NOTICE files, GPL components
require source availability, etc.

## Flags requiring legal review

1. Final licence for this application's code (`LICENSES/LicenseRef-Proprietary.txt` is a
   placeholder).
2. Qt route (commercial vs LGPL) and, for LGPL, the source-offer mechanism.
3. Whether the "Ubuntu" name in the product name requires permission under Canonical's trademark
   policy for commercial distribution.
4. The FluentWinUI3 style reproduces Microsoft's Fluent design language; it is Qt's own
   implementation and licensed with Qt, but check any branding expectations.
