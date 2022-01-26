#define _GNU_SOURCE
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct command 
{
    // bool redirect ? in the future maybe
    char *argument;
    struct command *next;
};

struct command *createCommand(char *arg)
{
    struct command *currCommand = malloc(sizeof(struct command));

    currCommand->argument = calloc(strlen(arg) + 1, sizeof(char));
    strcpy(currCommand->argument, arg);

    currCommand->next = NULL;

    return currCommand;
}

struct command *parseInput(char userInput[], char argumentsArray[])
{
    // printf("%s\n", userInput);
    struct command *head = NULL;
    struct command *tail = NULL;

    char *arg = strtok(userInput, " ");

    while (arg != NULL) {
        if (strstr(arg, "$$") != NULL) {printf("Found the replacement string!\n");}
        struct command *newArg = createCommand(arg);
        if (head == NULL) 
        {
            head = newArg;
            tail = newArg;
        } 
        else
        {
            tail->next = newArg;
            tail = newArg;
        }
        arg = strtok(NULL, " ");
    }
    return head;
}

void printArgs(struct command *current) 
{
    while(current!= NULL) {
        printf("%s\n", current->argument);
        fflush(stdout);
        current = current->next;
    }
}

void freeCommand(struct command *head)
{
    while (head != NULL) {
        free(head->argument);
        free(head);
        head = head->next;
    }
}

bool checkValid(struct command *head)
{
    if (head == NULL) {return false;}
        
    char *firstArg = head->argument;
    // printf("%c\n", firstArg[0]);
    if (firstArg[0] == '#') {
        return false;
    }
    return true;
}
int main()
{
    char userInput[2048];
    char argumentsArray[512];
    struct command *commandPrompt;
    
    while (1) {
        char *buf;
        size_t buflen;
        printf(": ");
        fflush(stdout);
        getline(&buf, &buflen, stdin);

        buf[strlen(buf)-1] = '\0';
        memset(userInput, '\0', 2048);
        strcpy(userInput, buf);
        if (strcmp(userInput, "break") == 0) {
            free(buf);
            break;
        }
        commandPrompt = parseInput(userInput, argumentsArray);
        bool valid = checkValid(commandPrompt);
        if (!valid) {continue;}
        printArgs(commandPrompt);
        
    }
    freeCommand(commandPrompt);
    return EXIT_SUCCESS;
}