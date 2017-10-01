PDFS=		dtrace.pdf						\
		labreport.pdf						\
		labreport-template.pdf					\
		lab-1-io.pdf						\
		lab-2-ipc.pdf						\
		lab-3-pmc.pdf						\
		lab-4-tcp-state.pdf					\
		lab-5-tcp-latency.pdf					\
		labsetup.pdf						\
		lectures.pdf						\
		readings.pdf						\
		syllabus.pdf

all: ${PDFS}

%.pdf : %.tex
	latexmk -pdf $<

clean:
	latexmk -C
