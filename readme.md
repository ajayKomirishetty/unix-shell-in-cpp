
Contents:

o  source code, included in sshell.cc
o  modules tokenize and tcp-utils as provided previously (note: the module tcp-utils is included because of the function `readline')
o  a makefile (the default target builds the executable, there are no other targets of interest)
o  a sample configuration file shconfig( which is internally used by more command and connect command)
o  a small file used for testing of command more (testmore)

Change the makefile and compile with -DDEBUG option for an executable
that prints debugging output to the standard error stream.


User guide and program description:

There are no command line arguments for the executable.  Once
launched, the executable behaves as follows:

o  The shell, on startup, reads a configuration file 'shconfig' and sets the values of RHost and RPort variables. (where RHost holds the value of ip address and RPort holds the value of port number). 

o  All local commands (which needs to be executed in foreground should be prefixed with '!' mark).

	so, commands such as '! ls' are accepted.

Test case: ! ls

Output: prints the list of files in current directory



o  All local commands which needs to be executed in background should be prefixed with '!' followed by '&'

Test case:  ! & /usr/local/bin/test464 

Output:  Incoming misile strike.  This is not a drill.
& /usr/local/bin/test464 done (0)


A new process is fork-ed for a background command.  The new process
runs then the command as a normal, foreground command and reports
completion.

A typical zombie reaper is set up as signal handler for SIGCHLD.  This
reaper is inhibited temporarily when we actually want to wait for
child completion (in function run_it) by setting an empty function as
SIGCHLD handler in the master process.  As soon as the child responds
we restore the normal handler. (Note: we do need the dummy, empty
handler as the behaviour of setting SIG_IGN as handler for SIGCHLD is
undefined according to the POSIX standard.)

The shell attempts first to run the given external command without any
path prefix.  So commands such as

  ! & /usr/local/bin/test464 

are accepted and run as expected (provided that the executable test464
is in /usr/local/bin).

o  Commands which are not prefixed by '!' are directly sent to the server for execution and response is printed back immediately.

   for each and every command, client establishes a connection, and after receiving data, it will terminate the connection

Test case:

1. In case of HTTP connection (RHost - 10.18.0.22, RPort - 9001)

shell>hello world
trying 10.18.0.22 ....
connected to 10.18.0.22  9001
ACK 1: hello world
ACK 2: hello world
terminating 10.18.0.22 ....
terminated connection 10.18.0.22 ....



2. In case of SMTP connection (RHost - 127.0.0.1, RPort - 25)

shell> mail from: bruda
trying 127.0.0.1 ....
connected to 127.0.0.1  25
220 turing.ubishops.ca ESMTP Postfix
250 2.1.0 Ok
terminating 127.0.0.1 ....
terminated connection 127.0.0.1 ....

The terminal will be blocked until entire response from server is printed. 


o   Commands which are not prefixed by '!' and are prefixed by '&' are  directly sent to the server in a background process for execution and response is printed back immediately.

 The terminal will be non blocking even before entire message from server is printed back.

sshell>  & hello
sshell> trying 10.18.0.22 ....
connected to 10.18.0.22  9001
ACK 1: hello
& world
sshell> trying 10.18.0.22 ....
connected to 10.18.0.22  9001
ACK 1: world
ACK 2: hello
ACK 2: world
terminating 10.18.0.22 ....
terminated connection 10.18.0.22 ....
done
sshell> terminating 10.18.0.22 ....
terminated connection 10.18.0.22 ....
done

o  For every interaction with server in our previous cases, the terminal (client) establishes a connection and terminates the connection once it prints entire response from server. command '! keepalive' will keep the connection alive ( i.e., a new connection will not be created for every interation with the server) until it is manually closed by '! close' command.)

Test cases:

=> without keepalive (RHost - 127.0.0.1, RPort - 25)

sshell> mail from: bruda
trying 127.0.0.1 ....
connected to 127.0.0.1  25
220 turing.ubishops.ca ESMTP Postfix
250 2.1.0 Ok
terminating 127.0.0.1 ....
terminated connection 127.0.0.1 ....
sshell> rcpt to: bruda
trying 127.0.0.1 ....
connected to 127.0.0.1  25
220 turing.ubishops.ca ESMTP Postfix
503 5.5.1 Error: need MAIL command
terminating 127.0.0.1 ....
terminated connection 127.0.0.1 ....

Here, we are connected to smtp server, In the second interaction with the server, the server did not remember our previous interaction and asks the user to enter 'mail' command.


=> with keepalive(RHost - 127.0.0.1, RPort - 25)

sshell> ! keepalive
keep alive is ON
sshell> mail from: bruda
220 turing.ubishops.ca ESMTP Postfix
250 2.1.0 Ok
sshell> rcpt to: bruda
250 2.1.5 Ok

when we use keepalive option, the connection will remain same until it is manually closed by '! close' command. The second interaction with server still remembers our first interaction and doesn't throw any error.




This particular program is simple enough to allow for an exhaustive
test suite, that contains the following test cases:

o  commands from /bin, /usr/bin, with absolute path, and nonexistent commands which are prefixed by '!'
o  commands that end up in errors (such as `ls nofile')
o  all of the above in background
o  more with various arguments (multiple files, unreadable files, nonexistent files)
o  error conditions for the configuration, such as unreadable or garbled configuration file; in particular, negative terminal dimensions were provided in the    configuration file
o  special cases for input such as empty commands and EOFs.



Question:

1. Test explicitly what happens if you issue “asynchronous” remote commands (i.e., commands prefixed by & only) in rapid succession (i.e., issue a new command before the completion of the previous one—you may want to use copy and paste here to insure the rapid succession).

Answer:

=> without keepalive


sshell> & hello
sshell> trying 10.18.0.22 ....
connected to 10.18.0.22  9001
ACK 1: hello
& world
sshell> trying 10.18.0.22 ....
connected to 10.18.0.22  9001
ACK 1: world
ACK 2: hello
ACK 2: world
terminating 10.18.0.22 ....
terminated connection 10.18.0.22 ....
done
sshell> terminating 10.18.0.22 ....
terminated connection 10.18.0.22 ....

The answers comeback interleaved with each other, because the two commands create 2 different connections with server and are executed in 2 different proeceses, where each process has their own buffers to store response from server.

process 1: creates a new connection and holds the response from server as follows:

 ACK 1: hello
 ACK 2: hello

process 2: creates a new connection and holds the response from server as follows:

 ACK 1: world
 ACK 2: world

where each of these 2 processes execute simultaneously (parallely) and print the response from server to the standard output.



=> with keepalive


sshell> ! keepalive
keep alive is ON
sshell> & hello
sshell> ACK 1: hello
& world
sshell> ACK 2: hello
ACK 1: world
ACK 2: world
& world (done)
sshell> & hello (done)


The answers are not interleaved with each other, but are sequential(responses are printed one after other), since the above 2 commands use same connection, The reponses from the server are pipelined. 

As per the pipelining concept 'A server MUST send its responses to those requests in the same order that the requests were received.'
(ref: https://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html)

so, irrespective of number of commands we send to server at the same time, it will pipeline all the responses and send one after other.

That is why, in case of `! keepalive`,(if we use same connection and issue multiple requests to server), we dont get response as interleaved.
