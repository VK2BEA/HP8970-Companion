
desktopdir = $(prefix)/share/applications
desktop_DATA = applications/hp8970.desktop

pixmapsdir = $(prefix)/share/pixmaps
pixmaps_DATA = application-icon/hicolor/scalable/apps/hp8970.svg

fontdir = $(prefix)/share/fonts/hp8970/
font_DATA = fonts/NotoSans_Condensed-Regular.ttf fonts/NotoSans-Italic.ttf fonts/NotoSans-Regular.ttf

hp8970dir = $(prefix)/share/icons/hicolor/256x256/apps/
hp8970_DATA = application-icon/hicolor/256x256/apps/hp8970.png

hp8970_2dir = $(prefix)/share/icons/hicolor/512x512/apps/
hp8970_2_DATA = application-icon/hicolor/512x512/apps/hp8970.png

hp8970_3dir = $(prefix)/share/icons/hicolor/48x48/apps/
hp8970_3_DATA = application-icon/hicolor/48x48/apps/hp8970.png

hp8970_4dir = $(prefix)/share/icons/hicolor/scalable/apps/
hp8970_4_DATA = application-icon/hicolor/scalable/apps/hp8970.svg

gsettingsdir = $(prefix)/share/glib-2.0/schemas/
gsettings_DATA = gsettings/us.heterodyne.hp8970.gschema.xml


install-data-hook:
	gtk-update-icon-cache --ignore-theme-index $(prefix)/share/icons/hicolor/
	glib-compile-schemas $(prefix)/share/glib-2.0/schemas
	fc-cache i$(prefix)/share/fonts/hp8970
