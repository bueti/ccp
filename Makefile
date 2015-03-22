CC := gcc
TARGETDIR := bin
SRCDIR := src

CFLAGS := -g -Wall -Wpedantic -Wextra -O2 -std=gnu99
INC := -I ./include -L ./lib

all:
	@echo " Building..."
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server;
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client;

server:
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server;

client:
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client;

clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(TARGETDIR)/*"; $(RM) -r $(TARGETDIR)/*
