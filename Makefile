proj_dir := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CC        = clang++ -isystem ${proj_dir}lib/range-v3/include
LIB       = -stdlib=libc++
CFLAGS   += -std=c++20 -I/usr/include/freetype2
LDFLAGS  += -lxcb -lxcb-xrm -lX11 -lX11-xcb -lXft -lfreetype -lfontconfig -lpthread
CFDEBUG   = -Wall -g3
CFWARN    = -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
CFREL     = -O3 -flto

EXEC = limebar
SRCS = limebar.cpp x.cpp modules/windows.cpp modules/workspaces.cpp modules/clock.cpp color.cpp window.cpp pixmap.cpp
OBJS = ${SRCS:.cpp=.o}

PREFIX ?= /usr
BINDIR  = ${PREFIX}/bin

all: ${EXEC}

.cpp.o:
	${CC} ${LIB} ${CFLAGS} -o $@ -c $<

${EXEC}: ${OBJS}
	${CC} ${LIB} -o ${EXEC} ${OBJS} ${LDFLAGS}

debug: ${EXEC}
debug: CFLAGS += ${CFDEBUG}

warnings: ${EXEC}
warnings: CFLAGS += ${CFDEBUG} ${CFWARN}

release: ${EXEC}
release: CFLAGS += ${CFREL}
release: LDFLAGS += -flto

test_addr: ${EXEC}
test_addr: CFLAGS += ${CFDEBUG} -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls
test_addr: LDFLAGS += -fsanitize=address

test_thread: ${EXEC}
test_thread: CFLAGS += ${CFDEBUG} -fsanitize=thread
test_thread: LDFLAGS += -fsanitize=thread

test_ub: ${EXEC}
test_ub: CFLAGS += ${CFDEBUG} -fsanitize=undefined
test_ub: LDFLAGS += -fsanitize=undefined

clean:
	rm -f ./*.o ./modules/*.o ./*.1
	rm -f ./${EXEC}

install:
	install -D -m 755 limebar ${DESTDIR}${BINDIR}/limebar

uninstall:
	rm -f ${DESTDIR}${BINDIR}/limebar

.PHONY: all debug warnings release clean install
