CC := gcc
TARGETDIR := bin
SRCDIR := src

CFLAGS := -Wall -Wpedantic -Wextra -O3 -std=gnu99 -pthread -D_GNU_SOURCE
CFLAGS_DEBUG := -g -Wall -Wpedantic -Wextra -std=gnu99 -pthread -D_GNU_SOURCE
INC := -I ./include -L ./lib
LIB := -L ./lib -lrt -lpthread

MKDIR_P = mkdir -p

.PHONY all: server client
.PHONY debug: d_server d_client

server:
	@echo " Building Server..."
	@${MKDIR_P} ${TARGETDIR}
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server $(LIB);

d_server:
	@echo " Building Server..."
	@${MKDIR_P} ${TARGETDIR}
	@echo " $(CC) $(CFLAGS_DEBUG) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server"; $(CC) -g $(CFLAGS_DEBUG) $(INC) $(SRCDIR)/server.c $(SRCDIR)/server_optparse.c -o $(TARGETDIR)/server $(LIB);

client:
	@${MKDIR_P} ${TARGETDIR}
	@echo " Building Client..."
	@echo " $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client"; $(CC) $(CFLAGS) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client $(LIB);

d_client:
	@${MKDIR_P} ${TARGETDIR}
	@echo " Building Client..."
	@echo " $(CC) $(CFLAGS_DEBUG) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client"; $(CC) $(CFLAGS_DEBUG) $(INC) $(SRCDIR)/client.c $(SRCDIR)/client_optparse.c -o $(TARGETDIR)/client $(LIB);

.PHONY clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(TARGETDIR)/*"; $(RM) -r $(TARGETDIR)/*
