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

struct command *parseInput(char userInput[])
{
    // printf("%s\n", userInput);
    struct command *head = NULL;
    struct command *tail = NULL;

    char *arg = strtok(userInput, " ");

    while (arg != NULL) {
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

int main()
{
    char userInput[2048];
    struct command *commandPrompt;
    
    while (1) {
        char *buf;
        size_t buflen;

        printf(": "); // input prompt
        fflush(stdout);
        getline(&buf, &buflen, stdin);

        buf[strlen(buf)-1] = '\0'; // remove extra new line character at the end
        memset(userInput, '\0', 2048);
        strcpy(userInput, buf);

        // Quick and easy way to end program without having to manually kill it
        if (strcmp(userInput, "break") == 0) {
            free(buf);
            break;
        }

        commandPrompt = parseInput(userInput);

        printArgs(commandPrompt);
        
    }
    freeCommand(commandPrompt);
    return EXIT_SUCCESS;
}