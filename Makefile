# see COPYING for copyright and license details
PREFIX = /usr/local
# to compile a very very small client using dietlibc
#CC = diet -Os gcc -Wall
CC = cc -O2 -Wall
OBJ = cgo.o
BIN = cgo

default: $(OBJ)
	$(CC) -o $(BIN) $(OBJ)

clean:
	rm -f $(OBJ) $(BIN)

install:
	@cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

