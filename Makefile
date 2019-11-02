CC	= clang++
CFLAGS += -Wall -std=c++2a -I/usr/include/freetype2
LDFLAGS += -lxcb -lxcb-xrm -lX11 -lX11-xcb -lXft -lfreetype -lz -lfontconfig -lpthread
CFDEBUG = -g3 -pedantic -Wall -Wunused-parameter -Wlong-long \
          -Wsign-conversion -Wconversion -Wimplicit-function-declaration \
	  -Weverything -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic

EXEC = limebar
SRCS = limebar.cpp x.cpp modules/windows.cpp modules/workspaces.cpp modules/clock.cpp color.cpp window.cpp
OBJS = ${SRCS:.cpp=.o}

PREFIX ?= /usr
BINDIR  = ${PREFIX}/bin

all: ${EXEC}

doc: README.pod
	pod2man --section=1 --center="limebar Manual" --name "limebar" README.pod > limebar.1

.cpp.o:
	${CC} ${CFLAGS} -o $@ -c $<

${EXEC}: ${OBJS}
	${CC} -o ${EXEC} ${OBJS} ${LDFLAGS}

debug: ${EXEC}
debug: CC += ${CFDEBUG}

clean:
	rm -f ./*.o ./modules/*.o ./*.1
	rm -f ./${EXEC}

install: limebar doc
	install -D -m 755 limebar ${DESTDIR}${BINDIR}/limebar
	install -D -m 644 limebar.1 ${DESTDIR}${PREFIX}/share/man/man1/limebar.1

uninstall:
	rm -f ${DESTDIR}${BINDIR}/limebar
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/limebar.1

.PHONY: all debug clean install
