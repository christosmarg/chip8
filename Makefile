# See LICENSE file for copyright and license details.
# chip8 - a minimal CHIP-8 emulator
.POSIX:

include config.mk

BIN = chip8
DIST = ${BIN}-${VERSION}
MAN1 = ${BIN}.1

SRC = chip8.c
OBJ = chip8.o

all: options ${BIN}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

${BIN}: ${OBJ}
	${CC} ${LDFLAGS} ${OBJ} -o $@

${OBJ}: ${SRC}
	${CC} ${CFLAGS} -c ${SRC} -o $@

dist: clean
	${MKDIR} ${DIST}
	${CP} -R roms ${SRC} config.mk LICENSE Makefile README.md ${DIST}
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
	chmod 755 ${DESTDIR}${BIN_DIR}/${BIN}
	chmod 644 ${DESTDIR}${MAN_DIR}/${MAN1}

uninstall:
	${RM} ${DESTDIR}${BIN_DIR}/${BIN}
	${RM} ${DESTDIR}${MAN_DIR}/${MAN1}

clean:
	${RM} ${BIN} ${OBJ} ${DIST}.tar.gz

.PHONY: all options clean dist install uninstall run
