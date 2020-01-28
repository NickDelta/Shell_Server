#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tokenizer.h"

cmd* cmd_initialize(commandType type)
{
    cmd* n = malloc(sizeof(cmd));

    n->type = type;
    n->current_args_size = 0;
    n->args_capacity = 5;
    n->arguments = malloc(n->args_capacity * sizeof(char*));
    if(n->arguments == NULL)
    {
        fprintf(stderr,"Could not allocate memory, exiting.\n");
        exit(EXIT_FAILURE);
    }
    cmd_nullify(n);
    return n;
}
void cmd_realloc(cmd* n)
{
    n->args_capacity *= 2;
    n->arguments = realloc(n->arguments, n->args_capacity * sizeof(char*));
    if(n->arguments == NULL)
    {
        fprintf(stderr,"Could not allocate memory, exiting.\n");
        exit(EXIT_FAILURE);
    }
    cmd_nullify(n);
}
bool cmd_isAlmostFull(cmd* n)
{
    if(n->current_args_size + 1 >= n->args_capacity)
        return true;
    else
        return false;

}
void cmd_addArgument(char* token, cmd* n)
{
    if(cmd_isAlmostFull(n))
        cmd_realloc(n);

    n->arguments[n->current_args_size++] = token;
}
void cmd_nullify(cmd* n)
{
    for(int i = n->current_args_size ; i < n->args_capacity ; i++)
        n->arguments[i] = NULL;
}
void free_cmd(cmd* n)
{
    free(n->arguments);
    free(n);
}

cmdHolder* cmdHolder_initialize()
{
    cmdHolder* n = malloc(sizeof(cmdHolder));

    n->current_size = 0;
    n->capacity = 5;
    n->commands = malloc(n->capacity * sizeof(cmd*));
    if(n->commands == NULL)
    {
        fprintf(stderr,"Could not allocate memory, exiting.\n");
        exit(EXIT_FAILURE);
    }
    cmdHolder_nullify(n);
    return n;
}
void cmdHolder_realloc(cmdHolder * n)
{
    n->capacity *= 2;
    n->commands = realloc(n->commands, n->capacity * sizeof(cmd*));
    if(n->commands == NULL)
    {
        fprintf(stderr,"Could not allocate memory, exiting.\n");
        exit(EXIT_FAILURE);
    }
    cmdHolder_nullify(n);
}
bool cmdHolder_isAlmostFull(cmdHolder * n)
{
    if(n->current_size + 1 >= n->capacity)
        return true ;
    else
        return false;
}
void add_cmd(cmdHolder* n , cmd* command)
{
    if(cmdHolder_isAlmostFull(n))
        cmdHolder_realloc(n);

    n->commands[n->current_size++] = command;
}
void cmdHolder_nullify(cmdHolder* n)
{
    for(int i = n->current_size ; i < n->capacity ; i++)
        n->commands[i] = NULL;
}
char** get_arguments(cmdHolder* n, int index)
{
    if(n->commands[index] != NULL)
        return n->commands[index]->arguments;
    else
        return NULL;
}
commandType get_commandType(cmdHolder* n, int index)
{
    if(n->commands[index] != NULL)
        return n->commands[index]->type;
    else
        return -1;
}
void free_cmdHolder(cmdHolder* n)
{
    for(int i = 0 ; i < n->current_size ; i++)
        free_cmd(n->commands[i]);
    free(n->commands);

    free(n);
}
bool cmdHolder_isEmpty(cmdHolder* n)
{
    if(n->current_size == 0)
        return true;
    else
        return false;
}
bool cmdHolder_hasRedirection(cmdHolder* n)
{
    int index = 0;
    while(n->commands[index] != NULL)
    {
        if (get_commandType(n,index) == OUT_REDIRECTION)
        {
            if (index == n->current_size - 1)
                return true;
            else
            {
                fprintf(stderr, "Implementation error: output redirection allowed only at the last command\n");
                return false;
            }
        }
        index++;
    }
    return false;

}


cmdHolder * tokenizer(char* string, const char* delimiters)
{
    //The first command is considered PIPED by default
    cmdHolder* command_holder = cmdHolder_initialize();
    cmd* command = cmd_initialize(PIPED);

    char *token;

    token = strtok(string, delimiters);

    //If no tokens are found, then return an empty cmdHolder
    if(token == NULL)
        return command_holder;


    /* walk through other tokens */
    while( token != NULL )
    {
        /*If a pipe or redirection is found
         *then store the command in the command_holder
         *and initialize a new command*/
        if(strcmp(token,"|") == 0)
        {
            add_cmd(command_holder, command);
            command = cmd_initialize(PIPED);
            token = strtok(NULL, delimiters);
            continue;
        }
        else if(strcmp(token,">") == 0)
        {
            add_cmd(command_holder, command);
            command = cmd_initialize(OUT_REDIRECTION);
            token = strtok(NULL, delimiters);
            continue;
        }
        //Add arguments
        cmd_addArgument(token, command);

        token = strtok(NULL, delimiters);
    }

    /*When strtok doesn't have any more tokens
     *then add the last command to the command_holder*/

    add_cmd(command_holder, command);

    return command_holder;
}
