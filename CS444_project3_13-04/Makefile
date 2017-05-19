#Michael Elliott, Kirash Teymoury, Liv Vitale
CC		= gcc
CCFLAGS		= -pthread\
		  -ansi -Wall -Wextra
TARGET		= synchro3
HEADERS		= Queue.h
SRCS		= $(TARGET).c Queue.c

.PHONY: clean all debug

all: $(TARGET)

$(TARGET):
	$(CC) $(CCFLAGS) $(SRCS) -o $@

debug:
	$(CC) -g $(CCFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
