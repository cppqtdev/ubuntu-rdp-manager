cmake_minimum_required(VERSION 3.21)

project(UbuntuRdpManager
    VERSION 1.0.1
    DESCRIPTION "Ubuntu multi-user remote desktop (xrdp) manager"
    HOMEPAGE_URL "https://example.invalid/ubuntu-rdp-manager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(URDM_BUILD_GUI "Build the Qt Quick management application" ON)
option(URDM_BUILD_HELPER "Build the privileged D-Bus helper" ON)
option(URDM_BUILD_TESTS "Build unit and integration tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Network Concurrent)
if(URDM_BUILD_GUI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick QuickControls2)
endif()
if(URDM_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.8)

# Install layout. The application bundles its own Qt 6.8 runtime (Ubuntu 24.04 ships Qt 6.4,
# which lacks FluentWinUI3), so everything except system integration files lives in /opt.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/ubuntu-rdp-manager" CACHE PATH "Install prefix" FORCE)
endif()
set(URDM_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(URDM_LIBEXEC_DIR "${URDM_PREFIX}/libexec")
set(URDM_DATA_DIR "${URDM_PREFIX}/share")
set(URDM_STATE_DIR "/var/lib/ubuntu-rdp-manager" CACHE PATH "Helper state (configuration backups)")
set(URDM_LOG_DIR "/var/log/ubuntu-rdp-manager" CACHE PATH "Helper audit log directory")

set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

include(cmake/CompilerWarnings.cmake)

add_subdirectory(src/core)
if(URDM_BUILD_HELPER)
    add_subdirectory(src/helper)
endif()
if(URDM_BUILD_GUI)
    add_subdirectory(src/app)
endif()
if(URDM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(packaging)
