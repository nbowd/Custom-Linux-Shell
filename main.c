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

        printf("%s\n", userInput);
        // commandPrompt = parseInput(userInput, argumentsArray);
        // bool valid = checkValid(commandPrompt);
        // if (!valid) {continue;}
        // printArgs(commandPrompt);
        
    }
    // freeCommand(commandPrompt);
    return EXIT_SUCCESS;
}