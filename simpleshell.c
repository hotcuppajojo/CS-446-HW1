#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAXINPUT 1024
#define MAXCWD 1024
#define MAXARGS 64

int changeDirectories(char** args);
int executeCommand(char** args);
char** parseInput(char* input);
void readInput(char *buffer);
    
int main() {
    char input[MAXINPUT];
    char** args;
    char cwd[MAXCWD];

    while (1) {
        // Print the prompt
        getcwd(cwd, sizeof(cwd));
        char *username = getenv("USER");
        if (!username) {
            username = getenv("LOGNAME");
        }
        printf("%s:%s$ ", username ? username : "<unknown>", cwd);
        fflush(stdout);
        readInput(input); // Read input
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        // Parse the input
        args = parseInput(input);

        // Check command type and execute
        if (strcmp(args[0], "exit") == 0) {
            free(args);
            exit(0);
        } else if (strcmp(args[0], "cd") == 0) {
            changeDirectories(args);
        } else {
            executeCommand(args);
        }

        free(args);
    }

    return 0;
}

int changeDirectories(char** args) {
    if (args[1] == NULL) {
        // If 'cd' is entered without arguments, display an error message
        fprintf(stderr, "chdir Failed: Path Not Formatted Correctly!\n");
        return -1; // Return -1 on error
    } else if (args[2] != NULL) {
        // If 'cd' is entered with too many arguments, display an error message
        fprintf(stderr, "Error: 'cd' command accepts only one directory argument.\n");
        return -1; // Return -1 on error
    } else {
        if (chdir(args[1]) != 0) {
            // Display an error message and the reason for failure
            fprintf(stderr, "chdir Failed: %s\n", strerror(errno));
            return -1; // Return -1 on error
        }
    }
    return 0; // Return 0 on success
}

int executeCommand(char** args) {
    pid_t pid, wpid;
    int status;

    if ((pid = fork()) == -1) {
        // Error forking
        perror("fork Failed");
        return -1; // Return -1 on error
    }

    if (pid == 0) {
        // Child process
        int dev_null_fd = open("/dev/null", O_RDONLY);
        if (dev_null_fd == -1) {
            perror("open /dev/null Failed");
            _exit(EXIT_FAILURE);
        }

        dup2(dev_null_fd, STDIN_FILENO);  // Redirect stdin to /dev/null
        close(dev_null_fd);

        fflush(stdout);  // Clear the stdout buffer
        if (execvp(args[0], args) == -1) {
            // Error executing the command
            perror("exec Failed");
            _exit(EXIT_FAILURE); // Terminate child process immediately
        }
    } else {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    // Return 0 on success
    return 0;
}

char** parseInput(char* input) {
    char** args = malloc((MAXARGS + 1) * sizeof(char*)); // +1 for the terminating NULL pointer
    if (args == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE); // Exit if memory allocation fails
    }

    int arg_count = 0;
    char* token;

    // Use strtok to split the input into tokens based on whitespace
    token = strtok(input, " \t\n");

    while (token != NULL) {
        args[arg_count] = strdup(token);
        if (args[arg_count] == NULL) {
            perror("strdup");
            exit(EXIT_FAILURE); // Exit if memory allocation fails
        }
        arg_count++;
        token = strtok(NULL, " \t\n");
    }

    args[arg_count] = NULL; // Terminating NULL pointer for execvp compatibility
    return args;
}

void readInput(char *buffer) {
    char c;
    int index = 0;
    int cursor_pos = 0;
    
    fflush(stdout);

    // Disable echoing temporarily
    system("stty -icanon -echo");

    while (1) {
        if (read(STDIN_FILENO, &c, 1) <= 0) {
            // Enable echoing and canonical mode on error
            system("stty icanon echo");
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Handle escape sequences (like arrow keys)
        if (c == 27) {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) && read(STDIN_FILENO, &seq[1], 1)) {
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'C': // Right arrow
                            if (cursor_pos < index) {
                                cursor_pos++;
                                write(STDOUT_FILENO, "\033[C", 3);
                            }
                            continue;
                        case 'D': // Left arrow
                            if (cursor_pos > 0) {
                                cursor_pos--;
                                write(STDOUT_FILENO, "\033[D", 3);
                            }
                            continue;
                    }
                }
            }
        }

        // Handle backspace (ASCII value 127)
        if (c == 127) {
            if (cursor_pos > 0) {
                for (int i = cursor_pos; i < index; i++) {
                    buffer[i - 1] = buffer[i];
                }
                index--;
                cursor_pos--;

                buffer[index] = '\0'; // Null-terminate

                write(STDOUT_FILENO, "\033[D\033[K", 5); // Move cursor left and clear to end of line
                write(STDOUT_FILENO, buffer + cursor_pos, index - cursor_pos); // Write shifted buffer
                for (int i = cursor_pos; i < index; i++) {
                    write(STDOUT_FILENO, "\033[D", 3); // Move cursor back to original position
                }
            }
            continue;
        }

        // Handle Enter key
        if (c == '\n') {
            buffer[index] = '\0';
            write(STDOUT_FILENO, &c, 1);
            break;
        }

        // Insert characters if cursor is moved
        for (int i = index; i > cursor_pos; i--) {
            buffer[i] = buffer[i - 1];
        }
        buffer[cursor_pos] = c;

        write(STDOUT_FILENO, "\033[K", 3); // Clear to end of line
        write(STDOUT_FILENO, buffer + cursor_pos, index - cursor_pos + 1); // Write buffer from cursor_pos
        cursor_pos++;
        index++;

        for (int i = cursor_pos; i < index; i++) {
            write(STDOUT_FILENO, "\033[D", 3); // Move cursor back to original position
        }
    }

    // Enable echoing and canonical mode after reading input
    system("stty icanon echo");
}
