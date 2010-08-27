all:
	. ./overo.env
	mkdir -p ./bin ./obj
	${CC} fidcap-open.c -o bin/fidcap-open
	${CC} -c configure_registers.c -o obj/configure_registers.o
	${CXX} fidcap-configure.cc -o bin/fidcap-configure
	${CXX} fidcap.cc -o bin/fidcap
