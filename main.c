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

// Foreground-only status, declared globally so it doesn't need to be passed around
bool fgOnly = false;

// Linked list to store the the tokenized command string
struct command 
{
    char *argument;
    struct command *next;
};

// Citation for the following function:
// Date: 01/20/2022
// Copied from /OR/ Adapted from /OR/ Based on:
// Source: https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c

// Expands an argument to insert the parent pid to replace any instance of "$$" in a command.
// Receives the command string to be expanded, the substring to be replaced, and the pid to be insterted.
void expandVar(char *source, char *substring, pid_t pid)
{
    // Create temp storage for substring 
    int length = snprintf(NULL, 0, "%d", pid);
    char *str = malloc(length + 1);
    
    // Convert pid int to string and store in temp
    snprintf(str, length + 1, "%d", pid);
    
    // Locates expansion starting point
    char *substring_source = strstr(source, substring);

    // Handles invalid strings
    if (substring_source == NULL) {
        return;
    }

    // Moves the characters after the "$$" substring over to make room for the new pid substring expansion
    // This will leave a hole in the string that will be the right size for the new substring
    memmove(
        substring_source + strlen(str),
        substring_source + strlen(substring),
        strlen(substring_source) - strlen(substring) + 1
    );
    
    // Copies pid value into string
    memcpy(substring_source, str, strlen(str));
    free(str);
}

// Creates an individual node for the command linked list
// Allocates space dynamically for each argument
// Expands any instance of "$$" to be replaced with the current pid
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
        expandVar(newArg, "$$", currentPid);
    }

    currCommand->argument = calloc(strlen(newArg) + 1, sizeof(char));
    strcpy(currCommand->argument, newArg);   
    currCommand->next = NULL;
    
    free(newArg);
    return currCommand;
}

// Tokenizes the userInput string to create a linked list.
// Breaks up string using spaces to separate commands.
// Returns the head of the LL when completed.
struct command *parseInput(char userInput[], pid_t currentPid)
{
    struct command *head = NULL;
    struct command *tail = NULL;

    char *arg = strtok(userInput, " ");

    while (arg != NULL) {
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

// Checks for blank input or comment line
// If either are present, returns false; else returns true.
bool checkValid(struct command *head)
{
    // Blank line
    if (head == NULL) {
        printf("\n");
        fflush(stdout);
        return false;
    }
    
    // Comment line
    char *firstArg = head->argument;
    if (firstArg[0] == '#') {
        printf("\n");
        fflush(stdout);
        return false;
    }
    return true;
}

// Built-in command: changes directory of parent process. 
// Takes at most one additional argument: the path to change to using absolute or relative paths.
// If no additional argument is provided then it will change to the default home directory.
// This argument cannot be run in the background.
void changeDirectory(struct command *current, char HOME[]) 
{
    // Checks for an argument, advances command
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

// Counts the number of arguments in a command,
// returns that number as an int.
int countArgs(struct command *head) 
{
    int count = 0;
    
    while (head != NULL) {
        count++;
        head = head->next;
    }
    return count;
}

// Parses linked list into arguments list
// This list is needed to execute the exec() family functions
// An empty list of the correct size is required.
void listToArr(struct command *current, char **list)
{   
    int i = 0;
    while (current != NULL) {
        list[i] = current->argument;
        i++;
        current = current->next;
    }
}

// Citation for the following function:
// Date: 02/05/2022
// Copied from /OR/ Adapted from /OR/ Based on:
// Source: https://canvas.oregonstate.edu/courses/1884946/pages/exploration-signal-handling-api?module_item_id=21835981

// Custom signal handler for SIGTSTP (Ctrl + z)
// Toggles foreground-only mode.
// When toggled on, no background commands can be run (& is ignored)
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

// This function is way to large and not structered well. I plan to properly organize it before showcasing it
// It works as it is right now, so I don't want to risk regression at the last minute. I'm honestly already super proud that I got it to this point, I wasn't sure if it would come together.

/* Citation for the following function:
    Date: 02/05/2022
    Copied from /OR/ Adapted from /OR/ Based on:
    source for redirect: https://canvas.oregonstate.edu/courses/1884946/pages/exploration-processes-and-i-slash-o?module_item_id=21835982
    source for signals: https://canvas.oregonstate.edu/courses/1884946/pages/exploration-signal-handling-api?module_item_id=21835981
*/
int main()
{
    char userInput[2048];
    struct command *commandPrompt;

    // os1 home
    char HOME[] = "/nfs/stak/users/bowdenn";
    // Parent shell pid
    pid_t pid = getpid();

    // Used for redirection
    int targetFD = -5;
    int sourceFD = -5;
    bool targetRedirect = false;
    bool sourceRedirect = false;

    // Process info
    bool background = false;
    int childExitMethod = 0;

    // Initialize both sigint and sigtstp structs to be empty
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // Ignore SIGINT
    SIGINT_action.sa_handler = SIG_IGN;
    // Block all catchable signals while running;
    sigfillset(&SIGINT_action.sa_mask);
    // No flags set
    SIGINT_action.sa_flags = 0;
    // Install signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);


    // Handle SIGSTP with new function
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    // Block all catchable signals while running;
    sigfillset(&SIGTSTP_action.sa_mask);
    // No flags set
    SIGTSTP_action.sa_flags = SA_RESTART;
    // Install signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while (1) {
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

        // BUILT-IN COMMAND: exit
        // Breaks loop where memory is freed and exit(0) is called
        // Cannot be run in the background
        if (strcmp(userInput, "exit") == 0 || strcmp(userInput, "exit &") == 0) {
            break;
        }

        // BUILT-IN COMMAND: status
        // Checks last exit method of child process, prints decoded value
        // Cannot be run in the background
        if (strcmp(userInput, "status") == 0 || strcmp(userInput, "status &") == 0) {
            if (WIFEXITED(childExitMethod)) {
                printf("exit value %d\n", WEXITSTATUS(childExitMethod));
                fflush(stdout);
            }
            else {
                printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                fflush(stdout);
            }
            continue;
        }

        // Parses the command and arguments into a linked list inside the command struct
        commandPrompt = parseInput(userInput, pid);
        bool valid = checkValid(commandPrompt);
        if (!valid) {continue;} // reprompts until valid command

        // After parsing, only the command is left inside userInput.
        // This is why "cd <some file path>" still triggers this condition
        if (strcmp(userInput, "cd") == 0) {
            changeDirectory(commandPrompt, HOME);
            printf("\n");
            fflush(stdout);
            continue;
        }
        // Initialize args array
        int argCount = countArgs(commandPrompt);
        char *args[argCount+1];
        args[argCount] = NULL;

        // temp pointer to check for background symbol
        struct command *argsHead = commandPrompt;

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
            listToArr(commandPrompt, args);
            
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
    freeCommand(commandPrompt);
    printf("\n");
    fflush(stdout);
    exit(0);
    return EXIT_SUCCESS;
}