# Specify the minimum version of CMake 
cmake_minimum_required(VERSION 3.5)

# Specify the project name
project(xdts_viewer VERSION 1.2.0)

set(MOC_HEADERS
    mywindow.h
    xsheetpreviewarea.h
    myparams.h
    tool.h
    dialogs.h
    aetool.h
    mixupkeydialog.h
    )

set(HEADERS
    xdtsio.h
    pathutils.h
    commandmanager.h
    ${MOC_HEADERS}
)

set(SOURCES
    main.cpp
    mywindow.cpp
    mywindow_psdexport.cpp
    xdtsio.cpp
    xsheetpreviewarea.cpp
    myparams.cpp
    tool.cpp
    selectiontool.cpp
    pathutils.cpp
    commandmanager.cpp
    linetool.cpp
    dialogs.cpp
    aetool.cpp
    mixupkeydialog.cpp
)

#-----------------------------------------------------------------------------
# Select build environment
set(BUILD_ENV_MSVC     OFF)
set(BUILD_ENV_APPLE    OFF)
if(MSVC)
    set(BUILD_ENV_MSVC ON)
    message(STATUS "Build environment is MSVC")
elseif(CMAKE_HOST_APPLE)
    set(BUILD_ENV_APPLE ON)
    message(STATUS "Build environment is APPLE")
else()
    message(ERROR "Unsupported build environment, only MSVC and APPLE are supported")
endif()

#-----------------------------------------------------------------------------
# Build Options
option(WITH_TRANSLATION "Generate translation projects as well" ON)

#-----------------------------------------------------------------------------
# Third Party & System Libs
get_filename_component(SDKROOT ../thirdparty/ ABSOLUTE)
message(STATUS "SDK Root:" ${SDKROOT})

# Third Party & System Libs
# specify qtdir
if(BUILD_ENV_MSVC)
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QTDIR})
        message("Specify QTDIR properly")
        return()
    endif()
elseif(BUILD_ENV_APPLE)
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    add_definitions(
        -DMACOSX
        -Di386
        -D__MACOS__
    )
endif()
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QTDIR};${QTDIR}/lib/cmake")

if(BUILD_ENV_APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10)
endif()

# Add Qt libraries. QtCore and QtGui libraries will be 
# added accordingly as the Widgets depends on them. 
find_package( Qt5 REQUIRED
    Widgets
    LinguistTools
)

# ビルドディレクトリを include ディレクトリに追加するオプションを有効にする。
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(MOC_SOURCES)

qt5_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})

set(SOURCES
    ${SOURCES}
    ${MOC_SOURCES}
)


# Resource file(s)
set(RESOURCES xdts_viewer_resources.qrc)
qt5_add_resources(SOURCES ${RESOURCES})

if(BUILD_ENV_MSVC)
    set(RC_FILE xdts_viewer.rc)
endif()

if(WITH_TRANSLATION)
    # generate Qt translations and messages
    set(LANGUAGES ja)
    # 翻訳対象のファイルを設定する。
    SET(TRANSLATION_TARGET_FILES
        ${HEADERS}
        ${SOURCES}
    )

    set(translation)
    foreach(lang ${LANGUAGES})
        set(name "${CMAKE_SOURCE_DIR}/translations/XDTS_Viewer_${lang}.ts")
        list(APPEND translation ${name})
        set_source_files_properties(${name} PROPERTIES
            OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/loc"
        )
    endforeach()

    qt5_create_translation(message ${translation} ${TRANSLATION_TARGET_FILES})

    add_custom_target("translation_XDTS_Viewer" DEPENDS ${message})
    set_target_properties("translation_XDTS_Viewer" PROPERTIES
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
endif()



if(BUILD_ENV_MSVC)
    include_directories( ${SDKROOT}/psd_sdk/include )
    set(PSD_SDK_LIB ${SDKROOT}/psd_sdk/lib/Psd.lib)
endif()

# 生成する実行ファイルを追加する
if(BUILD_ENV_MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RESOURCES} ${RC_FILE})
 else()
    set(MACOSX_BUNDLE_ICON_FILE "XDTS_Viewer.icns")
    set_source_files_properties(XDTS_Viewer.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE XDTS_Viewer.icns ${HEADERS} ${SOURCES} ${RESOURCES})
endif()

# link Qt library. Core and Gui will be linked accordingly
if(BUILD_ENV_MSVC)
    target_link_libraries(${PROJECT_NAME} Qt5::Widgets ${PSD_SDK_LIB} )
else()
    target_link_libraries(${PROJECT_NAME} Qt5::Widgets)
endif()

add_definitions(
    -DNOMINMAX
    -D_USE_MATH_DEFINES
)

if(BUILD_ENV_APPLE)
# info plists
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
        ${CMAKE_SOURCE_DIR}/../macOS/BundleInfo.plist.in)

# resources
    set_source_files_properties(${MACOSX_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
    add_custom_command (TARGET ${PROJECT_NAME} PRE_BUILD COMMAND mkdir -p ${PROJECT_NAME}.app/Contents/Resources)
endif()

# Set a default build type to Release
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()
