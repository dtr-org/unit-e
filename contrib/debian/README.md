
Debian
====================
This directory contains files used to package united/unite-qt
for Debian-based Linux systems. If you compile united/unite-qt yourself, there are some useful files here.

## unite: URI support ##


unite-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install unite-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your unite-qt binary to `/usr/bin`
and the `../../share/pixmaps/unite128.png` to `/usr/share/pixmaps`

unite-qt.protocol (KDE)

