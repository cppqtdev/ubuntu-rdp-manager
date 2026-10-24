# Core logic is a library so unit tests can link it with fake SystemOps/Authorizer.
add_library(urdm_helper_core STATIC
    Authorizer.h
    AuditLog.cpp AuditLog.h
    HelperCore.cpp HelperCore.h
    SystemOps.h
)
target_include_directories(urdm_helper_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(urdm_helper_core PUBLIC urdm_core)
urdm_set_warnings(urdm_helper_core)
urdm_harden(urdm_helper_core)

qt_add_executable(urdm-helper
    main.cpp
    HelperService.cpp HelperService.h
    LinuxSystemOps.cpp LinuxSystemOps.h
    PackageJob.cpp PackageJob.h
    PolkitAuthorizer.cpp PolkitAuthorizer.h
)
target_link_libraries(urdm-helper PRIVATE urdm_helper_core Qt6::DBus)
target_compile_definitions(urdm-helper PRIVATE
    URDM_VERSION="${PROJECT_VERSION}"
    URDM_STATE_DIR="${URDM_STATE_DIR}"
    URDM_DATA_DIR="${URDM_DATA_DIR}"
    URDM_LOG_DIR="${URDM_LOG_DIR}")
urdm_set_warnings(urdm-helper)
urdm_harden(urdm-helper)
# Absolute RPATH (not $ORIGIN) for the root helper: it must never resolve libraries relative
# to a location an attacker could influence.
set_target_properties(urdm-helper PROPERTIES INSTALL_RPATH "${URDM_PREFIX}/lib")
install(TARGETS urdm-helper RUNTIME DESTINATION libexec)
