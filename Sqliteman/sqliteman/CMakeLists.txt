MESSAGE(STATUS "Entering Sqliteman/sqliteman/CMakeLists.txt")
ADD_SUBDIRECTORY(icons)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(ts)
# Default: qsci2 as 3rd party lib!
IF (WANT_INTERNAL_QSCINTILLA)
    ADD_SUBDIRECTORY(qscintilla2)
ENDIF(WANT_INTERNAL_QSCINTILLA)
# build extensions if there is sqlite3 lib present
IF (NOT WANT_BUNDLE)
    ADD_SUBDIRECTORY(extensions)
ENDIF (NOT WANT_BUNDLE)

SET( SQLITEMAN_SRC
    altertabledialog.cpp
    altertriggerdialog.cpp
    alterviewdialog.cpp
    analyzedialog.cpp
    blobpreviewwidget.cpp
    constraintsdialog.cpp
    createindexdialog.cpp
    createtabledialog.cpp
    createtriggerdialog.cpp
    createviewdialog.cpp
    database.cpp
    dataexportdialog.cpp
    dataviewer.cpp
    dialogcommon.cpp
    extensionmodel.cpp
    finddialog.cpp
    getcolumnlist.cpp
    helpbrowser.cpp
    importtabledialog.cpp
    importtablelogdialog.cpp
    litemanwindow.cpp
    main.cpp
    multieditdialog.cpp
    mylineedit.cpp
    pd.cpp
    populatorcolumnwidget.cpp
    populatordialog.cpp
    preferences.cpp
    preferencesdialog.cpp
    queryeditordialog.cpp
    queryeditorwidget.cpp
    querystringmodel.cpp
    schemabrowser.cpp
    shortcuteditordialog.cpp
    shortcutmodel.cpp
    sqldelegate.cpp
    sqleditor.cpp
    sqleditorwidget.cpp
    sqliteprocess.cpp
    sqlitemview.cpp
    sqlkeywords.cpp
    sqlmodels.cpp
    sqlparser.cpp
    sqltableview.cpp
    tableeditordialog.cpp
    tabletree.cpp
    termstabwidget.cpp
    vacuumdialog.cpp
    utils.cpp
    sqlparser/tosqlparse.h
    sqlparser/tosqlparse.cpp
)
IF (WANT_INTERNAL_SQLDRIVER)
    INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/sqliteman/driver )
    SET (SQLITEMAN_SRC
        ${SQLITEMAN_SRC}
        driver/qsql_sqlite.cpp
    )
    SET (SQLITEMAN_MOC
        ${SQLITEMAN_MOC}
        driver/qsql_sqlite.h
    )
ELSE (WANT_INTERNAL_SQLDRIVER)
ENDIF (WANT_INTERNAL_SQLDRIVER)

# compiled in icons for windows
IF (WIN32)
    if (MINGW)
        # resource compilation for mingw
        ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sqliteman_rc.o
                           COMMAND windres.exe -I${CMAKE_CURRENT_SOURCE_DIR}
                                           -i${CMAKE_CURRENT_SOURCE_DIR}/icons/sqliteman.rc
                                           -o ${CMAKE_CURRENT_BINARY_DIR}/sqliteman_rc.o)
        SET(SQLITEMAN_SRC ${SQLITEMAN_SRC} ${CMAKE_CURRENT_BINARY_DIR}/sqliteman_rc.o)
    else(MINGW)
        SET (SQLITEMAN_SRC ${SQLITEMAN_SRC} icons/sqliteman.rc)
    endif(MINGW)
ENDIF (WIN32)

SET( SQLITEMAN_MOC
    altertabledialog.h
    altertriggerdialog.h
    alterviewdialog.h
    analyzedialog.h
    blobpreviewwidget.h
    constraintsdialog.h
    createindexdialog.h
    createtabledialog.h
    createtriggerdialog.h
    createviewdialog.h
    dataexportdialog.h
    dataviewer.h
    dialogcommon.h
    extensionmodel.h
    finddialog.h
    getcolumnlist.h
    helpbrowser.h
    importtabledialog.h
    importtablelogdialog.h
    litemanwindow.h
    multieditdialog.h
    mylineedit.h
    pd.h
    populatorcolumnwidget.h
    populatordialog.h
#     populatorstructs.h
    preferences.h
    preferencesdialog.h
    queryeditordialog.h
    queryeditorwidget.h
    querystringmodel.h
    schemabrowser.h
    shortcuteditordialog.h
    shortcutmodel.h
    sqldelegate.h
    sqleditor.h
    sqleditorwidget.h
#    sqliteprocess.h
    sqlitemview.h
    sqlmodels.h
    sqlparser.h
    sqltableview.h
    tableeditordialog.h
    tabletree.h
    termstabwidget.h
    vacuumdialog.h
)

SET( SQLITEMAN_UI
    alterviewdialog.ui
    analyzedialog.ui
    blobpreviewwidget.ui
    constraintsdialog.ui
    createtriggerdialog.ui
    dataexportdialog.ui
    dataviewer.ui
    finddialog.ui
    helpbrowser.ui
    importtabledialog.ui
    importtablelogdialog.ui
    multieditdialog.ui
    populatorcolumnwidget.ui
    populatordialog.ui
    preferencesdialog.ui
    prefsdatadisplaywidget.ui
    prefsextensionwidget.ui
    prefslnfwidget.ui
    prefssqleditorwidget.ui
    queryeditordialog.ui
    queryeditorwidget.ui
    schemabrowser.ui
    shortcuteditordialog.ui
    sqldelegateui.ui
    sqleditor.ui
    sqlitemview.ui
    tableeditordialog.ui
    termstabwidget.ui
    vacuumdialog.ui
)

IF (WANT_RESOURCES)
    SET( SQLITEMAN_RCS
        doc/doc.qrc
        icons/icons.qrc
        ts/ts.qrc
    )
ENDIF (WANT_RESOURCES)
SET (SQLITEMAN_RCS ${SQLITEMAN_RCS} api/api.qrc)
QT_ADD_RESOURCES( SQLITEMAN_RC_RCS ${SQLITEMAN_RCS} )

qt5_wrap_ui(SQLITEMAN_UI_HDRS ${SQLITEMAN_UI})
qt5_wrap_cpp(SQLITEMAN_MOC_SRC ${SQLITEMAN_MOC})

INCLUDE_DIRECTORIES( ${CMAKE_BINARY_DIR} )
INCLUDE_DIRECTORIES( ${CMAKE_BINARY_DIR}/sqliteman )
# due the "out of base directory" build. E.g. SQLITEMAN_SVN_DIR/mybuild etc.
INCLUDE_DIRECTORIES( . )
# qscintilla2
IF (WANT_INTERNAL_QSCINTILLA)
    INCLUDE_DIRECTORIES(
        ${CMAKE_SOURCE_DIR}/sqliteman/qscintilla2/Qt4
        ${CMAKE_SOURCE_DIR}/sqliteman/qscintilla2/Qt4/Qsci
    )
ELSE (WANT_INTERNAL_QSCINTILLA)
    INCLUDE_DIRECTORIES( ${QSCINTILLA_INCLUDE_DIR} )
ENDIF (WANT_INTERNAL_QSCINTILLA)

INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/sqliteman/sqlite )

SET (GUI_TYPE)
IF (MSVC)
    # this is a must to switch off subsystem:console (console window poping up)
    # WinMain wrapper from QT_QTMAIN_LIBRARY is used and liner must be modified
    SET (GUI_TYPE WIN32)
    SET (CMAKE_EXE_LINKER_FLAGS "/NODEFAULTLIB:msvcrt.lib ${CMAKE_EXE_LINKER_FLAGS}")
	LINK_LIBRARIES(${QT_QTMAIN_LIBRARY})
ENDIF (MSVC)

IF (APPLE AND WANT_BUNDLE)
    SET(GUI_TYPE MACOSX_BUNDLE)
    SET(APPLE_BUNDLE_SOURCES icons/sqliteman.icns)
ENDIF (APPLE AND WANT_BUNDLE)

ADD_EXECUTABLE( ${EXE_NAME} ${GUI_TYPE}
		${SQLITEMAN_SRC}
		${SQLITEMAN_MOC_SRC}
                ${SQLITEMAN_UI_HDRS}
		${SQLITEMAN_RC_RCS}
                ${APPLE_BUNDLE_SOURCES}
)
target_link_libraries(${EXE_NAME}
    ${${QTVERSION}Widgets_LIBRARIES})
IF (WANT_INTERNAL_SQLDRIVER)
ELSE (WANT_INTERNAL_SQLDRIVER)
    target_link_libraries(${EXE_NAME}
        ${${QTVERSION}Sql_LIBRARIES})
ENDIF (WANT_INTERNAL_SQLDRIVER)

IF (WIN32)
    # don't use console subsystem
    IF (MINGW)
        TARGET_LINK_LIBRARIES(${EXE_NAME} "-mwindows")
	ENDIF (MINGW)
ENDIF (WIN32)


IF (WANT_INTERNAL_QSCINTILLA)
    SET (TORA_QSCINTILLA_LIB tora_qscintilla2_lib)
    MESSAGE(STATUS "TARGET_LINK_LIBRARIES(
        '${EXE_NAME} ${TORA_QSCINTILLA_LIB} ${QT_LIBRARIES}')")
    TARGET_LINK_LIBRARIES(${EXE_NAME} ${TORA_QSCINTILLA_LIB} ${QT_LIBRARIES})
ELSE (WANT_INTERNAL_QSCINTILLA)
    MESSAGE(STATUS "TARGET_LINK_LIBRARIES(
        '${EXE_NAME} ${QSCINTILLA_LIBRARIES}
        ${QT_LIBRARIES}')")
    TARGET_LINK_LIBRARIES(${EXE_NAME} ${QSCINTILLA_LIBRARIES} ${QT_LIBRARIES})
ENDIF (WANT_INTERNAL_QSCINTILLA)

IF (SQLITE_FOUND)
    MESSAGE(STATUS "TARGET_LINK_LIBRARIES(
        '${EXE_NAME} ${SQLITE_LIBRARIES}')")
	TARGET_LINK_LIBRARIES(${EXE_NAME} ${SQLITE_LIBRARIES})
ENDIF (SQLITE_FOUND)

IF (WIN32)
    INSTALL(TARGETS ${EXE_NAME} RUNTIME DESTINATION .)
ELSE (WIN32)

    # common linux/unices come first, then macosx stuff
    IF (NOT WANT_BUNDLE)
        INSTALL(TARGETS ${EXE_NAME} RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
    ELSE (NOT WANT_BUNDLE)

      SET(MACOSX_BUNDLE_ICON_FILE sqliteman.icns)
      SET(MACOSX_BUNDLE_INFO_STRING "${EXE_NAME} ${VERSION}")
      SET(MACOSX_BUNDLE_GUI_IDENTIFIER "com.sqliteman")
      SET(MACOSX_BUNDLE_LONG_VERSION_STRING "${VERSION}")
      SET(MACOSX_BUNDLE_BUNDLE_NAME "${EXE_NAME}")
      SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION}")
      SET(MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION}")
      SET(MACOSX_BUNDLE_COPYRIGHT "(c) Petr Vanek and various volunteers")
      SET_SOURCE_FILES_PROPERTIES(${CMAKE_SOURCE_DIR}/sqliteman/icons/sqliteman.icns
                                  PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

      # Warning: This is dangerous to touch. Except you know what are you doing.
      #          I ususaly don't know anything about it. But it's working ;)
      INSTALL(TARGETS ${EXE_NAME} BUNDLE DESTINATION ${CMAKE_INSTALL_PREFIX})

      IF (WANT_BUNDLE_STANDALONE)
        # --- If we are on OS X copy all the embedded libraries to the app bundle
        # -------- Function to build OS X Stand Alone Bundles -----------------
        function(MakeOSXBundleApp target binary_dir)
            #-- Set some useful variables
            SET (OSX_MAKE_STANDALONE_BUNDLE_CMAKE_SCRIPT "${binary_dir}/${target}_OSX_MakeStandAloneBundle.cmake")
            SET (OSX_MAKE_STANDALONE_BUNDLE_BASH_SCRIPT "${binary_dir}/${target}_OSX_MakeStandAloneBundle.sh")
            #-- Configure the cmake file and the shell script
            CONFIGURE_FILE("${CMAKE_SOURCE_DIR}/osx_tools/CompleteBundle.cmake.in"
                           "${OSX_MAKE_STANDALONE_BUNDLE_CMAKE_SCRIPT}" @ONLY IMMEDIATE)
            CONFIGURE_FILE("${CMAKE_SOURCE_DIR}/osx_tools/CreateBundle.sh.in"
                           "${OSX_MAKE_STANDALONE_BUNDLE_BASH_SCRIPT}" @ONLY IMMEDIATE)
            #-- Create the installation code
            install(SCRIPT "${OSX_MAKE_STANDALONE_BUNDLE_CMAKE_SCRIPT}")
        endfunction(MakeOSXBundleApp)


        install(DIRECTORY ${QT_PLUGINS_DIR}/
                DESTINATION ${PROJECT_BINARY_DIR}/Contents/Plugins/
                REGEX "(designer|script|phonon_backend|debug|sqldriver|ico)" EXCLUDE)

        install(CODE "
            # glob for previously installed qt plugins to include in fixup_bundle
            file(GLOB_RECURSE PluginList
                 ${PROJECT_BINARY_DIR}/Contents/Plugins/*.bundle)
        ")

        MakeOSXBundleApp(${EXE_NAME} ${PROJECT_BINARY_DIR})

        # TODO/FIXME: this is evil! And it should be done automagically...
        set(hack_bundle_pth "${CMAKE_INSTALL_PREFIX}/${EXE_NAME}.app/Contents/MacOS")
        INSTALL(CODE "file(MAKE_DIRECTORY ${hack_bundle_pth}/sqldrivers)")
        INSTALL(CODE "file(MAKE_DIRECTORY ${hack_bundle_pth}/codecs)")
        INSTALL(CODE "file(MAKE_DIRECTORY ${hack_bundle_pth}/accessible)")
        INSTALL(CODE "file(MAKE_DIRECTORY ${hack_bundle_pth}/imageformats)")
        INSTALL(CODE "file(MAKE_DIRECTORY ${hack_bundle_pth}/graphicssystems)")
        function(hack_move_plugins filename targetdir)
            install(CODE "
                message(STATUS \"file:: ${filename} -> targetdir: ${targetdir} ${hack_bundle_pth}\")
            ")
            install(FILES ${hack_bundle_pth}/${filename} DESTINATION ${hack_bundle_pth}/${targetdir})
            install(CODE "file(REMOVE ${hack_bundle_pth}/${filename})")
        endfunction(hack_move_plugins)
        hack_move_plugins("libqcncodecs.bundle" "codecs")
        hack_move_plugins("libqjpcodecs.bundle" "codecs")
        hack_move_plugins("libqtwcodecs.bundle" "codecs")
        hack_move_plugins("libqkrcodecs.bundle" "codecs")
        hack_move_plugins("libqgif.bundle" "imageformats")
        hack_move_plugins("libqjpeg.bundle" "imageformats")
        hack_move_plugins("libqmng.bundle" "imageformats")
        hack_move_plugins("libqsvg.bundle" "imageformats")
        hack_move_plugins("libqtiff.bundle" "imageformats")
        hack_move_plugins("libqtaccessiblewidgets.bundle" "accessible")
        hack_move_plugins("libqtaccessiblecompatwidgets.bundle" "accessible")
        hack_move_plugins("libqglgraphicssystem.bundle" "graphicssystems")
        # end of hack

        #-- Put an empty qt.conf file in the bundle so as to NOT load plugins from the installed Qt.
        INSTALL(CODE "message(STATUS \"${CMAKE_INSTALL_PREFIX}/${EXE_NAME}.app/Contents/Resources/qt.conf\")")
        INSTALL(FILES "${CMAKE_SOURCE_DIR}/osx_tools/qt.conf.in"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${EXE_NAME}.app/Contents/Resources/"
                RENAME "qt.conf")

        if ("${QT_MAC_USE_COCOA}" STREQUAL "1")
            INSTALL(CODE "message(STATUS \"Copying qt_menu.nib into ${CMAKE_INSTALL_PREFIX}/${EXE_NAME}.app/Contents/Resources\")")
            INSTALL(CODE "file(MAKE_DIRECTORY \"${CMAKE_INSTALL_PREFIX}/${EXE_NAME}.app/Contents/Resources/qt_menu.nib\")")
            INSTALL(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                             \"${QT_LIBRARY_DIR}/QtGui.framework/Resources/qt_menu.nib\"
                             \"${CMAKE_INSTALL_PREFIX}/${EXE_NAME}.app/Contents/Resources/qt_menu.nib\")")
        endif()
      ENDIF (WANT_BUNDLE_STANDALONE)
    ENDIF (NOT WANT_BUNDLE)

ENDIF (WIN32)

# Added by Richard Parkins to force recalculation of build time.
# Currently only tested with Linux
IF (WIN32)
ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/buildtime.h COMMAND echo static const char * buildtime = "Built %date% %time%" ; > ${CMAKE_CURRENT_SOURCE_DIR}/buildtime.h DEPENDS ${SQLITEMAN_SRC} ${SQLITEMAN_MOC_SRC} ${SQLITEMAN_UI_HDRS} ${SQLITEMAN_RC_RCS} COMMENT "Creating timestamp in buildtime.h")
ELSE (WIN32)
# assume Linux
ADD_CUSTOM_COMMAND(OUTPUT buildtime.h COMMAND echo 'static const char * ' `date '+buildtime = \"Built %c from branch '` `git branch --show-current`'\"\;' > buildtime.h DEPENDS ${SQLITEMAN_SRC} ${SQLITEMAN_MOC_SRC} ${SQLITEMAN_UI_HDRS} ${SQLITEMAN_RC_RCS} COMMENT "Creating timestamp in buildtime.h")
ENDIF (WIN32)
SET_PROPERTY(SOURCE litemanwindow.cpp PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/buildtime.h)
set_property(TARGET sqliteman APPEND PROPERTY
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
      /usr/include/${QTVERSION}/
)
get_target_property(TEMP ${EXE_NAME} LINK_LIBRARIES)
MESSAGE(STATUS "LINK_LIBRARIES = '${TEMP}'")
MESSAGE(STATUS "Leaving Sqliteman/sqliteman/CMakeLists.txt")
