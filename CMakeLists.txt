cmake_minimum_required(VERSION 3.21)
project(hopy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Static single-file build ─────────────────────────────
set(BUILD_SHARED_LIBS OFF)                       # QHotkey and other deps become static too
# Static CRT (must match the static Qt runtime: Release=/MT, Debug=/MTd)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Enable LTO/IPO for non-Debug configs
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# The static Qt bundles a QPSQL SQL driver plugin whose config unconditionally
# references an imported PostgreSQL::PostgreSQL target. We don't use PostgreSQL,
# so provide a harmless stub to satisfy CMake; the QPSQL plugin is never linked
# into hopy (only the SQLite driver plugin is imported below).
if(NOT TARGET PostgreSQL::PostgreSQL)
  add_library(PostgreSQL::PostgreSQL INTERFACE IMPORTED)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Sql Svg Network Test)

# QHotkey: global hotkey library
include(FetchContent)
FetchContent_Declare(QHotkey
  GIT_REPOSITORY https://github.com/Skycoder42/QHotkey.git
  GIT_TAG 1.5.0)
set(QHOTKEY_EXAMPLES OFF CACHE BOOL "" FORCE)
# QHotkey 1.5.0 ships an old cmake_minimum_required that CMake 4 rejects; allow it.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(QHotkey)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# Business sources aggregated into a static lib shared by app and tests
# Include headers too so AUTOMOC reliably scans Q_OBJECT classes for moc.
file(GLOB_RECURSE HOPY_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM HOPY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(hopy_lib STATIC ${HOPY_SOURCES})
target_include_directories(hopy_lib PUBLIC src)
target_link_libraries(hopy_lib PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Svg Qt6::Network QHotkey::QHotkey)

# Platform system libraries
if(WIN32)
  target_link_libraries(hopy_lib PUBLIC user32)
elseif(APPLE)
  target_link_libraries(hopy_lib PUBLIC "-framework AppKit" "-framework ApplicationServices" "-framework Carbon")
elseif(UNIX)
  find_package(X11 REQUIRED)
  target_link_libraries(hopy_lib PUBLIC ${X11_LIBRARIES} ${X11_XTest_LIB})
  target_include_directories(hopy_lib PUBLIC ${X11_INCLUDE_DIR})
endif()

qt_add_executable(hopy WIN32 src/main.cpp resources/hopy.qrc)  # WIN32: GUI subsystem, no console
target_link_libraries(hopy PRIVATE hopy_lib)

# Static Qt: link platform/SQLite/image plugins in, else the single file can't find qwindows/qsqlite
if(NOT BUILD_SHARED_LIBS)
  qt_import_plugins(hopy
    INCLUDE Qt6::QWindowsIntegrationPlugin
    INCLUDE_BY_TYPE sqldrivers Qt6::QSQLiteDriverPlugin
    INCLUDE_BY_TYPE imageformats Qt6::QICOPlugin Qt6::QJpegPlugin)
endif()

enable_testing()
add_subdirectory(tests)
