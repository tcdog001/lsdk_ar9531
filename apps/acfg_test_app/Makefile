CC=$(TOOLPREFIX)gcc

ACFG_BINARY ?= acfg_tool

SRCS=$(shell ls *.c)
OBJS=$(patsubst %.c,%.o,$(SRCS))
PWD=$(shell pwd)

CFLAGS := -I$(PWD)
CFLAGS += -I$(PWD)/../acfg/include
CFLAGS += -I$(PWD)/../../include
CFLAGS += -I$(PWD) -I$(PWD)/../acfg/src -Wall -Werror
CFLAGS += -D_GNU_SOURCE
CFLAGS += -L $(PWD)/../acfg/src 

all:
	@echo -e "\nBuilding ACFG TESTAPP..."
	$(CC) $(SRCS) $(CFLAGS) -o $(ACFG_BINARY) -l$(LIB_OUT)

clean:
	@echo -e "\nCleaning ACFG TESTAPP..."
	rm -f $(ACFG_BINARY)
	rm -f $(OBJS)

