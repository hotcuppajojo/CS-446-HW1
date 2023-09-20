/** Homework 1 - CS 446/646
    File: simpleshell.c
    Author: JoJo Petersky
*/
// Include necessary header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
// Define max sizes for user input, current working directory, and arguments
#define MAXINPUT 1024
#define MAXCWD 1024
#define MAXARGS 64
// Define ASCII Values
#define ESCAPESEQ 27
#define BACKSPACE 127
// Function prototypes
int changeDirectories(char** args);
int executeCommand(char** args);
char** parseInput(char* input);
void readInput(char *buffer);
// Initiates the shell, reads user input, and executes appropriate commands
int main() {
    // Define local variables for the input buffer, parsed arguments, and current working directory
    char input[MAXINPUT];
    char** args;
    char cwd[MAXCWD];
    // Main loop that keeps running for the shell prompt
    while (1) {
        getcwd(cwd, sizeof(cwd)); // Get the current working directory
        char *username = getenv("USER"); // Attempt to get the username from environment variable "USER"
        if (!username) {
            username = getenv("LOGNAME"); // Fall back to "LOGNAME" if "USER" is not available
        }
        // Print the command prompt with the username and the current directory
        printf("%s:%s$ ", username ? username : "<unknown>", cwd);
        fflush(stdout); // Ensure the prompt is displayed immediately
        readInput(input); // Read user's input from the command line
        size_t len = strlen(input); // Remove trailing newline character if present
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        // Parse the user's input into arguments and allocate memory for them
        args = parseInput(input);
        // Determine the command to execute (e.g., cd, exit, or any external command)
        if (strcmp(args[0], "exit") == 0) {
            free(args);
            exit(0); // Exit the program
        } else if (strcmp(args[0], "cd") == 0) {
            changeDirectories(args); // Change the directory
        } else {
            executeCommand(args); // Execute an external command
        }
        free(args); // Free the memory allocated for the arguments after use
    }
    return 0;
}
// Function to handle 'cd' command
int changeDirectories(char** args) {
    if (args[1] == NULL) { // Check if no argument is provided to 'cd'
        // If 'cd' is entered without arguments, display an error message
        fprintf(stderr, "chdir Failed: Path Not Formatted Correctly!\n");
        return -1; // Return -1 on error
    } else if (args[2] != NULL) { // Check if more than one argument is provided to 'cd'
        // If 'cd' is entered with too many arguments, display an error message
        fprintf(stderr, "Error: 'cd' command accepts only one directory argument.\n");
        return -1; // Return -1 on error
    } else { // Change to the specified directory
        if (chdir(args[1]) != 0) {
            // Display an error message and the reason for failure
            fprintf(stderr, "chdir Failed: %s\n", strerror(errno));
            return -1; // Return -1 on error
        }
    }
    return 0; // Return 0 on success
}
// Function to execute external commands
int executeCommand(char** args) {
    pid_t pid, wpid;
    int status;
    // Create a child process
    if ((pid = fork()) == -1) {
        // If there's an error creating a child process, display an error and return
        perror("fork Failed");
        return -1; // Return -1 on error
    }
    // Code executed by child process
    if (pid == 0) {
        // Redirect stdin to /dev/null to prevent reading input from terminal
        int dev_null_fd = open("/dev/null", O_RDONLY);
        if (dev_null_fd == -1) {
            perror("open /dev/null Failed");
            _exit(EXIT_FAILURE);
        }
        // Execute the external command
        dup2(dev_null_fd, STDIN_FILENO);  // Redirect stdin to /dev/null
        close(dev_null_fd);
        fflush(stdout);  // Clear the stdout buffer
        // Execute the external command
        if (execvp(args[0], args) == -1) {
            perror("exec Failed"); // Error executing the command
            _exit(EXIT_FAILURE); // Terminate child process immediately
        }
    } else {
        // Parent process waits for child process to finish
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 0; // Return 0 on success
}
// Function to split user's input into an array of arguments
char** parseInput(char* input) {
    // Allocate memory for arguments
    char** args = malloc((MAXARGS + 1) * sizeof(char*)); // +1 for the terminating NULL pointer
    // Check if memory allocation was successful.\
    if (args == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE); // Exit if memory allocation fails
    }
    int arg_count = 0; // Counter for number of arguments parsed.
    char* token; // Pointer to store each tokenized string.
    token = strtok(input, " \t\n"); // Split the input into tokens based on whitespace
    // Continue tokenizing until no more tokens are found
    while (token != NULL) {
        args[arg_count] = strdup(token); // Duplicate the token string
        // Check if memory allocation for duplicated string was successful
        if (args[arg_count] == NULL) {
            perror("strdup"); // Error if memory allocation fails
            exit(EXIT_FAILURE); // Exit if memory allocation fails
        }
        arg_count++; // Increase argument counter.
        token = strtok(NULL, " \t\n"); // Get the next token from the string
    }
    args[arg_count] = NULL; // Terminating NULL pointer for execvp compatibility
    return args; // Return the array of argument pointers
}
// Function to read user input with interactive features like arrow key movement and backspacing
void readInput(char *buffer) {
    char c; // Store each character as read from stdin
    int index = 0; // Index to track current position in buffer
    int cursor_pos = 0; // Index to track current cursor position
    fflush(stdout); // Ensure any pending stdout data is flushed
    system("stty -icanon -echo"); // Temporarily disable canonical mode and echo for custom input reading
    // Reads characters from the user until an 'Enter' key is pressed
    while (1) {
        if (read(STDIN_FILENO, &c, 1) <= 0) { // Read a single character from stdin into 'c'
            // Enable echoing and canonical mode on error
            system("stty icanon echo"); // Restore terminal settings
            perror("read"); // Print read error details
            exit(EXIT_FAILURE); // Exit due to read error
        }
        // Handle escape sequences (ASCII value 27)
        if (c == ESCAPESEQ) {
            char seq[3];
            // Read the next two characters of the escape sequence
            if (read(STDIN_FILENO, &seq[0], 1) && read(STDIN_FILENO, &seq[1], 1)) {
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'C': // Right arrow detected
                            if (cursor_pos < index) {
                                cursor_pos++; // Move cursor position to the right
                                write(STDOUT_FILENO, "\033[C", 3); // Terminal command to move cursor right
                            }
                            continue; // Go to the next iteration without processing rest of loop
                        case 'D': // Left arrow detected
                            if (cursor_pos > 0) {
                                cursor_pos--; // Move cursor position to the left
                                write(STDOUT_FILENO, "\033[D", 3); // Terminal command to move cursor left
                            }
                            continue; // Go to the next iteration without processing rest of loop
                    }
                }
            }
        }
        // Handle backspace (ASCII value 127)
        if (c == BACKSPACE) {
            if (cursor_pos > 0) {
                // Shift buffer characters left from cursor position
                for (int i = cursor_pos; i < index; i++) {
                    buffer[i - 1] = buffer[i];
                }
                index--; // Reduce buffer length
                cursor_pos--; // Move cursor to the left
                buffer[index] = '\0'; // Ensure buffer is null-terminated
                write(STDOUT_FILENO, "\033[D\033[K", 5); // Move cursor left and clear to end of line
                write(STDOUT_FILENO, buffer + cursor_pos, index - cursor_pos); // Write shifted buffer
                for (int i = cursor_pos; i < index; i++) {
                    write(STDOUT_FILENO, "\033[D", 3); // Move cursor back to original position
                }
            }
            continue; // Go to the next iteration without processing rest of loop
        }
        // If 'Enter' key is detected, finalize the buffer and exit loop
        if (c == '\n') {
            buffer[index] = '\0'; // Null-terminate the buffer
            write(STDOUT_FILENO, &c, 1); // Echo the newline character
            break; // Exit loop
        }
        // If cursor is moved, adjust the buffer to insert characters
        for (int i = index; i > cursor_pos; i--) {
            buffer[i] = buffer[i - 1];
        }
        buffer[cursor_pos] = c; // Insert the character at cursor position
        write(STDOUT_FILENO, "\033[K", 3); // Terminal command to clear to end of line from cursor
        write(STDOUT_FILENO, buffer + cursor_pos, index - cursor_pos + 1); // Write the rest of the buffer from cursor position
        cursor_pos++; // Move cursor position right
        index++; // Increase buffer length
        // Move cursor back to the position after the inserted character
        for (int i = cursor_pos; i < index; i++) {
            write(STDOUT_FILENO, "\033[D", 3);
        }
    }
    // Enable echoing and canonical mode after reading input
    system("stty icanon echo");
}
