/*-
 * Copyright (c) 2015 Robert N. M. Watson
 * Copyright (c) 2015 Bjoern A. Zeeb
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#ifdef WITH_PMC
#include <pmc.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>


/*
 * L41: Lab 4-5 - TCP tracing
 *
 * Based on the simplistic IPC benchmark used in prior labs, this version is
 * extended to support TCP.
 */

#define	timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

static unsigned int Bflag;	/* bare */
static unsigned int qflag;	/* quiet */
static unsigned int sflag;	/* set socket-buffer sizes */
static unsigned int vflag;	/* verbose */

/*
 * Which mode is the benchmark operating in?
 */
#define	BENCHMARK_MODE_INVALID_STRING	"invalid"
#define	BENCHMARK_MODE_1THREAD_STRING	"1thread"
#define	BENCHMARK_MODE_2THREAD_STRING	"2thread"
#define	BENCHMARK_MODE_2PROC_STRING	"2proc"

#define	BENCHMARK_MODE_INVALID		-1
#define	BENCHMARK_MODE_1THREAD		1
#define	BENCHMARK_MODE_2THREAD		2
#define	BENCHMARK_MODE_2PROC		3

#define	BENCHMARK_MODE_DEFAULT		BENCHMARK_MODE_1THREAD
static unsigned int benchmark_mode = BENCHMARK_MODE_DEFAULT;

#define	BENCHMARK_IPC_INVALID_STRING	"invalid"
#define	BENCHMARK_IPC_PIPE_STRING	"pipe"
#define	BENCHMARK_IPC_LOCAL_SOCKET_STRING	"local"
#define	BENCHMARK_IPC_TCP_SOCKET_STRING		"tcp"

#define	BENCHMARK_IPC_INVALID		-1
#define	BENCHMARK_IPC_PIPE		1
#define	BENCHMARK_IPC_LOCAL_SOCKET		2
#define	BENCHMARK_IPC_TCP_SOCKET		3

#define	BENCHMARK_IPC_DEFAULT		BENCHMARK_IPC_PIPE
static unsigned int ipc_type = BENCHMARK_IPC_DEFAULT;

#define	BENCHMARK_TCP_PORT_DEFAULT	10141
static unsigned short tcp_port = BENCHMARK_TCP_PORT_DEFAULT;

#define	BUFFERSIZE	(128 * 1024UL)
static long buffersize = BUFFERSIZE;	/* I/O buffer size */

#define	TOTALSIZE	(16 * 1024 * 1024UL)
static long totalsize = TOTALSIZE;	/* total I/O size */

#define	max(x, y)	((x) > (y) ? (x) : (y))
#define	min(x, y)	((x) < (y) ? (x) : (y))

#ifdef WITH_PMC
#define	COUNTERSET_MAX_EVENTS	4	/* Maximum hardware registers */

#define	COUNTERSET_TRAILER						\
	"INSTR_EXECUTED",	/* Instructions retired */		\
	"CLOCK_CYCLES"		/* Cycle counter */

#define	COUNTERSET_TRAILER_INSTR_EXECUTED	2	/* Array index */
#define	COUNTERSET_TRAILER_CLOCK_CYCLES		3	/* Array index */

static const char *counterset_l1d[COUNTERSET_MAX_EVENTS] = {
	"L1_DCACHE_ACCESS",	/* Level-1 data-cache hits */
	"L1_DCACHE_REFILL",	/* Level-1 data-cache misses */
	COUNTERSET_TRAILER
};

static const char *counterset_l1i[COUNTERSET_MAX_EVENTS] = {
	NULL,			/* Level-1 instruction-cache hits (not on A8) */
	"L1_ICACHE_REFILL",	/* Level-1 instruction-cache misses */
	COUNTERSET_TRAILER
};

/*
 * XXXRW: For reasons we don't understand, L2_CACHE_MISS does not return the
 * L2 cache miss rate.  Maybe we have misunderstood the documentation.
 * Regardless, we can't report it.
 */
static const char *counterset_l2[COUNTERSET_MAX_EVENTS] = {
	"L2_ACCESS",		/* Level-2 cache hits */
	NULL,			/* Level-2 cache misses (not on A8) */
	COUNTERSET_TRAILER
};

static const char *counterset_mem[COUNTERSET_MAX_EVENTS] = {
	"MEM_READ",		/* Memory reads issued by instructions */
	"MEM_WRITE",		/* Memory writes issued by instructions */
	COUNTERSET_TRAILER
};

static const char *counterset_tlb[COUNTERSET_MAX_EVENTS] = {
	"ITLB_REFILL",		/* Instruction-TLB misses */
	"DTLB_REFILL",		/* Data-TLB misses */
	COUNTERSET_TRAILER
};

static const char *counterset_axi[COUNTERSET_MAX_EVENTS] = {
	"AXI_READ",		/* Memory reads over AXI bus */
	"AXI_WRITE",		/* Memory writes over AXI bus */
	COUNTERSET_TRAILER
};

#define	BENCHMARK_PMC_INVALID_STRING	"invalid"
#define	BENCHMARK_PMC_L1D_STRING	"l1d"
#define	BENCHMARK_PMC_L1I_STRING	"l1i"
#define	BENCHMARK_PMC_L2_STRING		"l2"
#define	BENCHMARK_PMC_MEM_STRING	"mem"
#define	BENCHMARK_PMC_TLB_STRING	"tlb"
#define	BENCHMARK_PMC_AXI_STRING	"axi"

#define	BENCHMARK_PMC_INVALID		-1
#define	BENCHMARK_PMC_NONE		0
#define	BENCHMARK_PMC_L1D		1
#define	BENCHMARK_PMC_L1I		2
#define	BENCHMARK_PMC_L2		3
#define	BENCHMARK_PMC_MEM		4
#define	BENCHMARK_PMC_TLB		5
#define	BENCHMARK_PMC_AXI		6

#define	BENCHMARK_PMC_DEFAULT	BENCHMARK_PMC_NONE
static unsigned int benchmark_pmc = BENCHMARK_PMC_NONE;

static pmc_id_t pmcid[COUNTERSET_MAX_EVENTS];
static uint64_t pmc_values[COUNTERSET_MAX_EVENTS];

static const char **counterset;		/* The actual counter set in use. */

static void
pmc_setup(void)
{
	int i;

	switch (benchmark_pmc) {
	case BENCHMARK_PMC_NONE:
		return;

	case BENCHMARK_PMC_L1D:
		counterset = counterset_l1d;
		break;

	case BENCHMARK_PMC_L1I:
		counterset = counterset_l1i;
		break;

	case BENCHMARK_PMC_L2:
		counterset = counterset_l2;
		break;

	case BENCHMARK_PMC_MEM:
		counterset = counterset_mem;
		break;

	case BENCHMARK_PMC_TLB:
		counterset = counterset_tlb;
		break;

	case BENCHMARK_PMC_AXI:
		counterset = counterset_axi;
		break;

	default:
		assert(0);
	}

	/*
	 * Use process-mode counting that descends to children processes --
	 * i.e., to properly account for child behaviour in 2proc.
	 */
	bzero(pmc_values, sizeof(pmc_values));
	if (pmc_init() < 0)
		err(EX_OSERR, "FAIL: pmc_init");
	for (i = 0; i < COUNTERSET_MAX_EVENTS; i++) {
		if (counterset[i] == NULL)
			continue;
		if (pmc_allocate(counterset[i], PMC_MODE_TC,
		    PMC_F_DESCENDANTS, PMC_CPU_ANY, &pmcid[i]) < 0)
			err(EX_OSERR, "FAIL: pmc_allocate %s", counterset[i]);
		if (pmc_attach(pmcid[i], 0) != 0)
			err(EX_OSERR, "FAIL: pmc_attach %s", counterset[i]);
		if (pmc_write(pmcid[i], 0) < 0)
			err(EX_OSERR, "FAIL: pmc_write  %s", counterset[i]);
	}
}

static void
pmc_teardown(void)
{
	int i;

	for (i = 0; i < COUNTERSET_MAX_EVENTS; i++) {
		if (counterset[i] == NULL)
			continue;
		if (pmc_detach(pmcid[i], 0) != 0)
			err(EX_OSERR, "FAIL: pmc_detach %s", counterset[i]);
		if (pmc_release(pmcid[i]) < 0)
			err(EX_OSERR, "FAIL: pmc_release %s", counterset[i]);
	}
}

static __inline void
pmc_begin(void)
{
	int i;

	for (i = 0; i < COUNTERSET_MAX_EVENTS; i++) {
		if (counterset[i] == NULL)
			continue;
		if (pmc_start(pmcid[i]) < 0)
			err(EX_OSERR, "FAIL: pmc_start %s", counterset[i]);
	}
}

static __inline void
pmc_end(void)
{
	int i;

	for (i = 0; i < COUNTERSET_MAX_EVENTS; i++) {
		if (counterset[i] == NULL)
			continue;
		if (pmc_read(pmcid[i], &pmc_values[i]) < 0)
			err(EX_OSERR, "FAIL: pmc_read %s", counterset[i]);
	}
	for (i = 0; i < COUNTERSET_MAX_EVENTS; i++) {
		if (counterset[i] == NULL)
			continue;
		if (pmc_stop(pmcid[i]) < 0)
			err(EX_OSERR, "FAIL: pmc_stop %s", counterset[i]);
	}
}

static int
benchmark_pmc_from_string(const char *string)
{

	if (strcmp(BENCHMARK_PMC_L1D_STRING, string) == 0)
		return (BENCHMARK_PMC_L1D);
	else if (strcmp(BENCHMARK_PMC_L1I_STRING, string) == 0)
		return (BENCHMARK_PMC_L1I);
	else if (strcmp(BENCHMARK_PMC_L2_STRING, string) == 0)
		return (BENCHMARK_PMC_L2);
	else if (strcmp(BENCHMARK_PMC_MEM_STRING, string) == 0)
		return (BENCHMARK_PMC_MEM);
	else if (strcmp(BENCHMARK_PMC_TLB_STRING, string) == 0)
		return (BENCHMARK_PMC_TLB);
	else if (strcmp(BENCHMARK_PMC_AXI_STRING, string) == 0)
		return (BENCHMARK_PMC_AXI);
	else
		return (BENCHMARK_PMC_INVALID);
}

static const char *
benchmark_pmc_to_string(int type)
{

	switch (type) {
	case BENCHMARK_PMC_L1D:
		return (BENCHMARK_PMC_L1D_STRING);

	case BENCHMARK_PMC_L1I:
		return (BENCHMARK_PMC_L1I_STRING);

	case BENCHMARK_PMC_L2:
		return (BENCHMARK_PMC_L2_STRING);

	case BENCHMARK_PMC_MEM:
		return (BENCHMARK_PMC_MEM_STRING);

	case BENCHMARK_PMC_TLB:
		return (BENCHMARK_PMC_TLB_STRING);

	case BENCHMARK_PMC_AXI:
		return (BENCHMARK_PMC_AXI_STRING);

	default:
		return (BENCHMARK_PMC_INVALID_STRING);
	}
}
#endif

static int
ipc_type_from_string(const char *string)
{

	if (strcmp(BENCHMARK_IPC_PIPE_STRING, string) == 0)
		return (BENCHMARK_IPC_PIPE);
	else if (strcmp(BENCHMARK_IPC_LOCAL_SOCKET_STRING, string) == 0)
		return (BENCHMARK_IPC_LOCAL_SOCKET);
	else if (strcmp(BENCHMARK_IPC_TCP_SOCKET_STRING, string) == 0)
		return (BENCHMARK_IPC_TCP_SOCKET);
	else
		return (BENCHMARK_IPC_INVALID);
}

static const char *
ipc_type_to_string(int type)
{

	switch (type) {
	case BENCHMARK_IPC_PIPE:
		return (BENCHMARK_IPC_PIPE_STRING);

	case BENCHMARK_IPC_LOCAL_SOCKET:
		return (BENCHMARK_IPC_LOCAL_SOCKET_STRING);

	case BENCHMARK_IPC_TCP_SOCKET:
		return (BENCHMARK_IPC_TCP_SOCKET_STRING);

	default:
		return (BENCHMARK_IPC_INVALID_STRING);
	}
}

static int
benchmark_mode_from_string(const char *string)
{

	if (strcmp(BENCHMARK_MODE_1THREAD_STRING, string) == 0)
		return (BENCHMARK_MODE_1THREAD);
	else if (strcmp(BENCHMARK_MODE_2THREAD_STRING, string) == 0)
		return (BENCHMARK_MODE_2THREAD);
	else if (strcmp(BENCHMARK_MODE_2PROC_STRING, string) == 0)
		return (BENCHMARK_MODE_2PROC);
	else
		return (BENCHMARK_MODE_INVALID);
}

static const char *
benchmark_mode_to_string(int mode)
{

	switch (mode) {
	case BENCHMARK_MODE_1THREAD:
		return (BENCHMARK_MODE_1THREAD_STRING);

	case BENCHMARK_MODE_2THREAD:
		return (BENCHMARK_MODE_2THREAD_STRING);

	case BENCHMARK_MODE_2PROC:
		return (BENCHMARK_MODE_2PROC_STRING);

	default:
		return (BENCHMARK_MODE_INVALID_STRING);
	}
}

/*
 * Print usage message and exit.
 */
static void
usage(void)
{

	fprintf(stderr,
	    "%s [-Bqsv] [-b buffersize] [-i pipe|local|tcp] [-p tcp_port]\n\t"
#ifdef WITH_PMC
	    "[-P l1d|l1i|l2|mem|tlb|axi] "
#endif
	    "[-t totalsize] mode\n", PROGNAME);
	fprintf(stderr,
  "\n"
  "Modes (pick one - default %s):\n"
  "    1thread                IPC within a single thread\n"
  "    2thread                IPC between two threads in one process\n"
  "    2proc                  IPC between two threads in two different processes\n"
  "\n"
  "Optional flags:\n"
  "    -B                     Run in bare mode: no preparatory activities\n"
  "    -i pipe|local|tcp      Select pipe, local sockets, or TCP (default: %s)\n"
  "    -p tcp_port            Set TCP port number (default: %u)\n"
#ifdef WITH_PMC
  "    -P l1d|l1i|l2|mem|tlb|axi  Enable hardware performance counters\n"
#endif
  "    -q                     Just run the benchmark, don't print stuff out\n"
  "    -s                     Set send/receive socket-buffer sizes to buffersize\n"
  "    -v                     Provide a verbose benchmark description\n"
  "    -b buffersize          Specify a buffer size (default: %ld)\n"
  "    -t totalsize           Specify total I/O size (default: %ld)\n",
	    benchmark_mode_to_string(BENCHMARK_MODE_DEFAULT),
	    ipc_type_to_string(BENCHMARK_IPC_DEFAULT),
	    BENCHMARK_TCP_PORT_DEFAULT,
	    BUFFERSIZE, TOTALSIZE);
	exit(EX_USAGE);
}

/*
 * The IPC benchmark itself.
 * XXX
 *
 * The I/O benchmark itself.  Perform any necessary setup.  Open the file or
 * device.  Take a timestamp.  Perform the work.  Take another timestamp.
 * (Optionally) print the results.
 */

/*
 * Whether using threads or processes, we have the second thread/process do
 * the sending, and the first do the receiving, so that it reliably knows when
 * the benchmark is done.  The sender will write the timestamp to shared
 * memory before sending any bytes, so the receipt of any byte means that the
 * timestamp has been stored.
 *
 * XXXRW: If we run this benchmark on multicore, we may want to put in a
 * memory barrier of some sort on either side, although because a system call
 * is involved, it is probably not necessary.  I wonder what C and POSIX have
 * to say about that.
 */
struct sender_argument {
	struct timespec	 sa_starttime;	/* Sender stores start time here. */
	int		 sa_writefd;	/* Caller provides send fd here. */
	long		 sa_blockcount;	/* Caller provides block count here. */
	void		*sa_buffer;	/* Caller provides buffer here. */
};

static void
sender(struct sender_argument *sap)
{
	ssize_t len;
	long write_sofar;

	if (clock_gettime(CLOCK_REALTIME, &sap->sa_starttime) < 0)
		err(EX_OSERR, "FAIL: clock_gettime");
#ifdef WITH_PMC
	if (benchmark_pmc != BENCHMARK_PMC_NONE)
		pmc_begin();
#endif

	/*
	 * HERE BEGINS THE BENCHMARK (2-thread/2-proc).
	 */
	write_sofar = 0;
	while (write_sofar < totalsize) {
		const size_t bytes_to_write = min(buffersize, totalsize - write_sofar);
		len = write(sap->sa_writefd, sap->sa_buffer,
		    min(buffersize, totalsize - write_sofar));
		/*printf("write(%d, %zd, %zd) = %zd\n", sap->sa_writefd, 0, bytes_to_write, len);*/
		if (len != bytes_to_write) {
			errx(EX_IOERR, "blocking write() returned early: %zd != %zd", len, bytes_to_write);
		}
		if (len < 0)
			err(EX_IOERR, "FAIL: write");
		write_sofar += len;
	}
}

static struct timespec
receiver(int readfd, long blockcount, void *buf)
{
	struct timespec finishtime;
	ssize_t len;
	long read_sofar;

	read_sofar = 0;
	/** read() always returns as soon as there is something to read,
	 * i.e. one pipe/socket buffer size. Make sure we use the whole buffer */
	while (read_sofar < totalsize) {
		const size_t offset = read_sofar % buffersize;
		const size_t bytes_to_read = min(totalsize - read_sofar, buffersize - offset);
		len = read(readfd, buf + offset, bytes_to_read);
		/*printf("read(%d, %zd, %zd) = %zd\n", readfd, offset, bytes_to_read, len);*/
		/* if (len != bytes_to_read) {
			warn("blocking read returned early: %zd != %zd", len, bytes_to_read);
		} */
		if (len < 0)
			err(EX_IOERR, "FAIL: read");
		read_sofar += len;
	}

	/*
	 * HERE ENDS THE BENCHMARK (2-thread/2-proc).
	 */
#ifdef WITH_PMC
	if (benchmark_pmc != BENCHMARK_PMC_NONE)
		pmc_end();
#endif
	if (clock_gettime(CLOCK_REALTIME, &finishtime) < 0)
		err(EX_OSERR, "FAIL: clock_gettime");
	return (finishtime);
}

static void *
second_thread(void *arg)
{
	struct sender_argument *sap = arg;

	if (!Bflag)
		sleep(1);
	sender(sap);

	/*
	 * No action needed to terminate thread other than to return.
	 */
	return (NULL);
}

static struct sender_argument sa;

static struct timespec
do_2thread(int readfd, int writefd, long blockcount, void *readbuf,
    void *writebuf)
{
	struct timespec finishtime;
	pthread_t thread;

	/*
	 * We can just use ordinary shared memory between the two threads --
	 * no need to do anything special.
	 */
	sa.sa_writefd = writefd;
	sa.sa_blockcount = blockcount;
	sa.sa_buffer = writebuf;
	if (pthread_create(&thread, NULL, second_thread, &sa) < 0)
		err(EX_OSERR, "FAIL: pthread_create");
	finishtime = receiver(readfd, blockcount, readbuf);
	if (pthread_join(thread, NULL) < 0)
		err(EX_OSERR, "FAIL: pthread_join");
	timespecsub(&finishtime, &sa.sa_starttime);
	return (finishtime);
}

static struct timespec
do_2proc(int readfd, int writefd, long blockcount, void *readbuf,
    void *writebuf)
{
	struct sender_argument *sap;
	struct timespec finishtime;
	pid_t pid, pid2;

	/*
	 * Set up a shared page across fork() that will allow not just
	 * passing arguments, but also getting back the starting timestamp
 	 * that may be somewhat after the time of fork() in this process.
	 */
	if ((sap = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_ANON,
	    -1, 0)) == MAP_FAILED)
		err(EX_OSERR, "mmap");
	if (minherit(sap, getpagesize(), INHERIT_SHARE) < 0)
		err(EX_OSERR, "minherit");
	sap->sa_writefd = writefd;
	sap->sa_blockcount = blockcount;
	sap->sa_buffer = writebuf;
	pid = fork();
	if (pid == 0) {
		if (!Bflag)
			sleep(1);
		sender(sap);
		if (!Bflag)
			sleep(1);
		_exit(0);
	}
	finishtime = receiver(readfd, blockcount, readbuf);
	if ((pid2 = waitpid(pid, NULL, 0)) < 0)
		err(EX_OSERR, "FAIL: waitpid");
	if (pid2 != pid)
		err(EX_OSERR, "FAIL: waitpid PID mismatch");
	timespecsub(&finishtime, &sap->sa_starttime);
	return (finishtime);
}

/*
 * The single threading case is quite different from the two-thread/process
 * case, as we need to manually interleave the sender and recipient.  Note the
 * opportunity for deadlock if the buffer size is greater than what the IPC
 * primitive provides.
 *
 * The buffer pointer must point at a buffer of size 'buffersize' here.
 *
 * XXXRW: Should we be explicitly setting the buffer size for the file
 * descriptor?  Pipes appear not to offer a way to do this.
 */
static struct timespec
do_1thread(int readfd, int writefd, long blockcount, void *readbuf,
    void *writebuf)
{
	struct timespec starttime, finishtime;
	fd_set fdset_read, fdset_write;
	long read_sofar, write_sofar;
	ssize_t len_read, len_write;
	int flags;

	flags = fcntl(readfd, F_GETFL, 0);
	if (flags < 0)
		err(EX_OSERR, "FAIL: fcntl(readfd, F_GETFL, 0)");
	if (fcntl(readfd, F_SETFL, flags | O_NONBLOCK) < 0)
		err(EX_OSERR, "FAIL: fcntl(readfd, F_SETFL, "
		    "flags | O_NONBLOCK)");
	flags = fcntl(writefd, F_GETFL, 0);
	if (flags < 0)
		err(EX_OSERR, "FAIL: fcntl(writefd, F_GETFL, 0)");
	if (fcntl(writefd, F_SETFL, flags | O_NONBLOCK) < 0)
		err(EX_OSERR, "FAIL: fcntl(writefd, F_SETFL, "
		    "flags | O_NONBLOCK)");

	FD_ZERO(&fdset_read);
	FD_SET(readfd, &fdset_read);
	FD_ZERO(&fdset_write);
	FD_SET(writefd, &fdset_write);

	if (clock_gettime(CLOCK_REALTIME, &starttime) < 0)
		err(EX_OSERR, "FAIL: clock_gettime");
#ifdef WITH_PMC
	if (benchmark_pmc != BENCHMARK_PMC_NONE)
		pmc_begin();
#endif

	/*
	 * HERE BEGINS THE BENCHMARK (1-thread).
	 */
	read_sofar = write_sofar = 0;
	/** As the I/O is nonblocking write()/read() will return after only
	 * reading part of the buffer. For this benchmark we ensure that
	 * the whole buffer is used instead of always using offset 0 to
	 * have the same behaviour as the 2thread/2proc version */
	while (read_sofar < totalsize) {
		const size_t remaining_write = totalsize - write_sofar;
		if (remaining_write > 0) {
			const size_t offset = write_sofar % buffersize;
			const size_t bytes_to_write = min(remaining_write, buffersize - offset);
			len_write = write(writefd, writebuf + offset, bytes_to_write);
			/*printf("write(%d, %zd, %zd) = %zd\n", writefd, offset, bytes_to_write, len_write);*/
			if (len_write < 0 && errno != EAGAIN)
				err(EX_IOERR, "FAIL: write");
			if (len_write > 0)
				write_sofar += len_write;
		}
		if (write_sofar != 0) {
			const size_t offset = read_sofar % buffersize;
			const size_t bytes_to_read = min(totalsize - read_sofar, buffersize - offset);
			len_read = read(readfd, readbuf + offset, bytes_to_read);
			/*printf("read(%d, %zd, %zd) = %zd\n", readfd, offset, bytes_to_read, len_read);*/
			if (len_read < 0 && errno != EAGAIN)
				err(EX_IOERR, "FAIL: read");
			if (len_read > 0)
				read_sofar += len_read;
		}

		/*
		 * If we've had neither read nor write progress in this
		 * iteration, block until one of reading or writing is
		 * possible.
		 */
		if (read_sofar < totalsize &&
		    (len_read == 0 && len_write == 0)) {
			if (select(max(readfd, writefd), &fdset_read,
			    &fdset_write, NULL, NULL) < 0)
				err(EX_IOERR, "FAIL: select");
		}
	}

	/*
	 * HERE ENDS THE BENCHMARK (1-thread).
	 */
#ifdef WITH_PMC
	if (benchmark_pmc != BENCHMARK_PMC_NONE)
		pmc_end();
#endif
	if (clock_gettime(CLOCK_REALTIME, &finishtime) < 0)
		err(EX_OSERR, "FAIL: clock_gettime");
	timespecsub(&finishtime, &starttime);
	return (finishtime);
}

static void
ipc(void)
{
	struct sockaddr_in sin;
	struct timespec ts;
	long blockcount;
	void *readbuf, *writebuf;
	int error, fd[2], flags, i, listenfd, readfd, writefd, sockoptval;
	double secs, rate;
#ifdef WITH_PMC
	uint64_t clock_cycles, instr_executed, counter0, counter1;
#endif

	if (totalsize % buffersize != 0)
		errx(EX_USAGE, "FAIL: data size (%ld) is not a multiple of "
		    "buffersize (%ld)", totalsize, buffersize);
	blockcount = totalsize / buffersize;
	if (blockcount < 0)
		errx(EX_USAGE, "FAIL: negative block count");

	/*
	 * Allocate zero-filled memory for our I/O buffer.
	 *
	 * XXXRW: Use mmap() rather than calloc()?  Touch each page?
	 */
	readbuf = calloc(buffersize, 1);
	if (readbuf == NULL)
		err(EX_OSERR, "FAIL: calloc");
	writebuf = calloc(buffersize, 2);
	if (writebuf == NULL)
		err(EX_OSERR, "FAIL: calloc");

#ifdef WITH_PMC
	/*
	 * Allocate and initialise performance counters, if required.
	 */
	if (benchmark_pmc != BENCHMARK_PMC_NONE)
		pmc_setup();
#endif

	/*
	 * Allocate a suitable IPC object.
	 */
	switch (ipc_type) {
	case BENCHMARK_IPC_PIPE:
		if (pipe(fd) < 0)
			err(EX_OSERR, "FAIL: pipe");

		/*
		 * On FreeBSD, it doesn't matter which end of the pipe we use,
		 * but on other operating systems, it is sometimes the case
		 * that the first file descriptor must be used for reading,
		 * and the second for writing.
		 */
		readfd = fd[0];
		writefd = fd[1];
		break;

	case BENCHMARK_IPC_LOCAL_SOCKET:
		if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd) < 0)
			err(EX_OSERR, "FAIL: socketpair");

		/*
		 * With socket pairs, it makes no difference which one we use
		 * for reading or writing.
		 */
		readfd = fd[0];
		writefd = fd[1];
		break;

	case BENCHMARK_IPC_TCP_SOCKET:
		listenfd = socket(PF_INET, SOCK_STREAM, 0);
		if (listenfd < 0)
			err(EX_OSERR, "FAIL: socket (listen)");

		/*
		 * Socket address used for both binding and connecting.
		 */
		i = 1;
		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i,
		    sizeof(i)) < 0)
			err(EX_OSERR, "FAIL: setsockopt SO_REUSEADDR");
		bzero(&sin, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sin.sin_port = htons(tcp_port);
		if (bind(listenfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
			err(EX_OSERR, "FAIL: bind");
		if (listen(listenfd, -1) < 0)
			err(EX_OSERR, "FAIL: listen");

		/*
		 * Create the 'read' endpoint and connect it to the listen
		 * socket in a non-blocking manner (as we need to accept()
		 * it before the connection can complete, and don't want to
		 * deadlock).
		 *
		 * Note that it *does* matter which we use for reading vs.
		 * writing -- we intentionally wait until the full three-way
		 * handshake is done before transmitting data, and only the
		 * accepted socket has this property (i.e., waiting for the
		 * ACK in the SYN-SYN/ACK-ACK exchange).
		 */
		readfd = socket(PF_INET, SOCK_STREAM, 0);
		if (readfd < 0)
			err(EX_OSERR, "FAIL: socket (read)");
		flags = fcntl(readfd, F_GETFL, 0);
		if (flags < 0)
			err(EX_OSERR, "FAIL: fcntl(readfd, F_GETFL, 0)");
		if (fcntl(readfd, F_SETFL, flags | O_NONBLOCK) < 0)
			err(EX_OSERR, "FAIL: fcntl(readfd, F_SETFL, "
			    "flags | O_NONBLOCK)");
		error = connect(readfd, (struct sockaddr *)&sin,
		    sizeof(sin));
		if (error < 0 && errno != EINPROGRESS)
			err(EX_OSERR, "FAIL: connect");

		/*
		 * On the listen socket, now accept the 'write' endpoint --
		 * which should block until the full three-way handshake is
		 * complete.
		 */
		writefd = accept(listenfd, NULL, NULL);
		if (writefd < 0)
			err(EX_OSERR, "accept");

		/*
		 * Restore blocking status to the 'read' endpoint, and close
		 * the now-unnecessary listen socket.  Any further use of
		 * the 'read' endpoint will block until the socket is ready,
		 * although in practice that is unlikely.
		 */
		if (fcntl(readfd, F_SETFL, flags) < 0)
			err(EX_OSERR, "FAIL: fcntl(readfd, F_SETFL, flags");
		close(listenfd);
		break;

	default:
		assert(0);
	}


	if (ipc_type == BENCHMARK_IPC_LOCAL_SOCKET ||
	    ipc_type == BENCHMARK_IPC_TCP_SOCKET) {
		if (sflag) {
			/*
			 * Default socket-buffer sizes may be too low (e.g.,
			 * 8K) to allow atomic sends/receives of our requested
			 * buffer length.  Extend both socket buffers to fit
			 * better.
			 */
			sockoptval = buffersize;
			if (setsockopt(writefd, SOL_SOCKET, SO_SNDBUF,
			    &sockoptval, sizeof(sockoptval)) < 0)
				err(EX_OSERR, "FAIL: setsockopt SO_SNDBUF");
			if (setsockopt(readfd, SOL_SOCKET, SO_RCVBUF,
			    &sockoptval, sizeof(sockoptval)) < 0)
				err(EX_OSERR, "FAIL: setsockopt SO_RCVBUF");
		}
	}

	/*
	 * Before we start, sync() the filesystem so that it is fairly
	 * quiesced from prior work.  Give things a second to settle down.
	 */
	if (!Bflag) {
		/* Flush terminal output. */
		fflush(stdout);
		fflush(stderr);

		/* Flush filesystems as a whole. */
		(void)sync();
		(void)sync();
		(void)sync();

		/*
		 * Let things settle.
		 *
		 * NB: This will have the side effect of aliasing execution
		 * to the timer.  Unclear if this is a good thing.
		 */
		(void)sleep(1);
	}

	/*
	 * Perform the actual benchmark; timing is done within different
	 * versions as they behave quite differently.  Each returns the total
	 * execution time from just before first byte sent to just after last
	 * byte received.
	 */
	switch (benchmark_mode) {
	case BENCHMARK_MODE_1THREAD:
		ts = do_1thread(readfd, writefd, blockcount, readbuf,
		    writebuf);
		break;

	case BENCHMARK_MODE_2THREAD:
		ts = do_2thread(readfd, writefd, blockcount, readbuf,
		    writebuf);
		break;

	case BENCHMARK_MODE_2PROC:
		ts = do_2proc(readfd, writefd, blockcount, readbuf, writebuf);
		break;

	default:
		assert(0);
	}

	/*
	 * Now we can disruptively print things -- if we're not in quiet mode.
	 */
	if (!qflag) {
		if (vflag) {
			printf("Benchmark configuration:\n");
			printf("  buffersize: %ld\n", buffersize);
			printf("  totalsize: %ld\n", totalsize);
			printf("  blockcount: %ld\n", blockcount);
			printf("  mode: %s\n",
			    benchmark_mode_to_string(benchmark_mode));
			printf("  ipctype: %s\n",
			    ipc_type_to_string(ipc_type));
			printf("  time: %jd.%09jd\n", (intmax_t)ts.tv_sec,
			    (intmax_t)ts.tv_nsec);
		}

#ifdef WITH_PMC
		if (benchmark_pmc != BENCHMARK_PMC_NONE) {
			clock_cycles =
			    pmc_values[COUNTERSET_TRAILER_CLOCK_CYCLES];
			instr_executed =
			    pmc_values[COUNTERSET_TRAILER_INSTR_EXECUTED];

			printf("\n");
			printf("  pmctype: %s\n",
			    benchmark_pmc_to_string(benchmark_pmc));
			printf("  INSTR_EXECUTED: %ju\n",
			    (uintmax_t)instr_executed);
			printf("  CLOCK_CYCLES: %ju\n", clock_cycles);
			printf("  CLOCK_CYCLES/INSTR_EXECUTED: %F\n",
			    (float)clock_cycles/instr_executed);

			/* Mode-specific counters. */
			if (counterset[0] != NULL) {
				counter0 = pmc_values[0];
				printf("  %s: %ju\n", counterset[0],
				    counter0);
				printf("  %s/INSTR_EXECUTED: %F\n",
				    counterset[0],
				    (float)counter0/instr_executed);
				printf("  %s/CLOCK_CYCLES: %F\n",
				    counterset[0],
				    (float)counter0/clock_cycles);
			}
			if (counterset[1] != NULL) {
				counter1 = pmc_values[1];
				printf("  %s: %ju\n", counterset[1],
				    counter1);
				printf("  %s/INSTR_EXECUTED: %F\n",
				    counterset[1],
				    (float)counter1/instr_executed);
				printf("  %s/CLOCK_CYCLES: %F\n",
				    counterset[1],
				    (float)counter1/clock_cycles);
			}
			printf("\n");
		}
#endif

		/* Seconds with fractional component. */
		secs = (float)ts.tv_sec + (float)ts.tv_nsec / 1000000000;

		/* Bytes/second. */
		rate = totalsize / secs;

		/* Kilobytes/second. */
		rate /= (1024);

		printf("%.2F KBytes/sec\n", rate);
	}
	close(readfd);
	close(writefd);
#ifdef WITH_PMC
	if (benchmark_pmc != BENCHMARK_PMC_NONE)
		pmc_teardown();
#endif
}

/*
 * main(): parse arguments, invoke benchmark function.
 */
int
main(int argc, char *argv[])
{
	char *endp;
	long l;
	int ch;

	buffersize = BUFFERSIZE;
	totalsize = TOTALSIZE;
	while ((ch = getopt(argc, argv, "Bb:i:p:P:qst:v"
#ifdef WITH_PMC
	"P:"
#endif
	    )) != -1) {
		switch (ch) {
		case 'B':
			Bflag++;
			break;

		case 'b':
			buffersize = strtol(optarg, &endp, 10);
			if (*optarg == '\0' || *endp != '\0' || buffersize <= 0)
				usage();
			break;

		case 'i':
			ipc_type = ipc_type_from_string(optarg);
			if (ipc_type == BENCHMARK_IPC_INVALID)
				usage();
			break;

		case 'p':
			l = strtol(optarg, &endp, 10);
			if (*optarg == '\0' || *endp != '\0' ||
			    l <= 0 || l > 65535)
				usage();
			tcp_port = l;
			break;

#ifdef WITH_PMC
		case 'P':
			benchmark_pmc = benchmark_pmc_from_string(optarg);
			if (benchmark_pmc == BENCHMARK_PMC_INVALID)
				usage();
			break;
#endif

		case 'q':
			qflag++;
			break;

		case 's':
			sflag++;
			break;

		case 't':
			totalsize = strtol(optarg, &endp, 10);
			if (*optarg == '\0' || *endp != '\0' || totalsize <= 0)
				usage();
			break;

		case 'v':
			vflag++;
			break;

		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * A little argument-specific validation.
	 */
	if (sflag && (ipc_type != BENCHMARK_IPC_LOCAL_SOCKET) &&
	    (ipc_type != BENCHMARK_IPC_TCP_SOCKET))
		usage();

	/*
	 * Exactly one of our operational modes, which will be specified as
	 * the next (and only) mandatory argument.
	 */
	if (argc != 1)
		usage();
	benchmark_mode = benchmark_mode_from_string(argv[0]);
	if (benchmark_mode == BENCHMARK_MODE_INVALID)
		usage();
	ipc();
	exit(0);
}
