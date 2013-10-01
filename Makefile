###############################################################################
#
# File:         Makefile
# RCS:          $Id: Makefile,v 1.2 2005/10/14 03:52:59 sbirrer Exp $
# Description:  Guess
# Author:       Fabian E. Bustamante
#               Northwestern Systems Research Group
#               Department of Computer Science
#               Northwestern University
# Created:      Fri Sep 12, 2003 at 15:56:30
# Modified:     Wed Sep 24, 2003 at 18:31:43 fabianb@cs.northwestern.edu
# Language:     Makefile
# Package:      N/A
# Status:       Experimental (Do Not Distribute)
#
# (C) Copyright 2003, Northwestern University, all rights reserved.
#
###############################################################################

# handin info
TEAM = `whoami`
VERSION = `date +%Y%m%d%H%M%S`
PROJ = tsh

CC = gcc
MV = mv
CP = cp
RM = rm
MKDIR = mkdir
TAR = tar cvf
COMPRESS = gzip
CFLAGS = -g -Wall -O2 -D HAVE_CONFIG_H

DELIVERY = Makefile *.h *.c test_type
PROGS = tsh
SRCS = interpreter.c io.c runtime.c tsh.c 
OBJS = ${SRCS:.c=.o}

all: ${PROGS}

test-reg-pipe: handin-pipe
	HANDIN=`pwd`/${TEAM}-${VERSION}-${PROJ}.tar.gz;\
	cd testsuite;\
	bash ./run_testcase_pipe.sh $${HANDIN};

test-reg-redir: handin-redir
	HANDIN=`pwd`/${TEAM}-${VERSION}-${PROJ}.tar.gz;\
	cd testsuite;\
	bash ./run_testcase_redir.sh $${HANDIN};


handin-both: cleanAll
	echo test-reg-both > test_type
	${TAR} ${TEAM}-${VERSION}-${PROJ}.tar ${DELIVERY}
	${COMPRESS} ${TEAM}-${VERSION}-${PROJ}.tar

handin-redir: cleanAll
	echo test-reg-redir > test_type
	${TAR} ${TEAM}-${VERSION}-${PROJ}.tar ${DELIVERY}
	${COMPRESS} ${TEAM}-${VERSION}-${PROJ}.tar

handin-pipe: cleanAll
	echo test-reg-pipe > test_type
	${TAR} ${TEAM}-${VERSION}-${PROJ}.tar ${DELIVERY}
	${COMPRESS} ${TEAM}-${VERSION}-${PROJ}.tar
.o:
	${CC} *.c

tsh: ${OBJS}
	${CC} -o $@ ${OBJS}

clean:
	${RM} -f *.o *~

cleanAll: clean
	${RM} -f ${PROGS} ${TEAM}-${VERSION}-${PROJ}.tar.gz
