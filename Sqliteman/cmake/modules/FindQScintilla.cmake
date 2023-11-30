# - Try to find the QScintilla2 includes and library
# which defines
#
# QSCINTILLA_FOUND - system has QScintilla2
# QSCINTILLA_INCLUDE_DIR - where to find qextscintilla.h
# QSCINTILLA_LIBRARIES - the libraries to link against to use QScintilla
# QSCINTILLA_LIBRARY - where to find the QScintilla library (not for general use)

# copyright (c) 2007 Thomas Moenicke thomas.moenicke@kdemail.net
#
# Redistribution and use is allowed according to the terms of the FreeBSD license.


MESSAGE (STATUS "CMAKE_CURRENT_LIST_DIR is " ${CMAKE_CURRENT_LIST_DIR})

SET(QSCINTILLA_FOUND FALSE)

MESSAGE (STATUS "Looking for qsciglobal.h in " /usr/include/${QTLCVERSION}/Qsci " " /usr/include " " /usr/include/Qsci )
FIND_PATH(QSCINTILLA_INCLUDE_DIR qsciglobal.h
/usr/include/${QTLCVERSION}/Qsci /usr/include /usr/include/Qsci)
MESSAGE (STATUS "QSCINTILLA_INCLUDE_DIR is " ${QSCINTILLA_INCLUDE_DIR})

SET(QSCINTILLA_NAMES ${QSCINTILLA_NAMES} libqscintilla2_${QTLCVERSION}.so)
SET(QSCINTILLA_PATHS ${QSCINTILLA_PATHS} ${_${QTLCVERSION}_install_prefix} ${_${QTLCVERSION}_install_prefix}/..)
MESSAGE (STATUS "Looking for qscintilla library in NAMES ${QSCINTILLA_NAMES} PATHS ${QSCINTILLA_PATHS}" )
FIND_LIBRARY(QSCINTILLA_LIBRARY
    NAMES ${QSCINTILLA_NAMES}
    PATHS ${QSCINTILLA_PATHS})
MESSAGE (STATUS "QSCINTILLA_LIBRARY is " ${QSCINTILLA_LIBRARY})

IF (QSCINTILLA_LIBRARY AND QSCINTILLA_INCLUDE_DIR)

    SET(QSCINTILLA_LIBRARIES ${QSCINTILLA_LIBRARY})
    SET(QSCINTILLA_FOUND TRUE)

    IF (CYGWIN)
        IF(BUILD_SHARED_LIBS)
        # No need to define QSCINTILLA_USE_DLL here, because it's default for Cygwin.
        ELSE(BUILD_SHARED_LIBS)
        SET (QSCINTILLA_DEFINITIONS -DQSCINTILLA_STATIC)
        ENDIF(BUILD_SHARED_LIBS)
    ENDIF (CYGWIN)

ENDIF (QSCINTILLA_LIBRARY AND QSCINTILLA_INCLUDE_DIR)

IF (QSCINTILLA_FOUND)
    MESSAGE(STATUS "Found QScintilla2: ${QSCINTILLA_LIBRARY}")
    MESSAGE(STATUS "         includes: ${QSCINTILLA_INCLUDE_DIR}")
ELSE (QSCINTILLA_FOUND)
  IF (QScintilla_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find QScintilla library")
  ENDIF (QScintilla_FIND_REQUIRED)
ENDIF (QSCINTILLA_FOUND)

MARK_AS_ADVANCED(QSCINTILLA_INCLUDE_DIR QSCINTILLA_LIBRARY)

