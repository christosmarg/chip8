BIN = chip8
VERSION = 0.1
DIST = ${BIN}-${VERSION}
MAN1 = ${BIN}.1
PREFIX = /usr/local
MAN_DIR = ${PREFIX}/man/man1
BIN_DIR = ${PREFIX}bin

EXT = c
SRC = ${wildcard *.${EXT}}
OBJ = ${SRC:%.${EXT}=%.o}

CC = gcc
CPPFLAGS += -Iinclude -DVERSION=\"${VERSION}\"
CFLAGS += -Wall -std=c99 -pedantic -O3
LDFLAGS += -Llib
LDLIBS += -lSDL2

CP = cp -f
RM = rm -f
RM_DIR = rm -rf
MKDIR = mkdir -p
TAR = tar -cf
GZIP = gzip

all: ${BIN}

${BIN}: ${OBJ}
	${CC} ${LDFLAGS} $^ ${LDLIBS} -o $@

%.o: %.${EXT}
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

dist: clean
	${MKDIR} ${DIST}
	${CP} -R roms ${SRC} LICENSE Makefile README.md ${DIST}
	${TAR} ${DIST}.tar ${DIST}
	${GZIP} ${DIST}.tar
	${RM_DIR} ${DIST}

run:
	./${BIN}

install: all
	${MKDIR} ${DESTDIR}${BIN_DIR} ${DESTDIR}${MAN_DIR}
	${CP} ${BIN} ${BIN_DIR}
	${CP} ${MAN1} ${DESTDIR}${MAN_DIR}
	sed "s/VERSION/${VERSION}/g" < ${MAN1} > ${DESTDIR}${MAN_DIR}/${MAN1}
	chmod 644 ${DESTDIR}${BIN_DIR}/${BIN}
	chmod 644 ${DESTDIR}${MAN_DIR}/${MAN1}

uninstall: all
	${RM} ${DESTDIR}${BIN_DIR}/${BIN}
	${RM} ${DESTDIR}${MAN_DIR}/${MAN1}

clean:
	${RM} ${OBJ} ${BIN}

.PHONY: all clean dist install uninstall run
