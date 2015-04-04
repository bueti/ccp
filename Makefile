CC := gcc
TARGETDIR := bin
SRCDIR := src

CFLAGS := -g -Wall -Wpedantic -Wextra -O2 -std=gnu99 -pthread
INC := -I ./include -L ./lib

.PHONY all: server client

server:
	@echo " Building Server..."
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c $(SRCDIR)/semaphre.c $(SRCDIR)/shmem.c -o $(TARGETDIR)/server"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server;

client:
	@echo " Building Client..."
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client;

.PHONY clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(TARGETDIR)/*"; $(RM) -r $(TARGETDIR)/*
