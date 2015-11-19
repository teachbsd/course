all: ipc-static ipc-dynamic

CFLAGS=-DWITH_PMC -Wall

ipc-static: ipc.c
	cc ${CFLAGS} -o ${.TARGET} -DPROGNAME=\"${.TARGET}\" ipc.c -static \
	    -lpmc -lpthread

ipc-dynamic: ipc.c
	cc ${CFLAGS} -o ${.TARGET} -DPROGNAME=\"${.TARGET}\" ipc.c -dynamic \
	    -lpmc -lpthread
