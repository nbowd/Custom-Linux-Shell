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
#include <sys/wait.h> // for waitpid

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
    char dir[256];
    getcwd(dir, sizeof(dir));
    printf("%s\n", dir);
    // No argument, navigates to HOME directory
    if (current == NULL) {
        chdir(HOME);
        getcwd(dir, sizeof(dir));
        printf("%s\n", dir);
        return;
    }

    // Displays error if argument is an invalid directory, 
    // Changes directories on valid argument.
    if (chdir(current->argument) != 0) {
        perror("Invalid argument, cd not executed");
    }
    getcwd(dir, sizeof(dir));
    printf("%s\n", dir);
}

int countArgs(struct command *head) 
{
    int count = 0;
    
    while (head != NULL) {
        count++;
        head = head->next;
    }
    return count;
}

void listToArr(struct command *current, char **list)
{   
    int i = 0;
    while (current != NULL) {
        list[i] = current->argument;
        i++;
        current = current->next;
    }
}

// source for redirect: https://canvas.oregonstate.edu/courses/1884946/pages/exploration-processes-and-i-slash-o?module_item_id=21835982
int main()
{
    char userInput[2048];
    char HOME[] = "/nfs/stak/users/bowdenn";
    pid_t pid = getpid();
    struct command *commandPrompt;
    int exitStatus = 0;
    int targetFD = -5;
    int sourceFD = -5;
    bool redirect = false;

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
        if (strcmp(userInput, "exit") == 0 || strcmp(userInput, "exit &") == 0) {
            break;
        }

        if (strcmp(userInput, "status") == 0 || strcmp(userInput, "status &") == 0) {
            printf("exit status %d\n", exitStatus);
            continue;
        }

        // Parses the command and arguments into a linked list inside the command struct
        commandPrompt = parseInput(userInput, pid);
        bool valid = checkValid(commandPrompt);
        if (!valid) {continue;}

        // After parsing, only the command is left inside userInput.
        // This is why "cd <some file path" still triggers this condition
        if (strcmp(userInput, "cd") == 0) {
            changeDirectory(commandPrompt, HOME);
            continue;
        }
        // printArgs(commandPrompt);
        int argCount = countArgs(commandPrompt);
        printf("arg count: %d\n", argCount);
        char *args[argCount+1];
        args[argCount] = NULL;

        // int childStatus;

        // Fork a new process
        pid_t spawnPid = fork();

        switch(spawnPid){
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:
            // In the child process
            printf("CHILD(%d) running\n", getpid());

            listToArr(commandPrompt, args);
            
            for (int i=0; i < argCount; i++) {
                printf("%s, ", args[i]);

                // stdin redirection
                if (strcmp(args[i], "<") == 0) {
                    redirect = true;

                    sourceFD = open(args[i+1], O_RDONLY);
                    if (sourceFD == -1) {
                        perror("source open()");
                        exit(1);
                    }

                    int result = dup2(sourceFD, 0); // 0 == stdin
                    if (result == -1) {
                        perror("source dup2()");
                        exit(2);
                    } 
                }

                // stdout redirection
                if (strcmp(args[i], ">") == 0) {
                    redirect = true;

                    targetFD = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (targetFD == -1) {
                        perror("source open()");
                        exit(1);
                    }

                    int result = dup2(targetFD, 1); // 1 == stdout
                    if (result == -1) {
                        perror("source dup2()");
                        exit(2);
                    } 
                }
            }

            printf("NULL\n");
            printf("argument run: %s\n", args[0]);
            // // Replace the current program with "/bin/ls"
            // execl("/bin/ls", "/bin/ls", "-al", NULL);
            if (redirect) {
                for ( int i = 1; i < argCount; i++) {
                    args[i] = NULL;
                }
            }

            execvp(args[0], args);
    
            // exec only returns if there is an error
            perror("execvp");
            exitStatus = -1;
            exit(2);
            break;
        default:
            // In the parent process
            // Wait for child's termination
            spawnPid = waitpid(spawnPid, &exitStatus, 0);
            printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
            // exit(0);
            // break;
        }
        
    }
    freeCommand(commandPrompt);
    exit(0);
    return EXIT_SUCCESS;
}