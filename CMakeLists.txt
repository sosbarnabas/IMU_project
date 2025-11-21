cmake_minimum_required(VERSION 3.19)
project(ExoGUI LANGUAGES CXX)

# Qt location (can be overridden with -DQt6_DIR=... )
set(Qt6_DIR "D:/Samu/Qt_m/6.9.3/msvc2022_64/lib/cmake/Qt6" CACHE STRING "Qt6 cmake dir")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Qt auto-generators
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6 packages
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets Svg PrintSupport Concurrent)
find_package(redis++ REQUIRED COMPONENTS)
find_package(hiredis)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(ExoGUI
         MACOSX_BUNDLE

        Application/main.cpp
        Application/MainWindow.cpp
        Application/MainWindow.h
        Application/StartupConfigDialog.cpp
        Application/StartupConfigDialog.h

        Widgets/TopBar/IconTab.cpp
        Widgets/TopBar/TopBar.cpp
        Widgets/ToggleSwitch/ToggleSwitch.cpp
        Widgets/SlotWidget/SlotWidget.cpp
        Widgets/Settings/ControlRow.cpp
        Widgets/UIUtilities/StyleHelper.cpp
        Widgets/LivePlotWidget/LivePlotWidget.cpp
        Widgets/LivePlotWidget/LivePlotWidget.h
        Widgets/LivePlotWidget/MotorColorScheme.h

        Pages/HomePage/HomePage.cpp
        Pages/SettingsPage/SettingsPage.cpp
        Pages/ArmSettingsPage/ArmSettingsPage.cpp
        Pages/ArmSettingsPage/ArmSettingsPage.h

        ThirdParty/QCustomPlot/qcustomplot.cpp
        Widgets/FunctionEditorWidget/FunctionEditorWidget.cpp
        Pages/SpringPage/SpringPage.cpp

        # Redis - Config
        Core/Redis/Config/RedisKeys.h
        Core/Redis/Config/Settings.cpp
        Core/Redis/Config/Settings.h
        Core/Redis/Config/UserParams.cpp
        Core/Redis/Config/UserParams.h
        Core/Redis/Config/EnvLoader.cpp
        Core/Redis/Config/EnvLoader.h

        # Redis - Client
        Core/Redis/Client/CommandClient.cpp
        Core/Redis/Client/CommandClient.h
        Core/Redis/Client/RedisTelemetryClient.cpp
        Core/Redis/Client/RedisTelemetryClient.h
        Core/Redis/Client/TelemetryController.cpp
        Core/Redis/Client/TelemetryController.h

        # Redis - Data
        Core/Redis/Data/RedisFacade.cpp
        Core/Redis/Data/RedisFacade.h
        Core/Redis/Data/SlotFunctionManager.cpp
        Core/Redis/Data/SlotFunctionManager.h

        # Redis - Infrastructure
        Core/Redis/Infrastructure/RedisDockerManager.cpp
        Core/Redis/Infrastructure/RedisDockerManager.h
        Core/Redis/Infrastructure/RedisTools.cpp
        Core/Redis/Infrastructure/RedisTools.h

        Widgets/Buttons/ActionButtons.cpp
        Widgets/Buttons/MotorDataLogger.cpp
        Widgets/Buttons/MotorDataLogger.h

        Widgets/ImuWidget/ImuWidget.cpp
        Widgets/ImuWidget/ImuWidget.h

)

add_executable(TelemetrySimulator
        Core/Redis/Testing/TelemetrySimulator.cpp
)

target_include_directories(TelemetrySimulator PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/Redis
        ${CMAKE_SOURCE_DIR}/Core/Redis/Client
        ${CMAKE_SOURCE_DIR}/Core/Redis/Config
        ${CMAKE_SOURCE_DIR}/Core/Redis/Data
        ${CMAKE_SOURCE_DIR}/Core/Redis/Infrastructure
        ${CMAKE_SOURCE_DIR}/Core/Redis/Testing
)

target_link_libraries(TelemetrySimulator PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Svg
        Qt6::PrintSupport
        redis++::redis++
        nlohmann_json::nlohmann_json
        hiredis::hiredis
)

# Include paths
target_include_directories(ExoGUI PRIVATE
        ${CMAKE_SOURCE_DIR}/Application
        ${CMAKE_SOURCE_DIR}/Widgets/TopBar
        ${CMAKE_SOURCE_DIR}/Widgets/ToggleSwitch
        ${CMAKE_SOURCE_DIR}/Widgets/SlotWidget
        ${CMAKE_SOURCE_DIR}/Widgets/Settings
        ${CMAKE_SOURCE_DIR}/Widgets/UIUtilities
        ${CMAKE_SOURCE_DIR}/Widgets/LivePlotWidget
        ${CMAKE_SOURCE_DIR}/Pages/HomePage
        ${CMAKE_SOURCE_DIR}/Pages/SettingsPage
        ${CMAKE_SOURCE_DIR}/Pages/ArmSettingsPage
        ${CMAKE_SOURCE_DIR}/ThirdParty/QCustomPlot
        ${CMAKE_SOURCE_DIR}/Widgets/FunctionEditorWidget
        ${CMAKE_SOURCE_DIR}/Pages/SpringPage
        ${CMAKE_SOURCE_DIR}/Core/Redis
        ${CMAKE_SOURCE_DIR}/Core/Redis/Client
        ${CMAKE_SOURCE_DIR}/Core/Redis/Config
        ${CMAKE_SOURCE_DIR}/Core/Redis/Data
        ${CMAKE_SOURCE_DIR}/Core/Redis/Infrastructure
        ${CMAKE_SOURCE_DIR}/Widgets/Buttons
        ${CMAKE_SOURCE_DIR}/Widgets/ImuWidget
)

# Define-ok
target_compile_definitions(ExoGUI PRIVATE QCP_NO_PRINTING)

# Link with Qt6
target_link_libraries(ExoGUI PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Svg
        Qt6::PrintSupport
        Qt6::Concurrent
        redis++::redis++
        nlohmann_json::nlohmann_json
        hiredis::hiredis
)
# Compiler warnings
if(MSVC)
    target_compile_options(ExoGUI PRIVATE /W4)
else()
    target_compile_options(ExoGUI PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS ExoGUI
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# (Optional) windeployqt post-build:
# add_custom_command(TARGET ExoGUI POST_BUILD
#   COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe" "$<TARGET_FILE:ExoGUI>"
#   VERBATIM)

