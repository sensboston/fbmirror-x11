CC = gcc
CFLAGS = -O3 -Wall
LIBS = -lX11 -lXext -lXfixes

PREFIX = /usr/local

fbmirror_x11: fbmirror_x11.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

install: fbmirror_x11
	install -m 755 fbmirror_x11 $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/fbmirror_x11

clean:
	rm -f fbmirror_x11

.PHONY: install uninstall clean
