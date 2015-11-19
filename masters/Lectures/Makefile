#
# Build the course materials into their relevant PDFs
#

.SUFFIXES: .tex .pdf
FIGDIR= ../../figures

SLIDES= tracing.pdf processes.pdf schedandlock.pdf		\
	networking.pdf stack.pdf				\
								\
	lecture1-intro-handout.pdf				\
	lecture1-intro-slides.pdf				\
	lecture2-kernel-tracing-handout.pdf			\
	lecture2-kernel-tracing-slides.pdf			\
	lecture3-processes-p1-handout.pdf			\
	lecture3-processes-p1-slides.pdf			\
	lecture4-processes-p2-handout.pdf			\
	lecture4-processes-p2-slides.pdf			\
	lecture5-networking-p1-handout.pdf			\
	lecture5-networking-p1-slides.pdf			\
	lecture6-networking-p2-handout.pdf			\
	lecture6-networking-p2-slides.pdf			\
	lecturelet1-lab-io-handout.pdf				\
	lecturelet1-lab-io-slides.pdf				\
	lecturelet2-lab-ipc-handout.pdf				\
	lecturelet2-lab-ipc-slides.pdf				\
	lecturelet3-lab-pmc-handout.pdf				\
	lecturelet3-lab-pmc-slides.pdf				\
	lecturelet4-lab-tcp-p1-handout.pdf			\
	lecturelet4-lab-tcp-p1-slides.pdf			\
	lecturelet5-lab-tcp-p2-handout.pdf			\
	lecturelet5-lab-tcp-p2-slides.pdf

all: ${SLIDES}

LECTURE1FIGS=	${FIGDIR}/bbb-onna-desk.jpg
LECTURE2FIGS=	${FIGDIR}/dtrace-the-big-picture.pdf
LECTURE3FIGS=	${FIGDIR}/kernel-user-threads.pdf		\
		${FIGDIR}/process-address-space.pdf		\
		${FIGDIR}/process-life-cycle.pdf		\
		${FIGDIR}/process-programmer-model.pdf		\
		${FIGDIR}/saltzer-schroeder-protection.png
LECTURE4FIGS=	${FIGDIR}/process-address-space.pdf		\
		${FIGDIR}/syscall-stacks.pdf			\
		${FIGDIR}/syscall-table-output.pdf		\
		${FIGDIR}/mach-vm-model.pdf
LECTURE5FIGS=	${FIGDIR}/cpu_nginx_sandstorm_6nics_new.pdf	\
		${FIGDIR}/network-cpu-ddio-nic.pdf		\
		${FIGDIR}/network-dispatch-input.pdf		\
		${FIGDIR}/network-dispatch-output.pdf		\
		${FIGDIR}/network-dispatch-toe.pdf		\
		${FIGDIR}/network-forward.pdf			\
		${FIGDIR}/network-in-out.pdf			\
		${FIGDIR}/network-mbufs.pdf			\
		${FIGDIR}/network-memory-flow.pdf		\
		${FIGDIR}/network-netmap-memory-flow.pdf	\
		${FIGDIR}/network-nic-stack.pdf			\
		${FIGDIR}/throughput_nginx_sandstorm_6nics_new.pdf
LECTURE6FIGS=		${FIGDIR}/2010-dispatch-variations.pdf	\
			${FIGDIR}/network-dispatch-input.pdf	\
			${FIGDIR}/network-in-out.pdf		\
			${FIGDIR}/tcp-hash-table.pdf		\
			${FIGDIR}/tcp-hash-table-groups.pdf	\
			${FIGDIR}/tcp-mp-strategies.pdf		\
			${FIGDIR}/tcp-structures.pdf		\
			${FIGDIR}/tcp-syn-flood.pdf		\
			${FIGDIR}/tcp-timeline.pdf		\
			${FIGDIR}/tcp-time-sequence.pdf		\
			${FIGDIR}/vj-congestion-slow-start.pdf
LECTURELET2FIGS=	${FIGDIR}/mach-vm-model.pdf
LECTURELET3FIGS=	${FIGDIR}/processor-pipeline.pdf
LECTURELET4FIGS=	${FIGDIR}/tcp-time-sequence.pdf
LECTURELET5FIGS=	${FIGDIR}/tcp-structures.pdf		\
			${FIGDIR}/tcp-timeline.pdf		\
			${FIGDIR}/tcp-time-sequence.pdf		\
			${FIGDIR}/vj-congestion-slow-start.pdf

lecture1-intro-handout.pdf: ${LECTURE1FIGS}
lecture1-intro-slides.pdf: ${LECTURE1FIGS}
lecture2-kernel-tracing-handout.pdf: ${LECTURE2FIGS}
lecture2-kernel-tracing-slides.pdf: ${LECTURE2FIGS}
lecture3-processes-p1-handout.pdf: ${LECTURE3FIGS}
lecture3-processes-p1-slides.pdf: ${LECTURE3FIGS}
lecture4-processes-p2-handout.pdf: ${LECTURE4FIGS}
lecture4-processes-p2-slides.pdf: ${LECTURE4FIGS}
lecture5-networking-p1-handout.pdf: ${LECTURE5FIGS}
lecture5-networking-p1-slides.pdf: ${LECTURE5FIGS}
lecture6-networking-p2-handout.pdf: ${LECTURE6FIGS}
lecture6-networking-p2-slides.pdf: ${LECTURE6FIGS}
lecturelet2-lab-ipc-handout.pdf: ${LECTURELET2FIGS}
lecturelet2-lab-ipc-slide.pdf: ${LECTURELET2FIGS}
lecturelet3-lab-pmc-handout.pdf: ${LECTURELET3FIGS}
lecturelet3-lab-pmc-slides.pdf: ${LECTURELET3FIGS}
lecturelet4-lab-tcp-p1-handout.pdf: ${LECTURELET4FIGS}
lecturelet4-lab-tcp-p1-slides.pdf: ${LECTURELET4FIGS}
lecturelet5-lab-tcp-p2-handout.pdf: ${LECTURELET5FIGS}
lecturelet5-lab-tcp-p2-slides.pdf: ${LECTURELET5FIGS}

%-handout.pdf : %-handout.tex %.tex
	latexmk -pdf $<

%-slides.pdf : %-slides.tex %.tex
	latexmk -pdf $<

clean:
	rm -f *.aux *.log *.nav *.out *.snm

realclean:
	rm -f *.aux *.log *.nav *.out *.snm ${SLIDES}
