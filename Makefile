## Part of the solution for Assignment 1 (CS 464/564)
## by Stefan Bruda.

## Uncomment this line (and comment out the following line) to produce
## an executable that provides some debug output.
#CXXFLAGS = -g -Wall -pedantic -DDEBUG
CXXFLAGS = -g -Wall -pedantic

all: sshell

tcp-utils.o: tcp-utils.h tcp-utils.cc
	$(CXX) $(CXXFLAGS) -c -o tcp-utils.o tcp-utils.cc

tokenize.o: tokenize.h tokenize.cc
	$(CXX) $(CXXFLAGS) -c -o tokenize.o tokenize.cc

sshell.o: tcp-utils.h tokenize.h sshell.cc
	$(CXX) $(CXXFLAGS) -c -o sshell.o sshell.cc

sshell: sshell.o tcp-utils.o tokenize.o
	$(CXX) $(CXXFLAGS) -o rsshell sshell.o tcp-utils.o tokenize.o

clean:
	rm -f sshell *~ *.o *.bak core \#*
