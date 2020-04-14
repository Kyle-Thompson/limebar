proj_dir := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
lib_dir  := ${proj_dir}lib/

CC        = clang++
STDLIB    = -stdlib=libc++
LIBS      = $(foreach d, $(shell ls $(lib_dir)),-isystem ${lib_dir}$(d)/include)
CFLAGS    = -std=c++20 -fno-rtti -I/usr/include/freetype2
LDFLAGS   = -lxcb -lxcb-xrm -lxcb-ewmh -lX11 -lX11-xcb -lXft -lfreetype -lfontconfig
CFDEBUG   = -Wall -g
CFWARN    = -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
CFWARN   += -Wno-padded -Wno-c++20-compat
CFWARN   += -Wno-exit-time-destructors -Wno-ctad-maybe-unsupported
CFREL     = -O2
# enable if you have lto
# CFREL    += -flto

EXEC = limebar
SRCS = $(shell find . -path ./lib -prune -o -name "*.cpp" -print)
OBJS = ${SRCS:.cpp=.o}

PREFIX ?= /usr
BINDIR  = ${PREFIX}/bin

all: ${EXEC}

.cpp.o:
	${CC} ${LIBS} ${STDLIB} ${CFLAGS} -o $@ -c $<

${EXEC}: ${OBJS}
	${CC} ${STDLIB} -o ${EXEC} ${OBJS} ${LDFLAGS}

debug: ${EXEC}
debug: CFLAGS += ${CFDEBUG}

warnings: ${EXEC}
warnings: CFLAGS += ${CFDEBUG} ${CFWARN}

release: ${EXEC}
release: CFLAGS += ${CFREL}
# enable if you have lto
# release: LDFLAGS += -flto -fuse-ld=gold

test_addr: ${EXEC}
test_addr: CFLAGS += ${CFDEBUG} -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls
test_addr: LDFLAGS += -fsanitize=address

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
