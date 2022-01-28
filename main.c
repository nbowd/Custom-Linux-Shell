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
// Citation for the following function:
// Date: 01/20/2022
// Copied from /OR/ Adapted from /OR/ Based on:
// Source: https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
// Source: https://www.youtube.com/watch?v=0qSU0nxIZiE&ab_channel=CodeVault

void expandVar(char *source, char *substring, pid_t with)
{
    // Create temp storage for substring 
    int length = snprintf(NULL, 0, "%d", with);
    char *str = malloc(length + 1);
    
    // Convert pid int to string and store in temp
    snprintf(str, length + 1, "%d", with);
    
    // Locates expansion starting point
    char *substring_source = strstr(source, substring);

    // Handles invalid strings
    if (substring_source == NULL) {
        return;
    }

    // Moves the characters after the substring over to make room for the new substring expansion
    // This will leave a hole in the string that will be the right size for the new substring
    memmove(
        substring_source + strlen(str),
        substring_source + strlen(substring),
        strlen(substring_source) - strlen(substring) + 1
    );

    memcpy(substring_source, str, strlen(str));
    free(str);
}

struct command *createCommand(char *arg, pid_t currentPid)
{
    struct command *currCommand = malloc(sizeof(struct command));
    // Creates a new string allocation that is large enough to contain the expansion if needed
    int pidLength = snprintf(NULL, 0, "%d", currentPid);
    char *newArg = calloc(strlen(arg) + pidLength - 1, sizeof(char));
    strcpy(newArg, arg);    
    
    // Modifies newArg to reflect the expanded argument
    if (strstr(arg, "$$") != NULL) {
        expandVar(newArg, "$$", currentPid);
    }

    currCommand->argument = calloc(strlen(newArg) + 1, sizeof(char));
    strcpy(currCommand->argument, newArg);   
    currCommand->next = NULL;
    
    free(newArg);
    return currCommand;
}


struct command *parseInput(char userInput[], pid_t currentPid)
{
    // printf("%s\n", userInput);
    struct command *head = NULL;
    struct command *tail = NULL;

    char *arg = strtok(userInput, " ");

    while (arg != NULL) {
        // if (strstr(arg, "$$") != NULL) {
        //     expandVar(arg, "$$", getpid());
        //     // printf("Found the replacement string!\n");
        //     }
        struct command *newArg = createCommand(arg, currentPid);
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

void changeDirectory(struct command *current, char HOME[]) 
{
    // Advances past cd command, checks for an argument
    current = current->next;
    
    // No argument, navigates to HOME directory
    if (current == NULL) {
        chdir(HOME);
        return;
    }

    // Displays error if argument is an invalid directory, 
    // Changes directories on valid argument.
    if (chdir(current->argument) != 0) {
        perror("Invalid argument, cd not executed");
    }
}

int main()
{
    char userInput[2048];
    char HOME[] = "/nfs/stak/users/bowdenn";
    pid_t pid = getpid();
    struct command *commandPrompt;
    int exitStatus = 0;

    while (1) {
        char *buf = NULL;
        size_t buflen;
        printf(": ");
        fflush(stdout);
        getline(&buf, &buflen, stdin);

        buf[strlen(buf)-1] = '\0';
        memset(userInput, '\0', 2048);
        strcpy(userInput, buf);
        free(buf);

        if (strcmp(userInput, "exit") == 0) {
            break;
        }

        if (strcmp(userInput, "status") == 0) {
            printf("exit status %d\n", exitStatus);
            continue;
        }

        commandPrompt = parseInput(userInput, pid);
        bool valid = checkValid(commandPrompt);
        if (!valid) {continue;}

        if (strcmp(userInput, "cd") == 0) {
            changeDirectory(commandPrompt, HOME);
        }
        printArgs(commandPrompt);
        
    }
    freeCommand(commandPrompt);
    exit(0);
    return EXIT_SUCCESS;
}