# This init file defines a gdb function which can print some QT structures
define pd
  if $argc == 0
# show struct Vdbe
    list sqlite3.c:20312,20383
  else
    if $argc == 1
# normal dump argument
      call pd::dump($arg0)
    else
      if $argc == 2
        if $arg1 == "ops"
# special case to dump oplist from a struct Vdbe *
          call pd::dumpOpList((void *)$arg0)
        else
# print a field from a struct Vdbe *
          print ((struct Vdbe *)$arg0)->$arg1
        end
      else
        print "Do not know how to pd with >2 args"
      end
    end
  end
end
set unwindonsignal on
set check type off
# source directory for Qt
dir /usr/src/debug/libqt5-qtbase-5.15.8+kde185-150500.4.8.1.x86_64/src/corelib
dir /usr/src/debug/libqt5-qtbase-5.15.8+kde185-150500.4.8.1.x86_64/src/widgets
dir /usr/src/debug/libqt5-qtbase-5.15.8+kde185-150500.4.8.1.x86_64/src/gui
dir /usr/src/debug/libqt5-qtbase-5.15.8+kde185-150500.4.8.1.x86_64/src/sql
