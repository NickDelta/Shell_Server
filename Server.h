//
// Created by delta on 3/1/20.
//

#include <stdio.h>
#include "tokenizer.h"

#ifndef SHELL_SERVER_H
#define SHELL_SERVER_H

//Basic Server functions
int initiate_server(unsigned short);
int connection_handler(int, int);

//Execute commands
int exec_pipes(int, cmdHolder*);

//Custom implementations of recv and send
ssize_t recv_all(int, char *, int);
ssize_t send_all(int, char *,ssize_t);

//Send fds to desired destinations
ssize_t sendall_pipe(int, int);
int sendall_pipe_file(int,int, char*);

//Signal handlers
void sigint_handler(int);
void sigchld_handler(int);

#endif //SHELL_SERVER_H
