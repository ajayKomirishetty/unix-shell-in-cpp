/*
 * Part of the solution for Assignment 2 (CS 464).
 */

#include <stdio.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <errno.h>
#include <iostream>

#include "tokenize.h"
#include "tcp-utils.h" // for `readline' only

using namespace std;

int keep_alive = 0; // this flag is to determine whether the connection between client and server should persist until user closes the connection explicitly

size_t RPort = 0;         // contains Remote machine's  port number
const char *RHost = NULL; // contains Remote machine's  IP address
int sd;                   // Holds socket descriptor
string RHost1;

/*
 * Global configuration variables.
 */
const char *path[] = {"/bin", "/usr/bin", 0}; // path, null terminated (static)
const char *prompt = "sshell> ";              // prompt
const char *config = "shconfig";              // configuration file for more command
/*
 * Typical reaper.
 */
void zombie_reaper(int signal)
{
    int ignore;
    while (waitpid(-1, &ignore, WNOHANG) >= 0)
        /* NOP */;
}

/*
 * Dummy reaper, set as signal handler in those cases when we want
 * really to wait for children.  Used by run_it().
 *
 * Note: we do need a function (that does nothing) for all of this, we
 * cannot just use SIG_IGN since this is not guaranteed to work
 * according to the POSIX standard on the matter.
 */
void block_zombie_reaper(int signal)
{
    /* NOP */
}

/*
 * run_it(c, a, e, p) executes the command c with arguments a and
 * within environment p just like execve.  In addition, run_it also
 * awaits for the completion of c and returns a status integer (which
 * has the same structure as the integer returned by waitpid). If c
 * cannot be executed, then run_it attempts to prefix c successively
 * with the values in p (the path, null terminated) and execute the
 * result.  The function stops at the first match, and so it executes
 * at most one external command.
 */
int run_it(const char *command, char *const argv[], char *const envp[], const char **path)
{

    // we really want to wait for children so we inhibit the normal
    // handling of SIGCHLD
    signal(SIGCHLD, block_zombie_reaper);

    int childp = fork();
    int status = 0;

    if (childp == 0)
    { // child does execve
#ifdef DEBUG
        fprintf(stderr, "%s: attempting to run (%s)", __FILE__, command);
        for (int i = 1; argv[i] != 0; i++)
            fprintf(stderr, " (%s)", argv[i]);
        fprintf(stderr, "\n");
#endif
        execve(command, argv, envp); // attempt to execute with no path prefix...
        for (size_t i = 0; path[i] != 0; i++)
        { // ... then try the path prefixes
            char *cp = new char[strlen(path[i]) + strlen(command) + 2];
            sprintf(cp, "%s/%s", path[i], command);
#ifdef DEBUG
            fprintf(stderr, "%s: attempting to run (%s)", __FILE__, cp);
            for (int i = 1; argv[i] != 0; i++)
                fprintf(stderr, " (%s)", argv[i]);
            fprintf(stderr, "\n");
#endif
            execve(cp, argv, envp);
            delete[] cp;
        }

        // If we get here then all execve calls failed and errno is set
        char *message = new char[strlen(command) + 10];
        sprintf(message, "exec %s", command);
        perror(message);
        delete[] message;
        exit(errno); // crucial to exit so that the function does not
                     // return twice!
    }

    else
    { // parent just waits for child completion
        waitpid(childp, &status, 0);
        // we restore the signal handler now that our baby answered
        signal(SIGCHLD, zombie_reaper);
        return status;
    }
}

/*
 * Implements the internal command `more'.  In addition to the file
 * whose content is to be displayed, the function also receives the
 * terminal dimensions.
 */
void do_more(const char *filename, const size_t hsize, const size_t vsize)
{
    const size_t maxline = hsize + 256;
    char *line = new char[maxline + 1];
    line[maxline] = '\0';

    // Print some header (useful when we more more than one file)
    printf("--- more: %s ---\n", filename);

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        sprintf(line, "more: %s", filename);
        perror(line);
        delete[] line;
        return;
    }

    // main loop
    while (1)
    {
        for (size_t i = 0; i < vsize; i++)
        {
            int n = readline(fd, line, maxline);
            if (n < 0)
            {
                if (n != recv_nodata)
                { // error condition
                    sprintf(line, "more: %s", filename);
                    perror(line);
                }
                // End of file
                close(fd);
                delete[] line;
                return;
            }
            line[hsize] = '\0'; // trim longer lines
            printf("%s\n", line);
        }
        // next page...
        printf(":");
        fflush(stdout);
        fgets(line, 10, stdin);
        if (line[0] != ' ')
        {
            close(fd);
            delete[] line;
            return;
        }
    }
    delete[] line;
}

void remove_keepalive()
{
    keep_alive = 0;
    if (sd > 0 || sd == -1)
    {
        shutdown(sd, SHUT_RDWR); // sends an EOF to server saying that client is terminating
        close(sd);
        printf("keep alive is OFF\n");
    }
    else
        printf("there is no active connection to be terminated\n");
}

void add_keepalive()
{
    keep_alive = 1;
    sd = connectbyportint(RHost, RPort);
    //if(errno>0)
    // printf("%s\n",strerror(errno));
    printf("keep alive is ON\n");
}

void sendToServer(string command)
{
    if (keep_alive == 0)
    {
        printf("trying %s ....\n", RHost);
        sd = connectbyportint(RHost, RPort); // connect to server
    }

    if (sd > 0)
    { // socket is created
        char buf[256];
        string temp = command;
        strncpy(buf, temp.c_str(), sizeof(buf));

        const int ALEN = 2400;
        char ans[ALEN];
        char *ans_ptr = ans;
        int ans_to_go = ALEN, n = 0;
        if (keep_alive == 0)
            printf("connected to %s  %ld\n", RHost, RPort);

        buf[strlen(buf)] = '\n'; // add /n at end of string entered by user
        buf[(strlen(buf) + 1)] = '\0';

        int a = send(sd, buf, strlen(buf), 0); // send data to server

        if (a < 0)
            printf("unable to send data to server");

        //if(errno>0)
        // printf("%s / %d",strerror(errno), a);

        struct timeval tv;
        tv.tv_sec = 3; // set 5 seconds timeout
        tv.tv_usec = 0;
        setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

        //n = read(sd, ans_ptr, 255);  n = 0; // eliminate the first imcomplete received data message

        while ((n = recv(sd, ans_ptr, ans_to_go, 0)) >= 0)
        {
            if (n == 0)
            { //when server disconnects the connection it sends eof, which means no bytes received
                //printf("eof reached");
                sd = -1;
                break;
            }
            for (int i = 0; i < n; ++i)
            {
                printf("%c", ans_ptr[i]);
            }
        }

        if (keep_alive == 0)
        {
            printf("terminating %s ....\n", RHost);
            shutdown(sd, SHUT_RDWR);
            close(sd);
            printf("terminated connection %s ....\n", RHost);
        }
    }
    if (sd == -1 && RPort == 25)
    {
        printf("connection closed by server.\n1. use '! close' to close your client or \n2. use '! keepalive' to reinitialize new connection \n");
    }
}

int connect_in_background(string command)
{
    int bgp = fork();
    if (bgp == 0)
    { // child executing the command
        sendToServer(command);
        cout << "& " << command << "(done)\n";
        return 0;
    }
    else
        return 1;
}

int main(int argc, char **argv, char **envp)
{
    size_t hsize = 0, vsize = 0; // terminal dimensions, read from
                                 // the config file
    char command[129];           // buffer for commands
    command[128] = '\0';
    char *com_tok[129]; // buffer for the tokenized commands
    size_t num_tok;     // number of tokens

    printf("Simple shell v1.0.\n");

    // Config: setup configuration for more command
    int confd = open(config, O_RDONLY);
    if (confd < 0)
    {
        perror("config");
        fprintf(stderr, "config: cannot open the configuration file.\n");
        fprintf(stderr, "config: will now attempt to create one.\n");
        confd = open(config, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        if (confd < 0)
        {
            // cannot open the file, giving up.
            perror(config);
            fprintf(stderr, "config: giving up...\n");
            return 1;
        }
        close(confd);
        // re-open the file read-only
        confd = open(config, O_RDONLY);
        if (confd < 0)
        {
            // cannot open the file, we'll probably never reach this
            // one but who knows...
            perror(config);
            fprintf(stderr, "config: giving up...\n");
            return 1;
        }
    }

    // read terminal size
    while (hsize == 0 || vsize == 0 || RHost == NULL || RPort == 0)
    {
        int n = readline(confd, command, 128);
        if (n == recv_nodata)
            break;
        if (n < 0)
        {
            sprintf(command, "config: %s", config);
            perror(command);
            break;
        }
        num_tok = str_tokenize(command, com_tok, strlen(command));
        // note: VSIZE and HSIZE can be present in any order in the
        // configuration file, so we keep reading it until the
        // dimensions are set (or there are no more lines to read)
        if (strcmp(com_tok[0], "VSIZE") == 0 && atol(com_tok[1]) > 0)
            vsize = atol(com_tok[1]);

        else if (strcmp(com_tok[0], "HSIZE") == 0 && atol(com_tok[1]) > 0)
            hsize = atol(com_tok[1]);

        else if (strcmp(com_tok[0], "RHOST") == 0)
        {
            RHost1.assign(com_tok[1]);
            RHost = RHost1.c_str();
        }

        else if (strcmp(com_tok[0], "RPORT") == 0 && atol(com_tok[1]) > 0)
        {
            RPort = atol(com_tok[1]);
        }
        // lines that do not make sense are hereby ignored
    }
    close(confd);

    if (hsize <= 0)
    {
        // invalid horizontal size, will use defaults (and write them
        // in the configuration file)
        hsize = 75;
        confd = open(config, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        write(confd, "HSIZE 75\n", strlen("HSIZE 75\n"));
        close(confd);
        fprintf(stderr, "%s: cannot obtain a valid horizontal terminal size, will use the default\n", config);
    }
    if (vsize <= 0)
    {
        // invalid vertical size, will use defaults (and write them in
        // the configuration file)
        vsize = 40;
        confd = open(config, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        write(confd, "VSIZE 40\n", strlen("VSIZE 40\n"));
        close(confd);
        fprintf(stderr, "%s: cannot obtain a valid vertical terminal size, will use the default\n", config);
    }

    printf("Terminal set to %ux%u.\n", (unsigned int)hsize, (unsigned int)vsize);
    printf("Remote set to %s %ld \n", RHost, RPort);

    // install the typical signal handler for zombie cleanup
    // (we will inhibit it later when we need a different behaviour,
    // see run_it)
    signal(SIGCHLD, zombie_reaper);

    // Command loop:
    while (1)
    {

        // Read input:
        printf("%s", prompt);
        fflush(stdin);
        if (fgets(command, 128, stdin) == 0)
        {
            // EOF, will quit
            printf("\nBye\n");
            return 0;
        }
        // fgets includes the newline in the buffer, get rid of it
        if (strlen(command) > 0 && command[strlen(command) - 1] == '\n')
            command[strlen(command) - 1] = '\0';

        // Tokenize input:
        char **real_com = com_tok; // may need to skip the first
                                   // token, so we use a pointer to
                                   // access the tokenized command
        num_tok = str_tokenize(command, real_com, strlen(command));
        real_com[num_tok] = 0; // null termination for execve

        if (strlen(com_tok[0]) == 0) // no command, luser just pressed return
            continue;

        int bg = 0, fg = 0, server_bg = 0;

        if (strcmp(com_tok[0], "!") == 0)
        { // foreground command coming
            fg = 1;
            // discard the first token now that we know that it
            // specifies a background process...
            real_com = com_tok + 1;
        }

        if ((strcmp(com_tok[0], "!") == 0) && (strcmp(com_tok[1], "&") == 0))
        { // background command coming
            bg = 1;
            // discard the first 2 tokens now that we know that it
            // specifies a background process...
            real_com = com_tok + 2;
        }

        if (strcmp(com_tok[0], "&") == 0)
        { // This command should be sent to the server in a background process
            server_bg = 1;
            // discard the first token now that we know that it
            // specifies a background process...
            real_com = com_tok + 1;
        }

        // ASSERT: num_tok > 0

        // Process input:
        if (strlen(real_com[0]) == 0) // no command, luser just pressed return
            continue;

        else if ((strcmp(com_tok[0], "!") == 0) && (strcmp(real_com[0], "exit") == 0))
        {
            printf("Bye\n");
            return 0;
        }

        else if (strcmp(real_com[0], "more") == 0)
        {
            // note: more never goes into background (any prefixing
            // `&' is ignored)
            if (real_com[1] == 0)
                printf("more: too few arguments\n");
            // list all the files given in the command line arguments
            for (size_t i = 1; real_com[i] != 0; i++)
                do_more(real_com[i], hsize, vsize);
        }

        else if ((strcmp(com_tok[0], "!") == 0) && (strcmp(real_com[0], "keepalive") == 0))
        {
            // This keep the connection alive until it is manually closed by '! close' command
            add_keepalive();
        }

        else if ((strcmp(com_tok[0], "!") == 0) && (strcmp(real_com[0], "close") == 0))
        {
            // This will terminate the active connection
            remove_keepalive();
        }

        else
        { // external command
            if (bg)
            { // background command, we fork a process that
                // awaits for its completion
                int bgp = fork();
                if (bgp == 0)
                { // child executing the command
                    int r = run_it(real_com[0], real_com, envp, path);
                    printf("& %s done (%d)\n", real_com[0], WEXITSTATUS(r));
                    if (r != 0)
                    {
                        printf("& %s completed with a non-null exit code\n", real_com[0]);
                    }
                    return 0;
                }
                else // parent goes ahead and accepts the next command
                    continue;
            }
            else if (fg)
            { // foreground command, we execute it and wait for completion
                int r = run_it(real_com[0], real_com, envp, path);
                if (r != 0)
                {
                    printf("%s completed with a non-null exit code (%d)\n", real_com[0], WEXITSTATUS(r));
                }
            }
            else
            { // commands to be executed at server side
                string command = "";

                for (int i = 0; real_com[i] != 0; i++)
                {
                    command += real_com[(i)];
                    // if(i != num_tok)
                    command += " ";
                }

                if (server_bg) // command that needs to be executed at server in a background process
                {
                    int r = connect_in_background(command);
                    if (r == 0)
                    {
                        signal(SIGCHLD, zombie_reaper);
                        return 0;
                    }
                }
                else
                {
                    sendToServer(command);
                }
            }
        }
    }
}
