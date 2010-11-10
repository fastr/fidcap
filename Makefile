COPTS = -Wall -Werror
CC += $(COPTS)

all:
	. ./overo.env
	mkdir -p ./bin ./obj
	${CC} fidcap-open.c -o bin/fidcap-open
	${CC} -c fsr-set-registers.c -o obj/fsr-set-registers.o
	${CC} -c fidcap-interface.c -o obj/fidcap-interface.o
	${CC} -ggdb3 -o bin/fidcap-main fidcap-main.c obj/fsr-set-registers.o obj/fidcap-interface.o
	${CXX} -o bin/fidcap-configure fidcap-configure.cc
	${CXX} -o bin/fidcap fidcap.cc
