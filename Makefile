CC        = clang++
CFLAGS   += -std=c++2a -I/usr/include/freetype2 -Wfatal-errors
LDFLAGS  += -lxcb -lxcb-xrm -lX11 -lX11-xcb -lXft -lfreetype -lfontconfig -lpthread
CFDEBUG   = -Wall -g3
CFWARN    = -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
CFREL     = -O3 -flto

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
debug: CFLAGS += ${CFDEBUG}

warnings: ${EXEC}
warnings: CFLAGS += ${CFDEBUG} ${CFWARN}

release: ${EXEC}
release: CFLAGS += ${CFREL}
release: LDFLAGS += -flto

test_addr: ${EXEC}
test_addr: CFLAGS += ${CFDEBUG} -O1 -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls
test_addr: LDFLAGS += -fsanitize=address

test_mem: ${EXEC}
test_mem: CFLAGS += ${CFDEBUG} -O1 -fsanitize=memory -fno-omit-frame-pointer -fno-optimize-sibling-calls
test_mem: LDFLAGS += -fsanitize=memory

test_thread: ${EXEC}
test_thread: CFLAGS += ${CFDEBUG} -fsanitize=thread
test_thread: LDFLAGS += -fsanitize=thread

test_ub: ${EXEC}
test_ub: CFLAGS += ${CFDEBUG} -fsanitize=undefined
test_ub: LDFLAGS += -fsanitize=undefined

clean:
	rm -f ./*.o ./modules/*.o ./*.1
	rm -f ./${EXEC}

install: limebar
	install -D -m 755 limebar ${DESTDIR}${BINDIR}/limebar

uninstall:
	rm -f ${DESTDIR}${BINDIR}/limebar

.PHONY: all debug warnings release clean install
