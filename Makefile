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

install:
	@mkdir -p $(DESTDIR)$(PREFIX)/bin/
	@install $(BIN) $(DESTDIR)$(PREFIX)/bin/${BIN}

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

