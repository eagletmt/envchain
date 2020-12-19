UNAME = $(shell uname)
ifeq ($(origin CC), default)
	CC = gcc
endif
CFLAGS += -Wall -Wextra -ansi -pedantic -std=c99
CXXFLAGS += -Wall -Wextra
ifeq ($(OS), Windows_NT)
	LIBS = -static -lstdc++ -lcredui
	OBJS = envchain.o envchain_windows.o
	RM = cmd.exe /C del
else ifeq ($(UNAME), Darwin)
	CFLAGS += -mmacosx-version-min=10.7
	LIBS = -ledit -ltermcap -framework Security -framework CoreFoundation
	OBJS = envchain.o envchain_osx.o envchain_ask_value_readline.o
else
	CFLAGS += `pkg-config --cflags libsecret-1`
	LIBS = -lreadline `pkg-config --libs libsecret-1`
	OBJS = envchain.o envchain_linux.o envchain_ask_value_readline.o
endif

DESTDIR ?= /usr

all: envchain
envchain: $(OBJS)
	$(CC) $(LDFLAGS) -o envchain $(OBJS) $(LIBS)

%.o: %.c envchain.h
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

%.o: %.cc envchain.h
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	$(RM) envchain envchain.exe $(OBJS)

install: all
	install -d $(DESTDIR)/./bin
	install -m755 ./envchain $(DESTDIR)/./bin/envchain
