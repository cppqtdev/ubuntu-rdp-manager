# System integration files. Absolute destinations are intentional: D-Bus, polkit and systemd
# only look in system locations.
configure_file(dbus/com.urdm.Helper1.service.in ${CMAKE_CURRENT_BINARY_DIR}/com.urdm.Helper1.service @ONLY)
configure_file(systemd/urdm-helper.service.in ${CMAKE_CURRENT_BINARY_DIR}/urdm-helper.service @ONLY)

if(URDM_BUILD_HELPER)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.urdm.Helper1.service DESTINATION /usr/share/dbus-1/system-services)
    install(FILES dbus/com.urdm.Helper1.conf DESTINATION /usr/share/dbus-1/system.d)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/urdm-helper.service DESTINATION /usr/lib/systemd/system)
    install(FILES polkit/com.urdm.policy DESTINATION /usr/share/polkit-1/actions)
    install(PROGRAMS startwm/startwm-xfce.sh DESTINATION "${URDM_DATA_DIR}/startwm")
    # Inside remote desktops: a "Disconnect" menu entry and (once per user) desktop icon.
    install(PROGRAMS remote/urdm-disconnect.sh remote/urdm-remote-shortcut.sh DESTINATION "${URDM_DATA_DIR}/remote")
    install(FILES desktop/urdm-disconnect.desktop DESTINATION /usr/share/applications)
    install(FILES desktop/urdm-remote-shortcut.desktop DESTINATION /etc/xdg/autostart)
endif()

if(URDM_BUILD_GUI)
    install(FILES desktop/ubuntu-rdp-manager.desktop DESTINATION /usr/share/applications)
    install(FILES ${PROJECT_SOURCE_DIR}/resources/icons/ubuntu-rdp-manager.svg
            DESTINATION /usr/share/icons/hicolor/scalable/apps)
    # /usr/bin entry point
    install(CODE "
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}/usr/bin\")
        file(CHMOD \"\$ENV{DESTDIR}/usr/bin\" DIRECTORY_PERMISSIONS
             OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
        # qt.conf is written by Qt's deploy script with the builder's umask
        if(EXISTS \"\$ENV{DESTDIR}${URDM_PREFIX}/bin/qt.conf\")
            file(CHMOD \"\$ENV{DESTDIR}${URDM_PREFIX}/bin/qt.conf\" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
        endif()
        file(CREATE_LINK \"${URDM_PREFIX}/bin/ubuntu-rdp-manager\" \"\$ENV{DESTDIR}/usr/bin/ubuntu-rdp-manager\" SYMBOLIC)
    ")
endif()

install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION "${URDM_DATA_DIR}/doc")
install(DIRECTORY ${PROJECT_SOURCE_DIR}/docs/ DESTINATION "${URDM_DATA_DIR}/doc")
install(DIRECTORY ${PROJECT_SOURCE_DIR}/LICENSES/ DESTINATION "${URDM_DATA_DIR}/doc/licenses")

# ---------------------------------------------------------------------------------------------
# Debian package (cpack -G DEB). Runtime dependencies were derived from ldd over every bundled
# library and plugin in a pristine ubuntu:24.04 container (packaging/debian/runtime-depends.txt).
# ---------------------------------------------------------------------------------------------
file(STRINGS debian/runtime-depends.txt URDM_RUNTIME_DEPENDS)
list(APPEND URDM_RUNTIME_DEPENDS
    "polkitd" "pkexec" "dbus" "systemd" "openssl" "passwd" "tar" "fonts-dejavu-core")
list(JOIN URDM_RUNTIME_DEPENDS ", " URDM_DEPENDS_STRING)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "ubuntu-rdp-manager")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Ubuntu RDP Manager")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Manage multi-user remote desktop (xrdp) on Ubuntu")
set(CPACK_PACKAGE_DESCRIPTION "Graphical administration tool for the xrdp RDP server: installation, service
control, session limits, concurrent multi-user sessions, access control, TLS, logs, diagnostics and
capacity monitoring. Privileged operations run in a separate polkit-authorized D-Bus helper.
Bundles the Qt 6.8 runtime under /opt/ubuntu-rdp-manager.")
set(CPACK_PACKAGING_INSTALL_PREFIX "${URDM_PREFIX}")
set(CPACK_SET_DESTDIR ON)
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ubuntu RDP Manager maintainers <maintainers@example.invalid>")
set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${URDM_DEPENDS_STRING}")
# The RDP server itself comes from the Ubuntu archive (installed by the wizard on request).
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "xrdp, xorgxrdp, xfce4, freerdp3-x11")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
include(CPack)
