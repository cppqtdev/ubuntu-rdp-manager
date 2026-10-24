add_library(urdm_core STATIC
    ConfigSchema.cpp ConfigSchema.h
    ConfigStore.cpp ConfigStore.h
    DBusTypes.cpp DBusTypes.h
    HelperProtocol.h
    IniDocument.cpp IniDocument.h
    ListenSpec.cpp ListenSpec.h
    LogClassifier.cpp LogClassifier.h
    LogSanitizer.cpp LogSanitizer.h
    ProcParsers.cpp ProcParsers.h
    RdpNegotiation.cpp RdpNegotiation.h
    ResourceAdvisor.cpp ResourceAdvisor.h
    ServiceState.cpp ServiceState.h
    SessionLimitPolicy.cpp SessionLimitPolicy.h
    Validators.cpp Validators.h
    XrdpVersion.cpp XrdpVersion.h
)
target_include_directories(urdm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(urdm_core PUBLIC Qt6::Core Qt6::Network Qt6::DBus)
urdm_set_warnings(urdm_core)
urdm_harden(urdm_core)
