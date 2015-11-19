all: io-static io-dynamic

io-static: io.c
	cc -Wall -o ${.TARGET} -DPROGNAME=\"${.TARGET}\" io.c -static
io-dynamic: io.c
	cc -Wall -o ${.TARGET} -DPROGNAME=\"${.TARGET}\" io.c -dynamic
