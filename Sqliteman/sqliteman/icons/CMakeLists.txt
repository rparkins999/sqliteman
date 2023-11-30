INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}
${CMAKE_SOURCE_DIR}/sqliteman
)

IF (WANT_RESOURCES)
    MESSAGE(STATUS "Icons are not installed as you are using Qt resources for it")
ELSE (WANT_RESOURCES)

INSTALL(FILES
blob.png
close.png
column.png
database_commit.png
database.png
database_rollback.png
delete_multiple.png
delete_table_row.png
delete_multiple.png
delete.png
document-export.png
document-new.png
document-open.png
document-save-as.png
document-save.png
duplicate_table_row.png
edit.png
explain.png
extensions.png
go-next.png
go-next-use.png
go-previous.png
index.png
insert_table_row.png
kate.png
key.png
move-down.png
move-up.png
preferences-desktop-display.png
queryplan.png
runscript.png
runsql.png
setnull.png
snapshot.png
sqliteman.png
system.png
system-search.png
table.png
trigger.png
view.png
  DESTINATION ${ICON_DIR}
)

ENDIF (WANT_RESOURCES)
