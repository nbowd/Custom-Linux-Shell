# Custom Linux Shell

Custom linux shell that implements a subset of features of well-known shells, such as bash.

The features include:
* Provides a prompt for running commands
* Handles blank lines and comments
* Provides expansion for the variable ```$$```
* Executes 3 built-in shell commands: exit, cd, and status 
* Execute other commands by creating new processes using a function from the exec family of functions
* Support input and output redirection
* Support running commands in foreground and background processes
* Implement custom handlers for 2 signals, SIGINT and SIGTSTP

Command prompt
----
The colon ```:``` symbol as a prompt for each command line  

The general syntax of a command line is:  
```command [arg1 arg2 ...] [< input_file] [> output_file] [&]```  
where items in square brackets are optional.

Comments and blank lines
----
Blank lines and commands that begin with ```#``` will be ignored and given a reprompt.

Expansion of variable $$
----
Expands any instance of ```$$``` into the process ID of the shell

Built in commands: exit, cd, status
----
**exit** - Takes no arguments and kills any other processes before terminating itself  
**cd** - Takes one optional argument. Changes the working directory of the shell to the argument location. If no argument is passed, returns to home directory.  
**status** - Takes no arguments. Prints out the exit status or terminating signal for the last foreground process run by the shell.  

Executing other commands
----
Whenever a non-built in command is received, the shell will fork off a child process to run the command.

Input and output redirection
----
Uses standard syntax, both can be used in a command.  
Input syntax: ```[< input_file]```   
Output syntax: ```[> output_file]```

Foreground and background commands
----
**Foreground command** -
Any command without an ```&``` at the end will be run as a foreground command and the shell will wait for the completion of the command before prompting for the next command.  

**Background command** -
Any non built-in command with an ```&``` at the end are run as a background command and the shell will not wait for such a command to complete. 

Termination Signals
----
A CTRL-C command from the keyboard sends a SIGINT signal to the parent process and all children at the same time. This command will terminate all children running foreground processes.

A CTRL-Z command from the keyboard sends a SIGTSTP signal to your parent shell process and all children at the same time. This enters 'Foreground only' mode and will begin ignoring ```&``` until SIGSTP is received again, or the process is terminated.

Running this program
----
Download the files and navigate to the directory.  
Build the program using the ```make``` command.  
Run the program using the ```./smallsh``` command.  
Clean up the program using the ```clean``` command.
