#Michael Elliott, Kirash Teymoury, Liv Vitale
.PHONY: all clean

all:
	$(MAKE) all -f CMakefile
	$(MAKE) all -f LaTeXMakefile

clean:
	$(MAKE) clean -f CMakefile
	$(MAKE) clean -f LaTeXMakefile
