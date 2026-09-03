cmake_minimum_required(VERSION 3.21)
project(ESDR3_Player VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia LinguistTools)
qt_standard_project_setup(REQUIRES 6.10)

set(BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
set(BUILD_AUTOTESTS   OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS  OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(FIND_FFTW         OFF CACHE BOOL "" FORCE)
set(_automoc_save ${CMAKE_AUTOMOC})
set(CMAKE_AUTOMOC OFF)
add_subdirectory(third_party/liquid-dsp EXCLUDE_FROM_ALL)
set(CMAKE_AUTOMOC ${_automoc_save})

get_property(_liquid_targets DIRECTORY third_party/liquid-dsp PROPERTY BUILDSYSTEM_TARGETS)
foreach(_t IN LISTS _liquid_targets)
    get_target_property(_type ${_t} TYPE)
    if(NOT _type STREQUAL "INTERFACE_LIBRARY" AND NOT _type STREQUAL "UTILITY" AND NOT MSVC)
        target_compile_options(${_t} PRIVATE -Wno-unused-variable)
    endif()
endforeach()

add_library(esdr3core STATIC
    src/core/Types.h
    src/core/SpscRing.h
    src/core/TailBuffer.h
    src/core/WavReader.h        src/core/WavReader.cpp
    src/core/PlaybackEngine.h   src/core/PlaybackEngine.cpp
    src/dsp/PsdComputer.h       src/dsp/PsdComputer.cpp
    src/dsp/SpectrumWorker.h    src/dsp/SpectrumWorker.cpp
    src/dsp/Demodulator.h       src/dsp/Demodulator.cpp
)
target_include_directories(esdr3core PUBLIC src third_party/liquid-dsp/include)
target_link_libraries(esdr3core PUBLIC Qt6::Core liquid-static)
if(NOT MSVC)
    target_compile_options(esdr3core PRIVATE -Wall -Wextra)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(esdr3core PUBLIC -Wno-return-type-c-linkage)
endif()

qt_add_executable(esdr3_probe tools/probe/main.cpp)
target_link_libraries(esdr3_probe PRIVATE esdr3core)

option(ESDR3_BUILD_TESTS "Собирать тесты" ON)
if(ESDR3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

qt_add_executable(ESDR3_Player MACOSX_BUNDLE WIN32
    src/app/main.cpp
    src/audio/AudioOutput.h      src/audio/AudioOutput.cpp
    src/ui/FftMapping.h          src/ui/FftMapping.cpp
    src/ui/Palettes.h            src/ui/Palettes.cpp
)

qt_add_qml_module(ESDR3_Player
    URI ESDR3Player
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/ui/PanoramaItem.h        src/ui/PanoramaItem.cpp
        src/app/PlayerController.h   src/app/PlayerController.cpp
)

target_include_directories(ESDR3_Player PRIVATE src/ui src/app)

qt_add_translations(ESDR3_Player
    TS_FILES i18n/ESDR3_Player_ru.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete -locations none
)

target_link_libraries(ESDR3_Player PRIVATE
    esdr3core
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia
)
if(NOT MSVC)
    target_compile_options(ESDR3_Player PRIVATE -Wall -Wextra)
endif()

target_compile_definitions(ESDR3_Player PRIVATE ESDR3_VERSION="${PROJECT_VERSION}")

set_target_properties(ESDR3_Player PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER net.vk6nx.esdr3player
    MACOSX_BUNDLE_BUNDLE_NAME ESDR3_Player
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

set(ESDR3_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon/ESDR3_Player.icns")
if(APPLE AND EXISTS "${ESDR3_ICNS}")
    target_sources(ESDR3_Player PRIVATE "${ESDR3_ICNS}")
    set_source_files_properties("${ESDR3_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(ESDR3_Player PROPERTIES MACOSX_BUNDLE_ICON_FILE ESDR3_Player.icns)
endif()

if(APPLE)
    add_custom_target(deploy
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/deploy.sh"
                --app "$<TARGET_BUNDLE_DIR:ESDR3_Player>"
                --macdeployqt "$<TARGET_FILE:Qt6::macdeployqt>"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        USES_TERMINAL VERBATIM)
    add_dependencies(deploy ESDR3_Player)

    add_custom_target(dmg
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/deploy.sh"
                --app "$<TARGET_BUNDLE_DIR:ESDR3_Player>"
                --macdeployqt "$<TARGET_FILE:Qt6::macdeployqt>"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                --version "${PROJECT_VERSION}" --dmg
        USES_TERMINAL VERBATIM)
    add_dependencies(dmg ESDR3_Player)
endif()

include(GNUInstallDirs)
install(TARGETS ESDR3_Player
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/ESDR3_Player.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/icon/esdr3player_256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME ESDR3_Player.png)
endif()
