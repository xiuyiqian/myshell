#include <stdlib.h>
#include <sys/wait.h>
#include "fcntl.h"
#include <unistd.h> //pipe command, parsing command line stuff, pause, forking
#include <stdlib.h> //malloc and stuff
#include <string.h> //fprintf
#include <signal.h> //kill
#include <errno.h>
#include <time.h>
#include <unistd.h>

#define ERR_PROMPT "Error: Usage: %s [prompt]\n"
#define ERR_TOKEN "Error: \"&\" must be last token on command line\n"
#define ERR_REDIRECT "Error: Ambiguous input redirection.\n"
#define ERR_INPUT "Error: Missing filename for input redirection.\n"
#define ERR_OUTPUT "Error: Ambiguous output redirection.\n"
#define ERR_FILENAME "Error: Missing filename for output redirection.\n"
#define ERR_CMD "Error: Invalid null command.\n"
#define ERR_OPEN "Error: open(\"%s\"): %s\n"
#include <stdio.h>
#include <sys/stat.h>


char ** get_tokens( const char * line ) {
    char **tokens=NULL;
    char * line_copy;
    const char * delim = " \t\n";
    char * cur_token;
    int num_tokens=0;

    tokens = (char**) malloc( sizeof(char*) );
    tokens[0] = NULL;

    if( line == NULL )
        return tokens;

    line_copy = strdup( line );
    cur_token = strtok( line_copy, delim );
    if( cur_token == NULL )
        return tokens;

    do {
        num_tokens++;
        tokens = ( char ** ) realloc( tokens, (num_tokens+1) * sizeof( char * ) );
        tokens[ num_tokens - 1 ] = strdup(cur_token);
        tokens[ num_tokens ] = NULL;
    } while( (cur_token = strtok( NULL, delim )) );
    free(line_copy);

    return tokens;
}

void free_tokens( char ** tokens ) {
    if( tokens == NULL )
        return;

    for( int i=0; tokens[i]; i++ )
        free(tokens[i]);
    free(tokens);
}

typedef struct commSet {
    struct Cmd ** commands;
    int numCmds;
    int foreground;
    int background;

} * set;

typedef struct Cmd {
    char** args;
    char* inputFile;
    char* outputFile;
    int pipeInput;
    int pipeOutput;
    int numArgs;
    int append;

} * cmd;
// store pid
typedef struct node {
    int pid;
    struct node *next;
}* n;

void freeCmd(set s, n head);
void removeNode(n head, int pid);
int main( int argc, char *argv[] )
{

    while(1) {
        n dummy = (n)malloc(sizeof (struct node));
        dummy->pid = -999;
        dummy->next = NULL;
        int status;
        int ppid;
        int error = 0;
        if (argc == 1) {  // default
            //print prompt
            //printf("call mysh\n");
            fprintf(stdout, "mysh: ");
        } else {
            if (argc == 2) {
                if (strcmp("-", argv[1]) != 0) {  // not equal
                    // print prompt
                    fprintf(stdout, "%s ", argv[1]);
                }
                else{
                    fprintf(stdout,"");
                }
                //else print nothing
            } else {
                // wrong prompt
                fprintf(stderr, ERR_PROMPT, argv[0]);
                exit(-1);
            }
        }

        char *buf = (char *) malloc(1024);  // maximize 1024 chars or less
        errno = 0;
        while (fgets(buf, 1024, stdin) == NULL) {
            continue;
        }

        if (buf == EOF || strcmp(buf, "exit\n") == 0) {
            int signal = 0;
            int pid;
            do{
                pid = wait(&status);
                if(pid>0){
                    //child process
                    kill(pid,signal);
                }
            }
            while(pid!= -1);
            return 0;
        }
        char** tokens = get_tokens(buf);

        set cmdSet = calloc(1, sizeof(struct commSet));
        cmdSet->numCmds = 1;
        cmdSet->commands = calloc(1, sizeof(char*) );
        cmd command = calloc(1, sizeof(struct Cmd));
        command->args = calloc(1, sizeof(char **));
        cmdSet->commands[0] = command;


        for( int i=0; tokens[i]; i++ ) {
            if(strcmp(tokens[i], "<") == 0) {
                i+=1; //increment i to the next token
                if (command->inputFile){
                    //printf("wrong input file %s\n", command->inputFile);
                    fprintf(stderr,ERR_REDIRECT);
                    error=1;
                    break;
                }
                if (tokens[i] == NULL){
                    fprintf(stderr,ERR_INPUT);
                    error=1;
                    break;
                }
                else {
                    command->inputFile = tokens[i];
                }
            }
            else if(strcmp(tokens[i], ">") == 0) {
                i+=1;
                if (command->outputFile){
                    fprintf(stderr,ERR_OUTPUT);
                    error=1;
                    break;
                }
                if (tokens[i] == NULL) {
                    fprintf(stderr, ERR_FILENAME);
                    error = 1;
                    break;
                }
                else {
                    command->outputFile = tokens[i];
                    struct stat buffer;
                    int exist = lstat(command->outputFile, &buffer);
                    if(exist == 0){
                        fprintf(stderr, "Error: open(\"%s\"): File exists\n", command->outputFile);
                        error = 1;
                        break;
                    }
                }
            }
            else if(strcmp(tokens[i], ">>") == 0) {
                i+=1;
                if (command->outputFile){
                    fprintf(stderr,ERR_OUTPUT);
                    error=1;
                    break;
                }
                if (tokens[i] == NULL){
                    fprintf(stderr,ERR_FILENAME);
                    error=1;
                    break;
                }
                else {
                    command->append = 1;
                    // add to existing file
                    command->outputFile = tokens[i];
                }
            }
            else if(strcmp(tokens[i], "|") == 0) {

                if(command->numArgs == 0){
                    fprintf(stderr, ERR_CMD);
                    error = 1;
                    break;
                }
                //printf("the | operator\n");
                command->pipeOutput = 1; // turn to input in the pipe
                cmdSet->numCmds += 1;
                // add one more cmd to set's comand
                cmd command1 = (set)malloc(sizeof(struct Cmd));
                command1->pipeInput = 1;
                //command1->pipeOutput = 0;
                command1->args  = malloc(sizeof(char**));
                //command1->numArgs = 0;
                // command1->append = 0;
                cmdSet->commands = realloc(cmdSet->commands, sizeof(char*) * cmdSet->numCmds);
                cmdSet->commands[cmdSet->numCmds-1] = command1;
                command = command1;
            }
            else if(strcmp(tokens[i], "&") == 0) {
                if(tokens[i+1] != NULL) {
                    fprintf(stderr, ERR_TOKEN);
                    error=1;
                    break;
                }
                else {
                    cmdSet->background=1;
                }
            }
            else {
                if(command->numArgs == 0){
                    command->numArgs += 1;
                }
                else{
                    command->numArgs += 1;
                    command->args = (char**) realloc(command->args,sizeof(char*) * command->numArgs);
                }
                //printf("store command %d: %s\n",cmdSet->numCmds, tokens[i]);
                command->args[command->numArgs-1] = tokens[i];
            }
        }
        if(error==1){
            continue;
        }
        if(cmdSet->background == 0){
            cmdSet->foreground = 1;
        }
        //printf("number of command being stored %d\n", cmdSet->numCmds);

        for(int i = 0; i<cmdSet->numCmds; i++) {
            //printf("arranging pipe: command %d\n",i);
            cmd command = cmdSet->commands[i];
            if(command->pipeOutput){
                //pipe out this command / not standard output
                int pipefds[2];
                if(command->numArgs == 0) {
                    fprintf(stderr,ERR_CMD);
                    error=1;
                    break;
                }
                else if(command->outputFile != NULL){
                    //there should be not standard output
                    fprintf(stderr,ERR_OUTPUT);
                    error=1;
                    break;
                }
                else {
                    if(pipe(pipefds)<0){
                        perror("Error: pipe()");
                        error=1;
                        break;
                    }
                    //writing end
                    command->pipeOutput = pipefds[1];
                }
                if (cmdSet->commands[i+1]){
                    // there must be another cmd
                    cmd command1 = cmdSet->commands[i+1];
                    if(command1->pipeInput == 0){
                        fprintf(stderr,"pipe no end\n");
                        error=1;
                        break;
                    } else if (command1->inputFile){
                        //printf("wrong input file in the pipe %s\n", command->inputFile);
                        fprintf(stderr,ERR_REDIRECT);
                        error=1;
                        break;
                    }
                    else if (command1->numArgs == 0){
                        if (command1->outputFile != 0 && command->pipeOutput != 0){
                            fprintf(stderr, ERR_OUTPUT);
                            error = 1;
                            break;
                        }
                        fprintf(stderr,ERR_CMD);
                        error=1;
                        break;
                    } else {
                        cmdSet->commands[i+1]->pipeInput = pipefds[0];
                    }
                }
                else {
                    fprintf(stderr, "missing pipe end\n");
                    error=1;
                    break;
                }
            }
        }
        if (error == 1){
            continue;
        }  //read for next command

        int pid;
        n head = dummy;

        for(int i = 0; i<cmdSet->numCmds; i++) {
            //printf("implementing command: command %d\n",i);
            cmd command = cmdSet->commands[i];
            pid = fork();
            if(pid > 0) {
                // parent process
                if(command->pipeOutput!=0) { //close fd[1]
                    close(command->pipeOutput);
                }
                if(command->pipeInput!=0) { //close parent's reading pipe after fork
                    close(command->pipeInput);
                }
                // number of child process = number of command
                if (cmdSet->foreground == 1){
                    n node1 = (n)malloc(sizeof (struct node));
                    node1->pid = pid;
                    node1->next = NULL;
                    //printf("inserted pid %d\n", pid);
                    head->next = node1;
                    head = head->next;
                }

            } else if (pid == 0){ //in the child

                if(command->inputFile) {
                    int fd1 = open(command->inputFile,O_RDONLY);
                    //printf("children pid %s\n", getpid());
                    //printf("input file: %s fd1 %d\n", command->inputFile, fd1);
                    if(fd1<0){
                        perror("fopen");
                        exit(1); //terminate child process
                    }
                    if(dup2(fd1,0)<0){ //redirect 0 to fd1
                        perror("dup2");
                        exit(1);
                    }
                    close(fd1);
                }
                if(command->outputFile){
                    int fd2;
                    if(command->append == 1){
                        fd2 = open(command->outputFile,
                                   O_WRONLY|O_CREAT|O_APPEND,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); //redirect in child!!
                    } else { //if we are creating a new file or exiting if one found
                        fd2 = open(command->outputFile,
                                   O_CREAT|O_WRONLY,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    }
                    if(fd2<0){
                        fprintf(stderr,ERR_OPEN,command->outputFile, strerror(errno));
                        exit(1);
                    }
                    //printf("here\n");
                    if(dup2(fd2,1)<0){
                        //redirect 1 to fd2
                        perror("dup2");
                        exit(1);
                    }
                    close(fd2);
                }
                if(command->pipeInput!=0){
                    if(dup2(command->pipeInput,0)<0){ //redirect 0 to fd1
                        perror("dup2");
                        exit(1);
                    }
                    close(command->pipeInput);
                }
                if(command->pipeOutput!=0){
                    if(dup2(command->pipeOutput,1)<0){ //redirect 0 to fd1
                        perror("dup2");
                        exit(1);
                    }
                    close(command->pipeOutput);
                }
                //printf("command %s\n",command->args[0]);

                if(execvp(command->args[0], command->args) <0){
                    perror("exec: execvp failed\n");
                    exit(-1);
                }
                exit(0);
            } else {
                fprintf(stderr,"Error: fork()\n");
                exit(-1);
            }
        }

        //terminate children in the foreground
        if(cmdSet->foreground==1){
            //printf("into the foreground\n");
            while(1)
            {
                ppid = wait(&status);
                //printf("terminated ppid : %d\n", ppid);
                if(ppid == -1){
                    //printf("Parent (%d): wait(): %s\n", getpid(), strerror(errno));
                    break;
                }
                else{
                    n head = dummy;
                    removeNode(head,ppid);
                }
                usleep(3);
            }
        }

        free(buf);
        //printf("free buf fine\n");
        free_tokens(tokens);
        //printf("free token fine\n");
        freeCmd(cmdSet, dummy);

    }

}

void removeNode(n head, int pid){
    if( !head->next){
        return;
    }
    //printf("remove function\n");
    //printf("current node ppid %d\n", head->next->pid);
    if(head->next->pid == pid){
        //printf("read to remove %d\n", pid);
        n tmp = head->next;
        head ->next = head->next->next;
        free(tmp);
        return;
    }
    removeNode(head->next, pid);
}

void freeCmd(set s, n head){
    //printf("free function\n");
    for (int i=0; i<s->numCmds; i++){
        cmd c = s->commands[i];
        if(c->args) {
            free(c->args);
        }
        free(c);
    }
    free(s);
    //printf("free Cmdset fine\n");

    while(head->next!=NULL){
        n tmp = head;
        head = head->next;
        free(tmp);
    }
    free(head);
    //printf("free node fine\n");
}

