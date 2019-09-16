# This snippet has been shmelessly stol^Hborrowed from thestinger's repose Makefile
VERSION = 1.1
GIT_DESC=$(shell test -d .git && git describe --always 2>/dev/null)

ifneq "$(GIT_DESC)" ""
	VERSION=$(GIT_DESC)
endif

CC	= clang++
CFLAGS += -Wall -std=c++2a -DVERSION="\"$(VERSION)\"" -I/usr/include/freetype2
LDFLAGS += -lxcb -lxcb-randr -lxcb-xrm -lX11 -lX11-xcb -lXft -lfreetype -lz -lfontconfig
CFDEBUG = -g3 -pedantic -Wall -Wunused-parameter -Wlong-long \
          -Wsign-conversion -Wconversion -Wimplicit-function-declaration \
	  -Weverything -Wextra

EXEC = limebar
SRCS = limebar.cpp
OBJS = ${SRCS:.cpp=.o}

PREFIX?=/usr
BINDIR=${PREFIX}/bin

all: ${EXEC}

doc: README.pod
	pod2man --section=1 --center="limebar Manual" --name "limebar" --release="limebar $(VERSION)" README.pod > limebar.1

.cpp.o:
	${CC} ${CFLAGS} -o $@ -c $<

${EXEC}: ${OBJS}
	${CC} -o ${EXEC} ${OBJS} ${LDFLAGS}

debug: ${EXEC}
debug: CC += ${CFDEBUG}

clean:
	rm -f ./*.o ./*.1
	rm -f ./${EXEC}

install: limebar doc
	install -D -m 755 limebar ${DESTDIR}${BINDIR}/limebar
	install -D -m 644 limebar.1 ${DESTDIR}${PREFIX}/share/man/man1/limebar.1

uninstall:
	rm -f ${DESTDIR}${BINDIR}/limebar
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/limebar.1

.PHONY: all debug clean install
