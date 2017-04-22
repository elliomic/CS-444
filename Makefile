#Michael Elliot, Kirash Teymory, Liv Vitale
.PHONY: all clean

all:
	$(MAKE) all -f CMakefile
	$(MAKE) all -f LaTeXMakefile

clean:
	$(MAKE) clean -f CMakefile
	$(MAKE) clean -f LaTeXMakefile
