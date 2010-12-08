COPTS = -Wall -Werror -I./include
CC += $(COPTS)

all:
	. ./overo.env
	mkdir -p ./bin ./obj
	${CC} -c src/fidcap-interface.c -o obj/fidcap-interface.o
	${CC} -o bin/fidcap src/fidcap-main.c obj/fidcap-interface.o
	${CC} -o bin/loop-10ms src/loop-100ms.c

archive:
	${CC} archive/fidcap-open.c -o bin/fidcap-open
	${CXX} -o bin/fidcap-configure archive/fidcap-configure.cc
	${CXX} -o bin/fidcap archive/fidcap.cc

clean:
	rm bin/* obj/*
