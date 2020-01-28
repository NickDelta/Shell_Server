#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include "tokenizer.h"

#define BUFFER_SIZE 4096
#define MAX_CHUNK 4096
int server_sock;

int client_connections = 0;

#define PIN_SUCCESS 0
#define PIN_FAILURE 1

#define EMPTY_FD (-2)

void sigint_handler(int signum)
{
    close(server_sock);
    puts("\nServer main process terminated");
    exit(EXIT_SUCCESS);
}

void sigchld_handler(int signum)
{
    int status;
    //Harvest all zombies
    while(waitpid(-1, &status, WNOHANG) > 0);
}

ssize_t recv_all(int sockfd, char *buf, int flags)
{
    /*IN THIS VERSION OF recv_all WE EXPECT THE READ SIZE
     *NOT TO EXCEED THE BUFFER_SIZE CONSTANT. IF IT DOES,
     * SOMETHING WENT WRONG*/
    ssize_t received_bytes;

    //Read the total expected transmission size
    ssize_t toread = 0;
    received_bytes = read(sockfd, &toread, sizeof(toread));

    if (received_bytes <= 0)
        return received_bytes;

    //printf("Preparing to receive %ld bytes total\n", toread);

    ssize_t totalread = 0;

    //Continue reading from socket until there are bytes left to read
    while (toread > 0)
    {
        //Read the chunk that the other end will attempt to send
        size_t chunk_size;
        received_bytes = read(sockfd, &chunk_size, sizeof(chunk_size));

        if(received_bytes <= 0)
            return received_bytes;

        //If the expected chunk is bigger than the buffer size then something went wrong
        if(chunk_size > BUFFER_SIZE)
        {
            fprintf(stderr,"Incompatible chunk size. Something went wrong.");
            return -1;
        }

        bzero(buf,BUFFER_SIZE + 1);
        received_bytes = recv(sockfd, buf, chunk_size, flags);

        if (received_bytes <= 0)
            return received_bytes;

        totalread += received_bytes;
        toread -= received_bytes;
    }

    return totalread;
}

ssize_t send_all(int socket, char *buf,ssize_t size)
{
    ssize_t total_sent = 0; // how many bytes we've sent
    ssize_t sent_bytes;

    //Inform client for the total_sent size of the transmission
    ssize_t total_size = size;
    //printf("Notifying client that I want to send %ld bytes total \n",size);
    sent_bytes = write(socket,&total_size, sizeof(total_size));

    if(sent_bytes <= 0)
        return sent_bytes;

    ssize_t transmission_size;
    //While all bytes have not been sent
    while(total_sent < size)
    {
        /*CHUNK SIZE
         * If the remaining data are bigger than our max chunk size then send MAX_CHUNK
         * Else send the smaller remaining portion
         */
        if((size - total_sent) > MAX_CHUNK )
            transmission_size = MAX_CHUNK;
        else
            transmission_size = size - total_sent;

        //Send the decided chunk size to the client
        //printf("Notifying client that I want to send a %ld byte chunk\n",transmission_size);
        sent_bytes = write(socket,&transmission_size, sizeof(transmission_size));

        if(sent_bytes <= 0)
            return sent_bytes;

        //Send the chunk to the client
        sent_bytes = send(socket, buf + total_sent, transmission_size, 0);
        //printf("Sent %ld bytes\n", sent_bytes);

        if (sent_bytes <= 0)
            return sent_bytes;

        total_sent += sent_bytes;
    }
    return total_sent;
}

ssize_t sendall_pipe(int socket, int fd )
{
    /*THE BASIC VERSION OF THE PIPE SENDING FUNCTION*/

    ssize_t sent_bytes;
    ssize_t total_sent = 0;
    int count = -1;
    char byte;
    char* buff;

    //While the file descriptor contains at least 1 byte
    while (read(fd, &byte, 1) == 1)
    {
        //Find the size of the file descriptor and store it to count
        if (ioctl(fd, FIONREAD, &count) != -1)
        {
            //fprintf(stdout, "Preparing to send %d bytes\n", count + 1);
            //Allocate the right-sized buffer
            buff = malloc(count + 1);
            //Store the first byte read
            buff[0] = byte;
            //Read all the remaining bytes
            if (read(fd, buff + 1, count) == count)
            {
                //Send all the buffer through sockets
                sent_bytes = send_all(socket, buff,count+1);
                if (sent_bytes <= 0)
                    return sent_bytes;

                total_sent += sent_bytes;
            }
            free(buff);
        }
    }
    if(count < 0)
        return EMPTY_FD;
    else
        return total_sent;
}

int sendall_pipe_file(int socket,int fd, char* filename)
{
    /*AN ALTERATION OF sendall_pipe. INSTEAD OF SENDING
     *THE FILE DESCRIPTOR THROUGH SOCKETS, IT IS STORED
     *IN A FILE.*/

    ssize_t sent_bytes;

    //Create the specified file
    int filefd = open(filename, O_WRONLY | O_CREAT, 0644);
    if(filefd == -1)
    {
        perror("open()");

        char *error_msg = strerror(errno);
        sent_bytes = send_all(socket, error_msg, strlen(error_msg));

        if(sent_bytes <= 0)
            return -1;

        return EXIT_FAILURE;
    }

    int count;
    char byte;
    char* buff;
    while (read(fd, &byte, 1) == 1)
    {
        if (ioctl(fd, FIONREAD, &count) != -1)
        {
            //fprintf(stdout, "Preparing to send %d bytes\n", count + 1);
            buff = malloc(count + 1);
            buff[0] = byte;
            if (read(fd, buff + 1, count) == count)
            {
                //Write data to the file
                ssize_t written_bytes = write(filefd, buff, count + 1);
                //fprintf(stdout,"Wrote %zi bytes to file\n",written_bytes);

                if(written_bytes < count)
                {
                    perror("write()");

                    char *error_msg = strerror(errno);
                    sent_bytes = send_all(socket, error_msg, strlen(error_msg));

                    if(sent_bytes <= 0)
                        return -1;

                    return EXIT_FAILURE;
                }

                sent_bytes = send_all(socket, "File made successfully.", 23);
                if(sent_bytes <= 0)
                    return -1;
            }
            free(buff);
        }
    }
    //Close the file
    close(filefd);
    return EXIT_SUCCESS;
}

/*Main idea taken from https://stackoverflow.com/questions/17630247/coding-multiple-pipe-in-c
 *But modified to match the command parser needs and some other requirements*/
int exec_pipes(int fd, cmdHolder *commands)
{
    int   fds[2];
    pid_t pid;
    int   fd_in = fd;

    int current_cmd = 0;

    char** args;
    //While there are arguments left and are not redirection commands
    while ((args = get_arguments(commands, current_cmd) ) != NULL && get_commandType(commands,current_cmd) == PIPED )
    {
        pipe(fds);
        int status;
        switch(pid = fork())
        {
            case -1: //fork() failed
                perror("fork()");
                exit(EXIT_FAILURE);
            case 0: //Child
                dup2(fd_in, 0); //Change the input according to the old one

                char** next_args = get_arguments(commands, current_cmd + 1);
                commandType next_type = get_commandType(commands,current_cmd + 1);

                //If there is a next command and is PIPED
                if (next_args != NULL && next_type == PIPED)
                    dup2(fds[1], 1);

                close(fds[0]);

                execvp(args[0], args);

                perror("Execvp()");
                exit(errno);

            default: //Parent
                waitpid(pid,&status,0);

                if (WIFEXITED(status))
                {
                    int res = WEXITSTATUS(status);
                    if(res != 0)
                    {
                        /*IF A COMMAND DIDN'T EXECUTE SUCCESSFULLY THEN
                         *INFORM THE CLIENT THAT SOMETHING WENT WRONG
                         *ON THAT COMMAND.*/
                        char* error_msg = strerror(res);
                        printf("execvp() executing %s : %s\n",args[0],error_msg);
                    }
                }

                close(fds[1]);
                fd_in = fds[0]; //save the input for the next command
                current_cmd++;
                break;
        }
    }
    return EXIT_SUCCESS;
}


int connection_handler(int sockfd, int client_id)
{
    char client_message[BUFFER_SIZE];

    ssize_t sent_bytes;

    /*DELTA SIMPLE (AND COMPLETELY INSECURE) PASSWORD PROTOCOL
     * 1. SERVER INFORMS CLIENTS ABOUT THEIR UNIQUE CLIENT ID (SO THE USER CAN FIND THE CORRECT PASS FOR THEIR CLIENT INSTANCE)
     * 2. A RANDOM PIN IS GENERATED ON THE SERVER (0000-9999)
     * ONLY THE SERVER KNOWS THE PASSWORD
     * 3. CLIENT CAN MAKE ATTEMPTS UNTIL THEY ENTER THE CORRECT PASS (BRUTE-FORCE LOVERS SMILE HERE ONLY 10^4)
     * 4. SERVER CHECKS EVERY ATTEMPT AND RETURNS A FLAG OF SUCCESS OR FAILURE TO THE CLIENT
     * 5. WHEN THE CORRECT PIN IS RECEIVED BOTH SERVER AND CLIENT ARE READY TO DO THEIR JOBS
    */

    sent_bytes = write(sockfd, &client_id, sizeof(client_id));
    if (sent_bytes <= 0)
        return EXIT_FAILURE;

    srand((unsigned int) time(NULL));
    int PIN = rand() % 10000 ;

    int received_PIN;
    printf("Client #%d must use %04d as PIN to login\n",client_id, PIN);
    do
    {
        ssize_t received_bytes = read(sockfd, &received_PIN, sizeof(received_PIN));
        if (received_bytes <= 0)
            return EXIT_FAILURE;

        int answer;
        if(received_PIN != PIN)
        {
            printf("Client #%d entered wrong PIN number\n",client_id);
            answer = PIN_FAILURE;
        }
        else
        {
            printf("Client #%d entered correct PIN number\n",client_id);
            answer = PIN_SUCCESS;
        }
        sent_bytes =  write(sockfd, &answer, sizeof(answer));

        if (sent_bytes <= 0)
            return EXIT_FAILURE;

    }while(received_PIN != PIN);

    while(true)
    {
        //Receive the message from the client
        ssize_t read_size = recv_all(sockfd, client_message, 0);
        if(read_size  <= 0)
            return EXIT_FAILURE;

        printf("Received from client #%d (%zd bytes): %s\n", client_id,read_size,client_message);

        //Tokenize
        const char delimiters[] = {' ', '\n', '\t', '\0'};
        cmdHolder *cmds = tokenizer(client_message, delimiters);

        //If the struct doesn't contain any commands
        if(cmdHolder_isEmpty(cmds))
        {
            send_all(sockfd, "Could not parse any commands. Did you type anything?", 53);
        }
        //If the client has pressed only exit
        else if ( strcmp(get_arguments(cmds, 0)[0], "exit") == 0 )
        {
            if(cmds->current_size == 1 &&
               cmds->commands[0]->current_args_size == 1 )
            {
                printf("Client #%d requested exit. Process: %d\n", client_id, getpid());

                int shut_res = close(sockfd);
                if (shut_res < 0)
                {
                    perror("Socket shutdown failed");
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }
            else
                send_all(sockfd, "exit: Syntax error: no arguments or other commands must be provided", 68);
        }
        //If the client has typed cd with 1 argument
        else if ( strcmp(get_arguments(cmds, 0)[0], "cd") == 0 )
        {
            if(cmds->current_size == 1 &&
               cmds->commands[0]->current_args_size == 2 )
            {
                //Change the directory
                int exit_code = chdir(get_arguments(cmds, 0)[1]);
                if (exit_code < 0)
                {
                    //If error occurs send the error message through sockets
                    char *error_msg = strerror(errno);
                    send_all(sockfd, error_msg, strlen(error_msg) + 1);
                } else
                    send_all(sockfd, "Successfully changed directory", 31);
            }
            else
                send_all(sockfd,"cd: Syntax error: 1 argument and no other commands must be provided", 68);

        }
        //Execute the supplied command(s) as exec-supported
        else
        {
            //Storing the result of exec_pipes through piping

            int fds[2];
            pipe(fds);

            int pid;
            switch (pid = fork())
            {
                case -1: //Fork error
                {
                    perror("fork()");
                    return EXIT_FAILURE;
                }
                case 0: //Child
                {
                    dup2(fds[1], 1);
                    close(fds[0]);

                    int exit_code = exec_pipes(fds[1], cmds);

                    /*When all commands are successfully executed, a 0 is returned from exec_pipes.
                     * If something else has been returned then something went wrong.
                     * So we shall give the child the proper exit code depending on the exec_pipes
                     * return code so as to notify the parent. */

                    close(fds[1]);

                    if (exit_code != 0)
                        exit(EXIT_FAILURE);
                    else
                        exit(EXIT_SUCCESS);

                }
                default: //Parent
                {
                    dup2(fds[0], 0);
                    close(fds[1]);

                    /*Waiting for the child to finish executing (taking care of zombies)
                     * and simultaneously getting the exit_code of the child*/

                    int child_status;
                    int res = waitpid(pid, &child_status, 0);

                    if (res < 0)
                        perror("waitpid()");

                    //If the child has terminated in an anticipated fashion
                    if (WIFEXITED(child_status))
                    {
                        //If the status returned is 0
                        if (WEXITSTATUS(child_status) == 0)
                            printf("Command executed successfully. Ready for transmission\n");
                        else
                            fprintf(stderr, "Command executed with errors. Ready to transmit error message\n");
                    }
                    else
                    {
                        fprintf(stderr, "Fatal error occured. Exiting abnormally...\n");
                        return EXIT_FAILURE;
                    }

                    /*Checking if the command has redirection.
                     * As a reminder, redirection is allowed only at the end.
                     * If given anywhere else but in the end, the result of the command
                     * before that redirection will be sent to the client */

                    if (cmdHolder_hasRedirection(cmds))
                    {
                        int redirection_result = sendall_pipe_file(sockfd,fds[0], get_arguments(cmds, cmds->current_size - 1)[0]);

                        switch(redirection_result)
                        {
                            case EXIT_SUCCESS:
                                fprintf(stdout, "Client #%d: Redirection completed successfully.\n", client_id);
                                break;
                            case EXIT_FAILURE:
                                fprintf(stderr, "Client #%d: Redirection completed with errors.\n", client_id);
                                break;
                            case -1:
                                fprintf(stderr, "Client #%d: Error in transmission.\n", client_id);
                                return EXIT_FAILURE;
                            default:
                                fprintf(stderr, "Implementation error: Undefined scenario.\n");
                        }
                    }
                    else
                    {

                        /*TRANSMIT THE COMMAND RESULT TO THE CLIENT
                         *IF THE COMMAND HAD OUTPUT THEN SEND IT THROUGH SOCKETS
                         *BUT SOME COMMANDS (e.g rm) DO NOT PRODUCE OUTPUT EVEN IN
                         *SUCCESSFUL EXECUTION. IN THOSE CASES A \0 BECAUSE THE CLIENT
                         *MUST RECEIVE SOMETHING*/

                        sent_bytes = sendall_pipe(sockfd, fds[0]);
                        switch(sent_bytes)
                        {
                            case EMPTY_FD:
                            {
                                sent_bytes = send_all(sockfd, "", 1);

                                if (sent_bytes <= 0)
                                {
                                    fprintf(stderr, "Client #%d: Error in transmission.\n", client_id);
                                    return EXIT_FAILURE;
                                }
                                break;
                            }
                            default:
                            {
                                if (sent_bytes <= 0)
                                {
                                    fprintf(stderr, "Client #%d: Error in transmission.\n", client_id);
                                    return EXIT_FAILURE;
                                }
                            }
                        }
                    }
                    close(fds[0]);
                    break;
                }
            }
        }
        //Free the memory allocated for the structures
        free_cmdHolder(cmds);
    }
}

int initiate_server(unsigned short port)
{
    struct sockaddr_in server;
    struct sockaddr_in client;
    int socklength = sizeof(struct sockaddr_in);

    //Create socket
    server_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sock < 0)
    {
        perror("Socket");
        exit(EXIT_FAILURE);
    }

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );

    //Bind
    if(bind(server_sock, (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("Bind");
        exit(EXIT_FAILURE);
    }
    else
        puts("Bind done");

    //Listen
    if(listen(server_sock , 5) < 0 )
    {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    else
        puts("Waiting for incoming connections...");

    //Accept new connections
    int new_socket;
    while((new_socket = accept(server_sock, (struct sockaddr *)&client, (socklen_t*)&socklength) ) > 0 )
    {
        //If something went wrong in the accept process
        if (new_socket < 0)
        {
            perror("Connection accept failed");
            exit(EXIT_FAILURE);
        }

        //Increment total client connections since the start of server's execution
        client_connections++;

        //Get info about the client
        char *client_ip = inet_ntoa(client.sin_addr);
        int client_port = ntohs(client.sin_port);

        printf("Connection accepted from IP: %s port: %d Client #%d\n", client_ip, client_port, client_connections);

        //Multiple clients support
        int pid;
        int client_id;
        switch(pid = fork())
        {
            case -1: //Fork error
            {
                perror("fork()");
                exit(EXIT_FAILURE);
            }
            case 0: //Child
            {
                //Give the client a unique ID
                client_id = client_connections;

                //Execute the handler for that client
                int exit_status = connection_handler(new_socket, client_id);

                printf("Client #%d disconnected : Address: %s port: %d\n", client_id, client_ip, client_port);

                switch(exit_status)
                {
                    case EXIT_SUCCESS:
                        printf("Client #%d exited properly (exit).\n",client_id);
                        break;
                    case EXIT_FAILURE:
                        fprintf(stderr,"Client #%d exited abnormally.\n",client_id);
                }
                exit(EXIT_SUCCESS);
            }
            default: //Parent
            {
                /*We cannot wait here for the child to finish because that would hang the accept of new clients.
                 * Instead, to prevent zombies, the program relies on the sigchld_handler when SIGCHLD is triggered
                 * to do the job. */
                printf("Handler assigned by creating process %d\n", pid);
                break;
            }
        }
    }
    exit(EXIT_SUCCESS);
}

int main(int argc , char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr,"Usage: ./server [port]\n");
        exit(EXIT_FAILURE);
    }

    //Convert port from string to integer
    errno = 0;
    char* strtol_check = NULL;
    long int port = strtol(argv[1],&strtol_check,10);
    //Check for validity of the port
    if((port == 0 && errno != 0 ) ||
       argv[1] == strtol_check || //From documentation
       port <= 0 || port > 65535)
    {
        fprintf(stderr,"Invalid port argument (1-65535 only)\n");
        exit(EXIT_FAILURE);
    }

    //Replacing the default handlers
    signal(SIGCHLD,sigchld_handler);
    signal(SIGINT,sigint_handler);

    //Start the server
    initiate_server((unsigned short)port);
}
