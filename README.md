# linkup source clone from sourceforge

https://sourceforge.net/projects/linkup/files/?source=navbar

README for LINKUp 2017 07 12 and later versions

The ZIP archive provided is a Win32 archive.  Unzip to a convenient place and run the
LINKUp.exe file.

The size of the archive may be reduced by deleting the LINKUp\lgpl3 folder after unzipping.  Those are the required source files under the LGPL license, but are NOT
required for proper operation.

LINUX users may use the git repository to download the source and compile in a Qt 5.5+ environment. (It may also work with the Debian 5.3.2 libs that are installed by the package installer.  

Qt5 libs (and their associated "-dev" packages) necessary include:
core
dbus
serialport
network
concurrent
gui
opengl
printsupport (optional)
widgets
base

Plus whatever the package manager requires as dependencies of course.

I recommend using qt5-default to make the version 5 libs the default ones to use when compiling.
