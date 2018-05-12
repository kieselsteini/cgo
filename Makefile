# see cgo.c for copyright and license details
PREFIX = /usr/local
CC = cc
CFLAGS ?= -O2 -Wall
OBJ = cgo.o
BIN = cgo

default: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJ)

clean:
	rm -f $(OBJ) $(BIN)

install: default
	@mkdir -p $(DESTDIR)$(PREFIX)/bin/
	@install $(BIN) $(DESTDIR)$(PREFIX)/bin/${BIN}
	@mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	@cp cgo.1 $(DESTDIR)$(MANPREFIX)/man1/cgo.1
	@chmod 644 $(DESTDIR)$(MANPREFIX)/man1/cgo.1

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
