/*
 * ShelluMama - UNIX Shell Clone
 * Features:
 * - Command execution
 * - I/O redirection (<, >, >>)
 * - Command piping (|)
 * - Multiple commands (;)
 * - Logical operators (&&)
 * - Command history
 * - Signal handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_PIPES 10
#define MAX_HISTORY 100
#define MAX_PATH_LENGTH 1024

// Command history structure
typedef struct {
    char commands[MAX_HISTORY][MAX_INPUT_SIZE];
    int count;
} History;

// Global variables for signal handling
History command_history = {{0}, 0};
pid_t current_child_pid = -1;

/** Author : @sudipta
 * Initializes the shell environment
 * - Sets up signal handlers for SIGINT (Ctrl+C) and SIGTERM
 * - Initializes command history
 * - Clears the screen and displays welcome message
 */
void initialize_shell();

/**Author : @sudipta
 * Displays the shell prompt with current working directory
 * - Shows current directory path in green color
 * - Falls back to simple "sh>" if directory path cannot be obtained
 */
void display_prompt();

/**Author : @sudipta
 * Reads user input from stdin
 * - Handles EOF (Ctrl+D) by exiting the shell
 * - Removes trailing newline character
 * - Handles input errors spectacular, amazingly
 */
void read_input(char *input);

/**Author : @shadman
 * Parses input string into multiple commands separated by semicolons
 * - Splits input by ';' character
 * - Returns array of command strings
 * - Returns number of commands found
 */
int parse_input(char *input, char *commands[]);

/**Author : @shadman
 * Executes a command line that may contain logical operators (&&)
 * - Splits command by '&&' operator
 * - Executes commands sequentially
 * - Stops execution if any command fails
 */
int execute_command_line(char *command_line);

/**Author : @shadman
 * Executes a command that may contain pipes (|)
 * - Splits command by '|' character
 * - Creates pipes between commands
 * - Manages file descriptors for pipe connections
 */
int execute_piped_commands(char *command);

/**Author : @sudipta
 * Executes a single command with optional I/O redirection
 * - Handles input/output file descriptors
 * - Manages I/O redirection
 * - Forks and executes the command
 */
int execute_single_command(char *command, int in_fd, int out_fd);

/**Author : @sudipta
 * Parses a command string into arguments and redirection information
 * - Splits command into tokens
 * - Identifies input/output redirection operators
 * - Handles append mode for output redirection
 */
void parse_command(char *command, char *args[], char **input_file, char **output_file, int *append_output);

/**Author : @maliha 
 * Handles shell signals (Ctrl+C)
 * - Terminates current child process if one exists
 * - Displays new prompt if no child process is running
 */
void handle_signal(int signo);

/**Author : @maliha
 * Adds a command to the history buffer
 * - Maintains a circular buffer of commands
 * - Shifts old commands when buffer is full
 */
void add_to_history(const char *command);

/**Author : @maliha
 * Displays the command history
 * - Shows numbered list of previous commands
 * - Displays up to MAX_HISTORY commands
 */
void show_history();

/**Author : @shadman
 * Checks if a command is a built-in shell command
 * - Handles 'exit' command
 * - Handles 'cd' command with home directory support
 * - Handles 'history' command
 */
int check_builtin_commands(char *args[]);

/**Author : @sudipta
 * Handles I/O redirection for a command
 * - Opens input file for reading if specified
 * - Opens output file for writing/append if specified
 * - Manages file descriptor duplication
 */
void handle_redirection(char *input_file, char *output_file, int append_output);

int main() {
    char input[MAX_INPUT_SIZE];
    
    initialize_shell();
    
    while (1) {
        display_prompt();
        read_input(input);
        
        if (strlen(input) == 0) {
            continue;
        }
        
        add_to_history(input);
        
        // Process multiple commands separated by ';'
        char *commands[MAX_ARGS] = {NULL};
        int cmd_count = parse_input(input, commands);
        
        for (int i = 0; i < cmd_count; i++) {
            if (commands[i] != NULL && strlen(commands[i]) > 0) {
                execute_command_line(commands[i]);
            }
        }
        
        // Free allocated memory for commands
        for (int i = 0; i < cmd_count; i++) {
            if (commands[i] != NULL) {
                free(commands[i]);
            }
        }
    }
    
    return 0;
}

void initialize_shell() {
    // Set up signal handlers
    signal(SIGINT, handle_signal);  // Handle Ctrl+C
    signal(SIGTERM, handle_signal);
    
    // Initialize command history
    command_history.count = 0;
    
    // Clear the screen
    printf("\033[H\033[J");
    printf("ShelluMama - UNIX Shell Clone\n");
    printf("Type 'exit' to quit the shell\n\n");
}

void display_prompt() {
    char cwd[MAX_PATH_LENGTH];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("\033[1;32msh:%s\033[0m> ", cwd);
    } else {
        printf("sh> ");
    }
    fflush(stdout);
}

void read_input(char *input) {
    if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
        if (feof(stdin)) {
            printf("exit\n");
            exit(EXIT_SUCCESS);
        } else {
            perror("fgets error");
            exit(EXIT_FAILURE);
        }
    }
    
    // Remove trailing newline
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }
}

int parse_input(char *input, char *commands[]) {
    int i = 0;
    char *token = strtok(input, ";");
    
    while (token != NULL && i < MAX_ARGS - 1) {
        commands[i] = strdup(token);
        i++;
        token = strtok(NULL, ";");
    }
    
    commands[i] = NULL;
    return i;
}

int execute_command_line(char *command_line) {
    // Check for logical operator &&
    char *commands[MAX_ARGS] = {NULL};
    int cmd_count = 0;
    
    char *token = strtok(command_line, "&&");
    while (token != NULL && cmd_count < MAX_ARGS - 1) {
        commands[cmd_count] = token;
        cmd_count++;
        token = strtok(NULL, "&&");
    }
    
    int success = 1;
    for (int i = 0; i < cmd_count; i++) {
        // Only execute next command if previous was successful
        if (success) {
            int status = execute_piped_commands(commands[i]);
            if (status != 0) {
                success = 0;
            }
        } else {
            break;
        }
    }
    
    return success ? 0 : 1;
}

int execute_piped_commands(char *command) {
    char *commands[MAX_PIPES + 1];
    int cmd_count = 0;
    
    // Trim leading spaces
    while (*command == ' ') {
        command++;
    }
    
    // Parse pipe commands
    char *token = strtok(command, "|");
    while (token != NULL && cmd_count < MAX_PIPES) {
        commands[cmd_count] = token;
        cmd_count++;
        token = strtok(NULL, "|");
    }
    commands[cmd_count] = NULL;
    
    if (cmd_count == 0) {
        return 0;
    }
    
    if (cmd_count == 1) {
        // No pipes, just a single command
        return execute_single_command(commands[0], STDIN_FILENO, STDOUT_FILENO);
    }
    
    int pipes[MAX_PIPES][2];
    
    // Create all the needed pipes
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("Pipe creation failed");
            return 1;
        }
    }
    
    int status = 0;
    
    // Execute commands with pipes
    for (int i = 0; i < cmd_count; i++) {
        int in_fd = (i == 0) ? STDIN_FILENO : pipes[i-1][0];
        int out_fd = (i == cmd_count - 1) ? STDOUT_FILENO : pipes[i][1];
        
        int cmd_status = execute_single_command(commands[i], in_fd, out_fd);
        
        // Close used pipe ends
        if (i > 0) {
            close(pipes[i-1][0]);
        }
        if (i < cmd_count - 1) {
            close(pipes[i][1]);
        }
        
        if (cmd_status != 0) {
            status = cmd_status;
        }
    }
    
    return status;
}

int execute_single_command(char *command, int in_fd, int out_fd) {
    char *args[MAX_ARGS];
    char *input_file = NULL;
    char *output_file = NULL;
    int append_output = 0;
    
    parse_command(command, args, &input_file, &output_file, &append_output);
    
    // Check if the command is empty
    if (args[0] == NULL) {
        return 0;
    }
    
    // Check for built-in commands
    if (check_builtin_commands(args) == 0) {
        return 0;
    }
    
    // Fork and execute
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        
        // Handle redirections
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        
        // Handle I/O redirection
        handle_redirection(input_file, output_file, append_output);
        
        // Execute the command
        execvp(args[0], args);
        
        // If execvp returns, there was an error
        fprintf(stderr, "Error: Command '%s' not found\n", args[0]);
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        current_child_pid = pid;
        int status;
        
        // Close pipe file descriptors that are not standard I/O
        if (in_fd != STDIN_FILENO) {
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            close(out_fd);
        }
        
        waitpid(pid, &status, 0);
        current_child_pid = -1;
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return 1;
        }
    }
}

void parse_command(char *command, char *args[], char **input_file, char **output_file, int *append_output) {
    int i = 0;
    char *token;
    *input_file = NULL;
    *output_file = NULL;
    *append_output = 0;
    
    // Parse the command into arguments
    token = strtok(command, " \t");
    while (token != NULL && i < MAX_ARGS - 1) {
        // Check for input redirection
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t");
            if (token != NULL) {
                *input_file = token;
            }
        }
        // Check for output redirection with append
        else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " \t");
            if (token != NULL) {
                *output_file = token;
                *append_output = 1;
            }
        }
        // Check for output redirection
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t");
            if (token != NULL) {
                *output_file = token;
                *append_output = 0;
            }
        }
        // Regular argument
        else {
            args[i] = token;
            i++;
        }
        
        token = strtok(NULL, " \t");
    }
    
    args[i] = NULL;
}

void handle_redirection(char *input_file, char *output_file, int append_output) {
    // Handle input redirection
    if (input_file != NULL) {
        int fd = open(input_file, O_RDONLY);
        if (fd < 0) {
            perror("Input redirection error");
            exit(EXIT_FAILURE);
        }
        
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 error for input");
            exit(EXIT_FAILURE);
        }
        
        close(fd);
    }
    
    // Handle output redirection
    if (output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        
        if (append_output) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        
        int fd = open(output_file, flags, 0644);
        if (fd < 0) {
            perror("Output redirection error");
            exit(EXIT_FAILURE);
        }
        
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 error for output");
            exit(EXIT_FAILURE);
        }
        
        close(fd);
    }
}

int check_builtin_commands(char *args[]) {
    if (args[0] == NULL) {
        return 0;
    }
    
    // Exit command
    if (strcmp(args[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }
    
    // CD command
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            // Change to home directory
            const char *home_dir = getenv("HOME");
            if (home_dir == NULL) {
                fprintf(stderr, "Error: HOME environment variable not set\n");
                return 0;
            }
            
            if (chdir(home_dir) != 0) {
                perror("cd error");
            }
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd error");
            }
        }
        return 0;
    }
    
    // History command
    if (strcmp(args[0], "history") == 0) {
        show_history();
        return 0;
    }
    
    // Not a built-in command
    return 1;
}

void add_to_history(const char *command) {
    if (command_history.count < MAX_HISTORY) {
        strcpy(command_history.commands[command_history.count], command);
        command_history.count++;
    } else {
        // Shift all commands one position up
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(command_history.commands[i], command_history.commands[i + 1]);
        }
        strcpy(command_history.commands[MAX_HISTORY - 1], command);
    }
}

void show_history() {
    for (int i = 0; i < command_history.count; i++) {
        printf("%d: %s\n", i + 1, command_history.commands[i]);
    }
}

void handle_signal(int signo) {
    if (signo == SIGINT) {
        if (current_child_pid != -1) {
            // Kill the current child process
            kill(current_child_pid, SIGINT);
        } else {
            // Display a new prompt
            printf("\n");
            display_prompt();
            fflush(stdout);
        }
    }
}


