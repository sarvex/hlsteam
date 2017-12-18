LBITS := $(shell getconf LONG_BIT)

ifndef ARCH
	ARCH = $(LBITS)
endif

CFLAGS = -Wall -O3 -I src -std=c11 -I native/include -fPIC -I ../sdk/public/steam
LFLAGS = -lhl -lsteam_api -lstdc++ -L native/lib/linux$(ARCH) -L ../sdk/redistributable_bin/linux$(ARCH)

SRC = native/cloud.o native/common.o native/controller.o native/friends.o native/gameserver.o \
	native/matchmaking.o native/networking.o native/stats.o native/ugc.o

all: ${SRC}
	${CC} ${CFLAGS} -shared -o steam.hdll ${SRC} ${LFLAGS}

install:
	cp steam.hdll /usr/lib
	cp native/lib/linux$(ARCH)/libsteam_api.* /usr/lib
	
.SUFFIXES : .cpp .o

.cpp.o :
	${CC} ${CFLAGS} -o $@ -c $<
	
clean_o:
	rm -f ${SRC}

clean: clean_o
	rm -f steam.hdll

