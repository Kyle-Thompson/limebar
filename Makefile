CC        = clang++
CFLAGS   += -std=c++2a -I/usr/include/freetype2
LDFLAGS  += -lxcb -lxcb-xrm -lX11 -lX11-xcb -lXft -lfreetype -lfontconfig -lpthread
CFDEBUG   = -Wall -g3
WARNINGS  = -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
RELEASE   = -O3 -flto

EXEC = limebar
SRCS = limebar.cpp x.cpp modules/windows.cpp modules/workspaces.cpp modules/clock.cpp color.cpp window.cpp
OBJS = ${SRCS:.cpp=.o}

PREFIX ?= /usr
BINDIR  = ${PREFIX}/bin

all: ${EXEC}

.cpp.o:
	${CC} ${CFLAGS} -o $@ -c $<

${EXEC}: ${OBJS}
	${CC} -o ${EXEC} ${OBJS} ${LDFLAGS}

debug: ${EXEC}
debug: CC += ${CFDEBUG}

warnings: ${EXEC}
warnings: CC += ${CFDEBUG} ${WARNINGS}

release: ${EXEC}
release: CC += ${RELEASE}
release: LDFLAGS += -flto

clean:
	rm -f ./*.o ./modules/*.o ./*.1
	rm -f ./${EXEC}

install: limebar
	install -D -m 755 limebar ${DESTDIR}${BINDIR}/limebar

uninstall:
	rm -f ${DESTDIR}${BINDIR}/limebar

.PHONY: all debug warnings release clean install
