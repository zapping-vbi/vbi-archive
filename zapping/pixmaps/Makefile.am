PIXMAPS = \
	gnome-digital-camera.png \
	gnome-grecord.png \
	gnome-info.png \
	gnome-keyboard.png \
	gnome-media-player.png \
	gnome-monitor.png \
	gnome-oscilloscope.png \
	gnome-session.png \
	gnome-shutdown.png \
	atvef_icon.png \
	eacem_icon.png \
	up.png \
	down.png \
	left.png \
	right.png \
	reset.png \
	reveal.png

RATING_CA = \
	rating_tv_ca_en_1.png rating_tv_ca_en_2.png \
	rating_tv_ca_en_3.png rating_tv_ca_en_4.png \
	rating_tv_ca_en_5.png rating_tv_ca_en_6.png

RATING_US = \
	rating_tv_us_10.png rating_tv_us_20.png \
	rating_tv_us_21.png rating_tv_us_30.png \
	rating_tv_us_40.png rating_tv_us_41.png \
	rating_tv_us_42.png rating_tv_us_43.png \
	rating_tv_us_44.png rating_tv_us_45.png \
	rating_tv_us_46.png rating_tv_us_47.png \
	rating_tv_us_48.png rating_tv_us_49.png \
	rating_tv_us_4a.png rating_tv_us_4b.png \
	rating_tv_us_4c.png rating_tv_us_4d.png \
	rating_tv_us_4e.png rating_tv_us_4f.png \
	rating_tv_us_50.png rating_tv_us_51.png \
	rating_tv_us_52.png rating_tv_us_53.png \
	rating_tv_us_54.png rating_tv_us_55.png \
	rating_tv_us_56.png rating_tv_us_57.png \
	rating_tv_us_58.png rating_tv_us_59.png \
	rating_tv_us_5a.png rating_tv_us_5b.png \
	rating_tv_us_5c.png rating_tv_us_5d.png \
	rating_tv_us_5e.png rating_tv_us_5f.png \
	rating_tv_us_60.png rating_tv_us_61.png \
	rating_tv_us_62.png rating_tv_us_63.png \
	rating_tv_us_64.png rating_tv_us_65.png \
	rating_tv_us_66.png rating_tv_us_67.png

IMAGES = \
	logo.png \
	vt_loading1.jpeg \
	vt_loading2.jpeg \
	gnome-television.png

ZAPPING_STOCK = \
	brightness.png contrast.png saturation.png hue.png \
	recordtb.png \
	mute.png \
	teletext.png \
	screenshot.png

pixmapdir = $(datadir)/pixmaps/zapping
pixmap_DATA = gnome-television.png $(PIXMAPS) $(RATING_CA) $(RATING_US) $(IMAGES)

EXTRA_DIST = $(pixmap_DATA) $(ZAPPING_STOCK)

BUILT_SOURCES = zapping_stock.h $(PIXMAPS:.png=.h)

noinst_HEADERS = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES) $(ZAPPING_STOCK:.png=.h)

%.h: @GDK_PIXBUF_CSOURCE@

%.h: %.png
	echo -e "\n/* Generated file, do not edit! */\n" > $@
	@GDK_PIXBUF_CSOURCE@ --struct --build-list \
	  `echo $(<F) | sed -e 's%[^[:alnum:]]%_%g'` $< >> $@

zapping_stock.h: $(ZAPPING_STOCK:.png=.h)
	echo -e "\n/* Generated file, do not edit! */\n" > $@
	for fi in $^; do echo "#include \"$$fi\"" >> $@; done
	echo -e "\nstatic const struct {\n const gchar *stock_id;" >> $@
	echo -e " const GdkPixdata *pixdata;\n} zapping_stock_table [] = {" >> $@
	for fi in $(ZAPPING_STOCK); do \
	  echo -n $$fi | sed -e 's%[^[:alnum:]]%-%g;s%\(.*\).png%  { "zapping-\1", \&%' >> $@; \
	  echo -n $$fi | sed -e 's%[^[:alnum:]]%_%g' >> $@; echo " }," >> $@; \
	done
	echo -e "  { 0, 0 }\n};\n" >> $@
