find_package(Qt6 6.8 REQUIRED COMPONENTS Svg)

qt_add_executable(ubuntu-rdp-manager
    main.cpp
    application/ApplicationController.cpp application/ApplicationController.h
    infrastructure/HelperClient.cpp infrastructure/HelperClient.h
    infrastructure/LogindClient.cpp infrastructure/LogindClient.h
    infrastructure/ProcessRunner.cpp infrastructure/ProcessRunner.h
    infrastructure/RdpProbe.cpp infrastructure/RdpProbe.h
    infrastructure/SystemdClient.cpp infrastructure/SystemdClient.h
    infrastructure/SystemSampler.cpp infrastructure/SystemSampler.h
    models/SessionListModel.cpp models/SessionListModel.h
    models/SettingsModel.cpp models/SettingsModel.h
    services/DiagnosticManager.cpp services/DiagnosticManager.h
    services/InstallationManager.cpp services/InstallationManager.h
    services/LogManager.cpp services/LogManager.h
    services/RdpConfigurationManager.cpp services/RdpConfigurationManager.h
    services/RdpServerManager.cpp services/RdpServerManager.h
    services/SessionManager.cpp services/SessionManager.h
    services/SystemMonitor.cpp services/SystemMonitor.h
    services/UserAccessManager.cpp services/UserAccessManager.h
)

target_include_directories(ubuntu-rdp-manager PRIVATE application infrastructure models services)

set(qml_dir ${PROJECT_SOURCE_DIR}/qml)
set_source_files_properties(${qml_dir}/themes/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Paths relative to this directory; QT_RESOURCE_ALIAS keeps resource paths short.
set(URDM_QML_FILES
    ../../qml/Main.qml
    ../../qml/themes/Theme.qml
    ../../qml/themes/Words.qml
    ../../qml/components/AppSidebar.qml
    ../../qml/components/Card.qml
    ../../qml/components/ConfirmDialog.qml
    ../../qml/components/DisclosureCard.qml
    ../../qml/components/EmptyState.qml
    ../../qml/components/InlineMessage.qml
    ../../qml/components/KeyValue.qml
    ../../qml/components/MetricTile.qml
    ../../qml/components/PageScaffold.qml
    ../../qml/components/SettingEditor.qml
    ../../qml/components/Sparkline.qml
    ../../qml/components/StatusPill.qml
    ../../qml/components/ToastHost.qml
    ../../qml/screens/AboutScreen.qml
    ../../qml/screens/ConfigurationScreen.qml
    ../../qml/screens/DashboardScreen.qml
    ../../qml/screens/DiagnosticsScreen.qml
    ../../qml/screens/InstallationScreen.qml
    ../../qml/screens/LogsScreen.qml
    ../../qml/screens/PerformanceScreen.qml
    ../../qml/screens/ServerControlScreen.qml
    ../../qml/screens/SessionLimitsScreen.qml
    ../../qml/screens/SessionsScreen.qml
    ../../qml/screens/UsersScreen.qml
)
foreach(f ${URDM_QML_FILES})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS qml/${name})
endforeach()
set_source_files_properties(../../qml/themes/Theme.qml ../../qml/themes/Words.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

file(GLOB icon_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/resources/icons/*.svg)
foreach(f ${icon_files})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS resources/icons/${name})
endforeach()

qt_add_qml_module(ubuntu-rdp-manager
    URI UbuntuRdpManager
    VERSION 1.0
    QML_FILES ${URDM_QML_FILES}
    RESOURCES ${icon_files}
    DEPENDENCIES QtQuick QtQuick.Controls QtQuick.Layouts
)

target_link_libraries(ubuntu-rdp-manager PRIVATE
    urdm_core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus Qt6::Network Qt6::Concurrent Qt6::Svg)
target_compile_definitions(ubuntu-rdp-manager PRIVATE URDM_VERSION="${PROJECT_VERSION}")
urdm_set_warnings(ubuntu-rdp-manager)
urdm_harden(ubuntu-rdp-manager)
set_target_properties(ubuntu-rdp-manager PROPERTIES INSTALL_RPATH "${URDM_PREFIX}/lib")

install(TARGETS ubuntu-rdp-manager RUNTIME DESTINATION bin)

# Deploy the Qt runtime (libraries, platform/image/TLS plugins and the QML modules found by
# qmlimportscanner, including QtQuick.Controls.FluentWinUI3) into the same prefix.
qt_generate_deploy_qml_app_script(
    TARGET ubuntu-rdp-manager
    OUTPUT_SCRIPT urdm_deploy_script
    NO_TRANSLATIONS
)
install(SCRIPT ${urdm_deploy_script})
# The gtk3 platform theme is how the light/dark choice reaches FluentWinUI3 on Linux (main.cpp);
# the deploy script does not include platform themes by default.
install(FILES "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platformthemes/libqgtk3.so"
        DESTINATION plugins/platformthemes)
