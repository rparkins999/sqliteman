#
# This is the base config/build file for Sqliteman
# for more info about available options read: README
#
MESSAGE(STATUS "Entering Sqliteman/CMakeLists.txt")
OPTION(WANT_INTERNAL_SQLDRIVER "Use internal sqlite3 driver instead of system one. It enables more features (extensions, r-tree, etc.)" OFF)
OPTION(WANT_DEBUG "Set the debug build and possible additional outputs" OFF)
OPTION(WANT_INTERNAL_QSCINTILLA "Use internal/bundled QScintilla2 source" OFF)
OPTION(WANT_BUNDLE "Enable Mac OS X bundle build" OFF)
OPTION(WANT_BUNDLE_STANDALONE "Do not copy required libs and tools into bundle (WANT_BUNDLE)" ON)

CMAKE_MINIMUM_REQUIRED( VERSION 3.0 )

#Pretty colors
SET(CMAKE_COLOR_MAKEFILE ON)

#Force verbose to investigate problems with build.make
#SET(CMAKE_VERBOSE_MAKEFILE ON)
SET(CMAKE_VERBOSE_MAKEFILE OFF)

#Set the custom CMake module directory where our include/lib finders are
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

#PROJECT SETUP
PROJECT(sqliteman)
SET (VERSION_MAJOR "1")
SET (VERSION_MINOR "9")
SET (VERSION_PATCH "1")
SET (VERSION_SUFFIX "")
SET (VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX})

SET (EXE_NAME "sqliteman")
IF (WANT_BUNDLE)
    SET (EXE_NAME "Sqliteman")
ENDIF (WANT_BUNDLE)

MESSAGE(STATUS "Sqliteman ${VERSION} will be installed into ${CMAKE_INSTALL_PREFIX}")

   IF (NOT LIB_SUFFIX)
       MESSAGE(STATUS "")
       MESSAGE(STATUS "LIB_SUFFIX variable is not defined. It will be autodetected now.")
       MESSAGE(STATUS "You can set it manually with -DLIB_SUFFIX=<value> (64 for example)")
       IF (NOT APPLE)
           # check 64 bit
           if (CMAKE_SIZEOF_VOID_P EQUAL 4)
               SET (LIB_SUFFIX "")
           else (CMAKE_SIZEOF_VOID_P EQUAL 4)
               set (LIB_SUFFIX "64")
               MESSAGE(STATUS "   LIB_SUFFIX is set to '${LIB_SUFFIX}'")
           endif (CMAKE_SIZEOF_VOID_P EQUAL 4)
       ELSE (NOT APPLE)
           SET (LIB_SUFFIX "")
       ENDIF (NOT APPLE)
       MESSAGE(STATUS "")
   ENDIF (NOT LIB_SUFFIX)

# extensions are switched off in some cases later
SET (ENABLE_EXTENSIONS 1)

# set platform dependent stuff - e.g. sqlite3 binary name
# NOTE: it's built with RESOURCES on Windows 
IF (WIN32)
#     SET (SQLITE_BIN "sqlite3.exe")
    SET (WANT_RESOURCES 1)
    #SET (ENABLE_EXTENSIONS 0)
    SET (WANT_INTERNAL_SQLDRIVER 1)
# ELSE (WIN32)
#     SET (SQLITE_BIN "sqlite3")
ENDIF (WIN32)

IF (APPLE)
    #SET (ENABLE_EXTENSIONS 0)
    SET (WANT_INTERNAL_SQLDRIVER 1)
ENDIF (APPLE)

IF (WANT_BUNDLE)
    SET (WANT_RESOURCES 1)
ENDIF (WANT_BUNDLE)

# set paths to install
IF (WANT_RESOURCES)
    SET (TRANSLATION_DIR ":/ts/")
    SET (DOC_DIR ":/doc/")
    SET (ICON_DIR ":/icons/")
ELSE (WANT_RESOURCES)
    SET (TRANSLATION_DIR "${CMAKE_INSTALL_PREFIX}/share/sqliteman/")
    SET (DOC_DIR "${CMAKE_INSTALL_PREFIX}/share/sqliteman/doc/")
    SET (ICON_DIR "${CMAKE_INSTALL_PREFIX}/share/sqliteman/icons")
ENDIF (WANT_RESOURCES)

SET (QTVERSION "Qt5")
SET (QTLCVERSION "qt5")
SET (QT_MT_REQUIRED true)
SET( QT_USE_QTSQL TRUE )
SET( QT_USE_QTXML TRUE )
find_package(${QTVERSION} COMPONENTS Widgets Sql REQUIRED)
MESSAGE(STATUS "Qt version: "
    ${${QTVERSION}Core_VERSION_MAJOR}.
    ${${QTVERSION}Core_VERSION_MINOR}.
    ${${QTVERSION}Core_VERSION_PATCH})

include_directories(
    ${${QTVERSION}Widgets_INCLUDE_DIRS}
    ${${QTVERSION}Sql_INCLUDE_DIRS})

add_definitions(${${QTVERSION}Core_DEFINITIONS})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# defines for compiler based on the previous setup
add_definitions(-DTRANSLATION_DIR="${TRANSLATION_DIR}")
add_definitions(-DDOC_DIR="${DOC_DIR}")
add_definitions(-DICON_DIR="${ICON_DIR}")
# defines for compiler
add_definitions(-DSQLITEMAN_VERSION="${VERSION}")
IF (WANT_RESOURCES)
    add_definitions("-DWANT_RESOURCES")
ENDIF (WANT_RESOURCES)

IF (WANT_INTERNAL_SQLDRIVER)
    MESSAGE(STATUS "Sqliteman will be built with internal Sqlite driver.")
    add_definitions("-DINTERNAL_SQLDRIVER")
    SET (ENABLE_EXTENSIONS 1)
ELSE (WANT_INTERNAL_SQLDRIVER)
    MESSAGE(STATUS "Sqliteman will be built with standard Qt Sqlite driver.")
ENDIF (WANT_INTERNAL_SQLDRIVER)

IF (DISABLE_SQLITE_EXTENSIONS)
    SET (ENABLE_EXTENSIONS 0)
ENDIF (DISABLE_SQLITE_EXTENSIONS)
IF (FORCE_SQLITE_EXTENSIONS)
    SET (ENABLE_EXTENSIONS 1)
ENDIF (FORCE_SQLITE_EXTENSIONS)
IF (ENABLE_EXTENSIONS)
    MESSAGE(STATUS "Sqliteman will be built with extension support.")
    add_definitions("-DENABLE_EXTENSIONS")
    MESSAGE(STATUS "Sqliteman will be built with R-Tree support.")
    add_definitions("-DSQLITE_ENABLE_RTREE=1")
ELSE (ENABLE_EXTENSIONS)
    MESSAGE(STATUS "Sqliteman will be built without extension support.")
ENDIF (ENABLE_EXTENSIONS)

#uninstall
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

IF (WANT_DEBUG)
    SET (CMAKE_BUILD_TYPE Debug)
    MESSAGE (STATUS " *** Debug build ***")
ELSE(WANT_DEBUG)
    SET(CMAKE_BUILD_TYPE RELEASE)
    MESSAGE(STATUS " *** Release build (use -DWANT_DEBUG=1 for debug one) ***")
ENDIF(WANT_DEBUG)

# Lets disable Wall warnings due the MSVC too much talkative
# output. Wall can slow down compiletion up to hours and
# my boxes can boil eggs too.
# MSVC, MSVC_IDE, MSVC60, MSVC70, MSVC71, MSVC80, CMAKE_COMPILER_2005
IF (MSVC)
    SET (CXX_WARNINGS "")
ELSE (MSVC)
    SET (CXX_WARNINGS "-Wall")
ENDIF (MSVC)

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fkeep-inline-functions ${CXX_WARNINGS}")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g3 ${CXX_WARNINGS}")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CXX_WARNINGS}")
SET(CMAKE_C_FLAGS_RELEASE "-O2 ${CXX_WARNINGS}")

IF (WANT_INTERNAL_QSCINTILLA)
    MESSAGE(STATUS "QSci: Building with internal copy of QScintilla.")
    MESSAGE(STATUS "QSci: It's recomended to use external library for it")
ELSE (WANT_INTERNAL_QSCINTILLA)
    FIND_PACKAGE(QScintilla REQUIRED)
    IF (QSCINTILLA_FOUND)
        MESSAGE(STATUS "QScintilla2 Found OK")
        ADD_DEFINITIONS("-DQT -DSCI_LEXER")
    ELSE (QSCINTILLA_FOUND)
        MESSAGE(STATUS "QSci: No QScintilla2 libraries found")
        MESSAGE(STATUS "QSci: You can use -DWANT_INTERNAL_QSCINTILLA=1 to build included copy of it.")
        MESSAGE(FATAL_ERROR "No QScintilla2 found")
    ENDIF (QSCINTILLA_FOUND)

ENDIF (WANT_INTERNAL_QSCINTILLA)

FIND_PACKAGE (Sqlite)
IF (SQLITE_FOUND)
	MESSAGE("Sqlite environment Found OK")
	ADD_DEFINITIONS("-DSQLITE_FOUND")
ELSE (SQLITE_FOUND)
	MESSAGE(STATUS "No Sqlite environment found - support for loadable modules is skipped")
ENDIF (SQLITE_FOUND)
MESSAGE(STATUS "SQLITE_INCLUDE_DIR:  ${SQLITE_INCLUDE_DIR}")
MESSAGE(STATUS "SQLITE_LIBRARIES:  ${SQLITE_LIBRARIES}")

ADD_SUBDIRECTORY( sqliteman )

IF (WIN32)
    MESSAGE(STATUS "Installation directories 'share' etc. aren't created in WIN32")
ELSE (WIN32)
    IF (NOT WANT_BUNDLE)
        # install desktop
        INSTALL(FILES
            sqliteman/icons/sqliteman.png
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/32x32/apps"
        )
        INSTALL(FILES
            sqliteman.desktop
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications"
        )
    ENDIF (NOT WANT_BUNDLE)
ENDIF (WIN32)

# Check that we have set things up correctly
# Can't print a property directly
get_directory_property(TEMP INCLUDE_DIRECTORIES)
MESSAGE(STATUS "INCLUDE_DIRECTORIES = '${TEMP}'")
get_directory_property(TEMP COMPILE_DEFINITIONS)
MESSAGE(STATUS "COMPILE_DEFINITIONS = '${TEMP}'")
get_directory_property(TEMP COMPILE_OPTIONS)
MESSAGE(STATUS "COMPILE_OPTIONS = '${TEMP}'")
MESSAGE(STATUS "Qt moc executable = '${QT_MOC_EXECUTABLE}'")
MESSAGE(STATUS "Leaving Sqliteman/CMakeLists.txt")
