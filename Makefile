###########################################
# Service Manager
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2019-02-18
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

#MAKEFLAGS="-j ${C}"
prefix=/usr/local
UNIX_SOCKET_DEFINE:=$(shell cat UNIX_SOCKET)

all: bin/keepalive bin/svcmgr bin/svcmgrd

bin/keepalive: ../common/libcommon.a obj/keepalive.o bin
	g++ -o $@ obj/keepalive.o $(LDFLAGS)

bin/svcmgr: ../common/libcommon.a obj/svcmgr.o bin
	g++ -o $@ obj/svcmgr.o $(LDFLAGS) -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/svcmgrd: ../common/libcommon.a obj/svcmgrd.o bin
	g++ -o $@ obj/svcmgrd.o $(LDFLAGS) -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin:
	if [ ! -d bin ]; then mkdir bin; fi;

../common/libcommon.a: ../common/Makefile
	cd ../common; make;

../common/Makefile: ../common/configure
	cd ../common; ./configure;

../common/configure:
	cd ../; git clone https://github.com/benkietzman/common.git

obj/keepalive.o: keepalive.cpp obj ../common/Makefile
	g++ -g -std=c++14 -Wall -c keepalive.cpp -o $@ $(CPPFLAGS)

obj/svcmgr.o: svcmgr.cpp obj ../common/Makefile
	g++ -g -std=c++14 -Wall -c svcmgr.cpp -o $@ $(UNIX_SOCKET_DEFINE) $(CPPFLAGS) -I../common

obj/svcmgrd.o: svcmgrd.cpp obj ../common/Makefile
	g++ -g -std=c++14 -Wall -c svcmgrd.cpp -o $@ $(UNIX_SOCKET_DEFINE) $(CPPFLAGS) -I../common

obj:
	if [ ! -d obj ]; then mkdir obj; fi;

install: bin/keepalive bin/svcmgr bin/svcmgrd
	-if [ ! -d $(prefix)/svcmgr ]; then mkdir $(prefix)/svcmgr; fi;
	install --mode=775 bin/keepalive $(prefix)/svcmgr/keepalive
	install --mode=775 bin/svcmgr $(prefix)/svcmgr/svcmgr
	install --mode=775 bin/svcmgrd $(prefix)/svcmgr/svcmgrd

clean:
	-rm -fr obj bin

uninstall:
	-rm -fr $(prefix)/svcmgr
