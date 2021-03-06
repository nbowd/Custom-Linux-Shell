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
#include <sys/wait.h> 
#include <signal.h>

// Foreground-only status
bool fgOnly = false;

// Linked list to store the the tokenized command string
struct command 
{
    char *argument;
    struct command *next;
};

/* 
    Expands an argument to insert the parent pid to replace any instance of "$$" in a command.
    Receives the command string to be expanded and the pid to be insterted. 
*/
void expandVar(char *source, pid_t pid)
{
    // Create temp storage for substring 
    int length = snprintf(NULL, 0, "%d", pid);
    char *str = malloc(length + 1);
    
    // Convert pid int to string and store in temp
    snprintf(str, length + 1, "%d", pid);
    
    // Locates expansion starting point
    char *substring_source = strstr(source, "$$");

    // Handles invalid strings
    if (substring_source == NULL) {
        return;
    }

    // Moves the characters after the "$$" substring over to make room for the new pid substring expansion
    // This will leave a hole in the string that will be the right size for the new substring
    memmove(
        substring_source + strlen(str),
        substring_source + 2,
        strlen(substring_source) - 1
    );
    
    // Copies pid value into string
    memcpy(substring_source, str, strlen(str));
    free(str);
}

/*
    Creates an individual node for the command linked list
    Allocates space dynamically for each argument
    Checks and expands any instance of "$$" to be replaced with the current pid
*/
struct command *createCommand(char *arg, pid_t currentPid)
{
    struct command *currCommand = malloc(sizeof(struct command));

    // Creates a new string allocation that is large enough to contain the expansion if needed
    int pidLength = snprintf(NULL, 0, "%d", currentPid);
    char *newArg = calloc(strlen(arg) + pidLength - 1, sizeof(char));
    strcpy(newArg, arg);    
    
    // Checks for expansion shortcut
    // Modifies newArg to reflect the expanded argument
    if (strstr(arg, "$$") != NULL) {
        expandVar(newArg, currentPid);
    }

    currCommand->argument = calloc(strlen(newArg) + 1, sizeof(char));
    strcpy(currCommand->argument, newArg);   
    currCommand->next = NULL;
    
    free(newArg);
    return currCommand;
}

/* 
    Tokenizes the userInput string to create a linked list.
    Breaks up string using spaces to separate commands.
    Returns the head of the LL when completed.
*/
struct command *parseInput(char userInput[], pid_t currentPid)
{
    struct command *head = NULL;
    struct command *tail = NULL;

    // token separator
    char *arg = strtok(userInput, " ");

    while (arg != NULL) {
        struct command *newArg = createCommand(arg, currentPid);
        
        // Empty list, add first node
        if (head == NULL) 
        {
            head = newArg;
            tail = newArg;
        } 
        else // the subsequent nodes
        {
            tail->next = newArg;
            tail = newArg;
        }
        arg = strtok(NULL, " ");
    }
    return head;
}

// Used for debugging, helpful to vefiry user input is being gathered correctly. 
void printArgs(struct command *current) 
{
    while(current!= NULL) {
        printf("%s\n", current->argument);
        fflush(stdout);
        current = current->next;
    }
}

// Frees up the dynamically allocated memory used in the command linked list.
void freeCommand(struct command *head)
{
    while (head != NULL) {
        free(head->argument);
        free(head);
        head = head->next;
    }
}

/* 
    Checks for blank input or comment line
    If either are present, returns false; else returns true.
*/
bool checkValid(struct command *head)
{
    // Blank line
    if (head == NULL) {
        return false;
    }
    
    // Comment line
    char *firstArg = head->argument;
    if (firstArg[0] == '#') {
        return false;
    }
    return true;
}

/* 
    Built-in command: changes directory of parent process. 
    Takes at most one additional argument: the path to change to using absolute or relative paths.
    If no additional argument is provided then it will change to the default home directory.
    This argument cannot be run in the background.
*/
void changeDirectory(struct command *current) 
{
    // Checks for an argument, advances command
    current = current->next;

    // No argument, navigates to HOME directory
    if (current == NULL) {
        chdir(getenv("HOME"));
        return;
    }

    // Displays error if argument is an invalid directory, 
    // Changes directories on valid argument.
    if (chdir(current->argument) != 0) {
        perror("Invalid argument, cd not executed");
    }
}
/*
    Counts the number of arguments in a command,
    returns that number as an int.
*/
int countArgs(struct command *head) 
{
    int count = 0;
    
    while (head != NULL) {
        count++;
        head = head->next;
    }
    return count;
}

/*
    Parses linked list into arguments list
    This list is needed to execute the exec() family functions
    An empty list of the correct size is required.
*/
void listToArr(struct command *current, char **list)
{   
    int i = 0;
    while (current != NULL) {
        list[i] = current->argument;
        i++;
        current = current->next;
    }
}
/*
    Custom signal handler for SIGTSTP (Ctrl + z)
    Toggles foreground-only mode.
    When toggled on, no background commands can be run (& is ignored)
*/
void handle_SIGTSTP(int signo){
    if (fgOnly) {
        char *message = "Exiting foreground-only mode\n: ";
        fgOnly = false;
        write(STDOUT_FILENO, message, 32);
    }
    else
    {
	    char* message = "Entering foreground-only mode (& is now ignored)\n: ";
	    fgOnly = true;
	    write(STDOUT_FILENO, message, 52);
    }

}

/*
    Prompts user with ": " and saves the retrieved input to the userInput array passed in.
    Clears value currently stored in userInput before copying over value
*/
void getCommand(char userInput[])
{
    // Retrieve user input by prompting ": "
    char *buf = NULL;
    size_t buflen;
    printf(": ");
    fflush(stdout);
    getline(&buf, &buflen, stdin);

    buf[strlen(buf)-1] = '\0'; // trims \n from input

    // transfer value of cleaned up buffer to correct location
    memset(userInput, '\0', 2048);
    strcpy(userInput, buf);
    free(buf);    
}

/*
    BUILT-IN COMMAND: exit
    On exit command, breaks input loop and returns true
    else returns false
    Cannot be run in the background.
*/ 
bool handleExit(char userInput[])
{
    if (strcmp(userInput, "exit") == 0 || strcmp(userInput, "exit &") == 0) {
        return true;
    }
    return false;  // else
}

/*
    BUILT-IN COMMAND: status
    Checks last exit method of child process, prints decoded value and returns true
    returns false for all other commands
    Cannot be run in the background
*/ 
bool handleStatus(char userInput[], int childExitMethod)
{
    if (strcmp(userInput, "status") == 0 || strcmp(userInput, "status &") == 0) {
        if (WIFEXITED(childExitMethod)) {
            printf("exit value %d\n", WEXITSTATUS(childExitMethod));
            fflush(stdout);
        }
        else {
            printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
            fflush(stdout);
        }
        return true;
    }
    return false;    
}

/*
    BUILT-IN COMMAND: cd
    Changes directory of the current process, 
    accepts up to 1 additional argument: the target directory (relative or absolute path)
    If no additional argument, changes to HOME directory by default.
    Returns true on command execution, false otherwise.
*/ 
bool handleCD(char userInput[], struct command *parsedCommand) 
{
    // After parsing, only the command is left inside userInput.
    // This is why "cd <some file path>" still triggers this condition
    if (strcmp(userInput, "cd") == 0) {
        changeDirectory(parsedCommand);
        return true;
    }
    return false;
}

/*
    Checks each user input for the symbol ("&") to indicate that the current command should be run in the background.
    Background symbol must be the last argument in the command input to be valid.
    Sets background variable to true for valid background symbol placement.
*/ 
void checkBgCommand(struct command *parsedCommand, bool background) 
{
    // temp pointer to check for background symbol
    struct command *argsHead = parsedCommand;

    // Checks that the & symbol is present and in the correct position.
    while (argsHead != NULL) {
        if (strcmp(argsHead->argument, "&") == 0 && argsHead->next == NULL) {
            // Does not set background flag is fgOnly is active
            if (!fgOnly) {
                background = true;
            }
        }
        argsHead = argsHead->next;
    }
            
}

int main()
{
    char userInput[2048];
    struct command *parsedCommand;

    // Parent shell pid
    pid_t pid = getpid();

    // Redirection:
    // Status
    int targetFD = -5;
    int sourceFD = -5;
    // Indicator
    bool targetRedirect = false;
    bool sourceRedirect = false;

    // Process info
    bool background = false;
    int childExitMethod = 0;

    // Initialize both sigint and sigtstp structs to be empty
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // Ignore SIGINT globally, will restore for foreground mode.
    SIGINT_action.sa_handler = SIG_IGN;
    // Block all catchable signals while running;
    sigfillset(&SIGINT_action.sa_mask);
    // No flags set
    SIGINT_action.sa_flags = 0;
    // Install signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Handle SIGSTP with custom function
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    // Block all catchable signals while running
    sigfillset(&SIGTSTP_action.sa_mask);
    // No flags set
    SIGTSTP_action.sa_flags = SA_RESTART;
    // Install signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while (1) {

        getCommand(userInput);

        // Parses the command and arguments into a linked list inside the command struct
        parsedCommand = parseInput(userInput, pid);

        // Verifies valid input, will not progress until so
        bool valid = checkValid(parsedCommand);
        if (!valid) {continue;} // reprompts until valid command

        if (handleExit(userInput)) {
            break;
        }

        if (handleStatus(userInput, childExitMethod)) {
            continue;
        }

        if (handleCD(userInput, parsedCommand)) {
            continue;
        }
        // Initialize args array
        int argCount = countArgs(parsedCommand);
        char *args[argCount+1];
        args[argCount] = NULL;

        checkBgCommand(parsedCommand, background);

        // Fork a new process
        pid_t spawnPid = fork();

        // Handling for Parent and Child process, as well as error handling.
        switch(spawnPid){
        // Failed fork
        case -1:
            perror("failed fork()\n");
            exit(1);
            break;
        // Child Processes
        case 0:
        {   
            // Foreground child will terminate itself if SIGINT signal is recieved
            if (!background) {
                // Change signal handler to default settings
                SIGINT_action.sa_handler = SIG_DFL;
                // Reinstall signal handler
                sigaction(SIGINT, &SIGINT_action, NULL);
            }

            // Convert LL to arr for exec()
            listToArr(parsedCommand, args);
            
            // Checks for input/output redirection
            for (int i=0; i < argCount; i++) {
                // stdin redirection
                if (strcmp(args[i], "<") == 0) {
                    sourceRedirect = true;

                    sourceFD = open(args[i+1], O_RDONLY);
                    // File opening error
                    if (sourceFD == -1) {
                        perror("source open()");
                        exit(1);
                    }

                    int result = dup2(sourceFD, 0); // 0 == stdin
                    // Redirection error
                    if (result == -1) {
                        perror("source dup2()");
                        exit(2);
                    } 
                }

                // stdout redirection
                if (strcmp(args[i], ">") == 0) {
                    targetRedirect = true;

                    targetFD = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    // File opening error
                    if (targetFD == -1) {
                        perror("source open()");
                        exit(1);
                    }

                    int result = dup2(targetFD, 1); // 1 == stdout
                    // Redirection error
                    if (result == -1) {
                        perror("source dup2()");
                        exit(2);
                    } 
                }
            }

            // If redirection occurred, NULL all arguments except for the command
            if (sourceRedirect || targetRedirect) {
                for ( int i = 1; i < argCount; i++) {
                    args[i] = NULL;
                }
            }
             
            // Undirected inputs and/or outputs of background processes
            // get directed towards /dev/null
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

            // Removes & from background arguments so it is not included in execution
            if (background && strcmp(args[argCount-1], "&") == 0) {
                args[argCount-1] = NULL;
            }
            // Removes & from arguments run in foreground-only mode 
            // so it is not included in execution
            if (fgOnly && strcmp(args[argCount-1], "&") == 0) {
                args[argCount-1] = NULL;
            }

            execvp(args[0], args);
    
            // exec only returns if there is an error
            perror("execution error");
            childExitMethod = -1;
            exit(2);
            break;
        }
        // Parent Process
        default: 
        {
            // Background process does not waitpid() and returns control to user
            if (background) {
                printf("background pid is %ld\n", (long) spawnPid);
                fflush(stdout);
                background = false;
            }
            else {
                // Foreground process will wait until completion (or signal termination)
                //  before it returns control to user
                spawnPid = waitpid(spawnPid, &childExitMethod, 0);
                
                // Check if child process was killed by a signal, prints message with signal number if so.
                if (WIFSIGNALED(childExitMethod) && spawnPid > 0 ) {
                    printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                    fflush(stdout);
                }                

                // Cleans up finished processes before returning control to user.
                // SpawnPid returns 0 while running,
                // It returns the pid (only one time) when completed,
                // It returns -1, if pid value has been read and process has been cleaned up
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
    freeCommand(parsedCommand);
    fflush(stdout);
    return EXIT_SUCCESS;

}