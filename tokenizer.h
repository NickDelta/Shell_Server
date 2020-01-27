//
// Created by delta on 20/1/20.
//

#ifndef TOKENIZER_TOKENIZER_H
#define TOKENIZER_TOKENIZER_H

#include <stdbool.h>

//Command type
typedef enum
{
    PIPED = 0,
    OUT_REDIRECTION = 1
}commandType;

//Struct that holds the arguments of a command
struct command
{
    commandType type ;
    char** arguments;
    int current_args_size;
    int args_capacity;
};
typedef struct command cmd;

//Command struct functions
cmd* cmd_initialize(commandType);
void cmd_realloc(cmd*);
bool cmd_isAlmostFull(cmd*);
void cmd_addArgument(char *token, cmd *n);
void cmd_nullify(cmd*);
void free_cmd(cmd*);

//Struct that holds commands (command structs)
struct commands_holder
{
    cmd** commands;
    int current_size;
    int capacity;
};
typedef struct commands_holder cmdHolder;

//commands_holder struct functions

cmdHolder* cmdHolder_initialize(void);
void cmdHolder_realloc(cmdHolder *);
bool cmdHolder_isAlmostFull(cmdHolder *);
void add_cmd(cmdHolder*, cmd*);
void cmdHolder_nullify(cmdHolder *);
char** get_arguments(cmdHolder*, int);
commandType get_commandType(cmdHolder*, int);
void free_cmdHolder(cmdHolder *);
bool cmdHolder_isEmpty(cmdHolder *);
bool cmdHolder_hasRedirection(cmdHolder *);
cmdHolder * tokenizer(char*, const char*);

#endif //TOKENIZER_TOKENIZER_H
