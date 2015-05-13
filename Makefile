CC := gcc
TARGETDIR := bin
SRCDIR := src

CFLAGS := -g -Wall -Wpedantic -Wextra -O0 -std=gnu99 -pthread -D_GNU_SOURCE
INC := -I ./include -L ./lib
LIB := -L ./lib -lrt -lpthread

.PHONY all: server client

server:
	@echo " Building Server..."
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server $(LIB);

client:
	@echo " Building Client..."
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client $(LIB);

.PHONY clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(TARGETDIR)/*"; $(RM) -r $(TARGETDIR)/*
