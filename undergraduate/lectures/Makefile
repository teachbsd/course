#
# Build the course materials into their relevant PDFs
#

.SUFFIXES: .tex .pdf
FIGDIR= ../../figures

SLIDES= lecture1-intro-slides.pdf \
	lecture2-processes-slides.pdf \
	lecture3-communication-slides.pdf \
	lecture4-storage-slides.pdf \
	lecture5-whole-system-slides.pdf \


all: ${SLIDES}

LECTURE1FIGS=	${FIGDIR}/bbb-onna-desk.jpg

lecture1-intro-handout.pdf: ${LECTURE1FIGS}
lecture1-intro-slides.pdf: ${LECTURE1FIGS}

%-handout.pdf : %-handout.tex %.tex
	latexmk -pdf $<

%-slides.pdf : %-slides.tex %.tex
	latexmk -pdf $<

clean:
	rm -f *.aux *.log *.nav *.out *.snm

realclean:
	rm -f *.aux *.log *.nav *.out *.snm ${SLIDES}
