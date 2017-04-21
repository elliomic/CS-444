SHELL 		= /bin/sh
CC			= gcc
CCFLAGS		= -pthread\
			  -ansi -Wall -Wextra
TARGET		= synchro1
HEADERS		= mt19937ar.h Queue.h
SRCS		= mt19937ar.c Queue.c $(TARGET).c

.PHONY: clean all debug

all: $(TARGET)

$(TARGET):
	$(CC) $(CCFLAGS) $(SRCS) -o $@

debug:
	$(CC) -g $(CCFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
