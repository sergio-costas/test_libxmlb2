PKGS="glib-2.0 gobject-2.0 gio-2.0 xmlb gnome-software appstream"
CFLAGS=`pkg-config --cflags ${PKGS}`
LIBS=`pkg-config --libs  ${PKGS}`

OPTS=-c -O2 -g

OBJECTS=gsplugin.o gsplugin2.o gsplugin3.o test_xmlb.o

all: ${OBJECTS}
	gcc -o test_xmlb ${OBJECTS} ${LIBS}

%.o: %.c
	gcc ${OPTS} -o $@ $< ${CFLAGS}

clean:
	rm -f test_xmlb *.o
