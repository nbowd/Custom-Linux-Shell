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
#include <signal.h>

bool fgOnly = false;

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
    if (head == NULL) {
        printf("\n");
        fflush(stdout);
        return false;
    }
        
    char *firstArg = head->argument;
    // printf("%c\n", firstArg[0]);
    if (firstArg[0] == '#') {
        printf("\n");
        fflush(stdout);
        return false;
    }
    return true;
}

void changeDirectory(struct command *current, char HOME[]) 
{
    // Advances past cd command, checks for an argument
    current = current->next;
    // char dir[256];
    // getcwd(dir, sizeof(dir));
    // printf("%s\n", dir);
    // No argument, navigates to HOME directory
    if (current == NULL) {
        chdir(HOME);
        // getcwd(dir, sizeof(dir));
        // printf("%s\n", dir);
        return;
    }

    // Displays error if argument is an invalid directory, 
    // Changes directories on valid argument.
    if (chdir(current->argument) != 0) {
        perror("Invalid argument, cd not executed");
    }
    // getcwd(dir, sizeof(dir));
    // printf("%s\n", dir);
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
    // if ((strcmp(list[i-1], "&") == 0) && current->next == NULL) {
    //     list[i-1] = NULL;
    // }
}

void handle_SIGTSTP(int signo){
    if (fgOnly) {
        char *message = "Exiting foreground-only mode\n: ";
        fgOnly = false;
        write(STDOUT_FILENO, message, 32);
    }
    else
    {
	    char* message = "Entering foreground-only mode (& is now ignored)\n: ";
	    // We are using write rather than printf
	    fgOnly = true;
	    write(STDOUT_FILENO, message, 52);
    }
    // char *message = "you made it! \n: ";
    // write(STDOUT_FILENO, message,17);
}

// source for redirect: https://canvas.oregonstate.edu/courses/1884946/pages/exploration-processes-and-i-slash-o?module_item_id=21835982
int main()
{
    char userInput[2048];
    char HOME[] = "/nfs/stak/users/bowdenn";
    pid_t pid = getpid();
    struct command *commandPrompt;
    int childExitMethod = 0;
    int targetFD = -5;
    int sourceFD = -5;
    bool targetRedirect = false;
    bool sourceRedirect = false;
    bool background = false;

    // Initialize sigint struct to be empty
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // ignore sigint
    // SIGINT_action.sa_handler = handle_SIGINT;
    SIGINT_action.sa_handler = SIG_IGN;
    // Block all catchable signals while running;
    sigfillset(&SIGINT_action.sa_mask);
    // No flags set
    SIGINT_action.sa_flags = 0;
    // Install signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);


    // Replace SIGSTP with new function
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    // Block all catchable signals while running;
    sigfillset(&SIGTSTP_action.sa_mask);
    // No flags set
    SIGTSTP_action.sa_flags = SA_RESTART;
    // Install signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

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
            if (WIFEXITED(childExitMethod)) {
                printf("exit value %d\n", WEXITSTATUS(childExitMethod));
                fflush(stdout);

            }
            else {
                printf("child terminated by signal %d\n", WTERMSIG(childExitMethod));
                fflush(stdout);
            }
            // printf("exit status %d\n", childExitMethod);
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
            printf("\n");
            fflush(stdout);
            continue;
        }
        // printArgs(commandPrompt);
        int argCount = countArgs(commandPrompt);
        // printf("arg count: %d\n", argCount);
        char *args[argCount+1];
        args[argCount] = NULL;

        struct command *argsHead = commandPrompt;

        // Checks that the & symbol is present and in the correct position.
        while (argsHead != NULL) {
            if (strcmp(argsHead->argument, "&") == 0 && argsHead->next == NULL) {
                if (!fgOnly) {
                    background = true;
                }
            }
            argsHead = argsHead->next;
        }   

        // Fork a new process
        pid_t spawnPid = fork();

        switch(spawnPid){
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:
        {   
            // Foreground child will terminate itself if SIGINT signal is recieved
            if (!background) {
                // Change signal handler to default settings
                SIGINT_action.sa_handler = SIG_DFL;
                // Reinstall signal handler
                sigaction(SIGINT, &SIGINT_action, NULL);
            }
            // In the child process
            // printf("CHILD(%d) running\n", getpid());

            listToArr(commandPrompt, args);
            
            for (int i=0; i < argCount; i++) {
                // printf("%s, ", args[i]);
                // stdin redirection
                if (strcmp(args[i], "<") == 0) {
                    sourceRedirect = true;

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
                    targetRedirect = true;

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

            // printf("NULL\n");
            // printf("argument run: %s\n", args[0]);
            // // Replace the current program with "/bin/ls"
            // execl("/bin/ls", "/bin/ls", "-al", NULL);
            if (sourceRedirect || targetRedirect) {
                for ( int i = 1; i < argCount; i++) {
                    args[i] = NULL;
                }
            }
             
            if (!sourceRedirect && background){
                sourceFD = open("/dev/null", O_RDONLY);
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
            if (!targetRedirect && background){
                targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

            if (background && strcmp(args[argCount-1], "&") == 0) {
                args[argCount-1] = NULL;
            }
            if (fgOnly && strcmp(args[argCount-1], "&") == 0) {
                args[argCount-1] = NULL;
            }

            execvp(args[0], args);
    
            // exec only returns if there is an error
            perror("execvp");
            childExitMethod = -1;
            exit(2);
            break;
        }
        default: 
        {
            if (background) {
                printf("background pid is %ld\n", (long) spawnPid);
                fflush(stdout);
                background = false;
            }
            else {
                spawnPid = waitpid(spawnPid, &childExitMethod, 0);
                
                // Check if child process was killed by a signal, prints signal number
                if (WIFSIGNALED(childExitMethod) && spawnPid > 0 ) {
                    printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                    fflush(stdout);

                }                

                // SpawnPid returns 0 while running,
                // It returns the pid once when completed,
                // It returns -1, if pid value has been read and process has been cleaned up
                // while (spawnPid != -1) {
                while (spawnPid > 0) {
                    spawnPid = waitpid(-1, &childExitMethod, WNOHANG);
                    if (WIFEXITED(childExitMethod) && spawnPid > 0) {
                        printf("background pid %d is done: exit value %d\n", spawnPid, WEXITSTATUS(childExitMethod));
                        fflush(stdout);

                    }
                    else if (WIFSIGNALED(childExitMethod) && spawnPid > 0 ) {
                        printf("background pid %d is done: terminated by signal %d\n", spawnPid, WTERMSIG(childExitMethod));
                        fflush(stdout);
                    }
                }
            }
        }
    }
        
}
    freeCommand(commandPrompt);
    fflush(stdout);
    exit(0);
    return EXIT_SUCCESS;
}