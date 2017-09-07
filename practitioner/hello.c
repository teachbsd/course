#include <stdio.h>
#include <unistd.h>
#include <sys/sdt.h> /* <- new header file */

int main (int argc, char **argv) {
	int i;
	for (i = 0; i < 5; i++) {
		DTRACE_PROBE1(world, loop, i); /* <- probe point */
		printf("Hello world\n");
		sleep(1);
	}
}  
