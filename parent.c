#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE_LENGTH 1024

void write_string(int fd, const char *str) {
    write(fd, str, strlen(str));
}

int main() {
    int pipe1[2];
    int pipe2[2];
    
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        const char msg[] = "error: pipe failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    char filename1[MAX_LINE_LENGTH];
    char filename2[MAX_LINE_LENGTH];
    
    write_string(STDOUT_FILENO, "enter filename for child1: ");
    ssize_t bytes = read(STDIN_FILENO, filename1, MAX_LINE_LENGTH);
    if (bytes <= 0) {
        const char msg[] = "error: read failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    filename1[bytes - 1] = '\0';
    
    write_string(STDOUT_FILENO, "enter filename for child2: ");
    bytes = read(STDIN_FILENO, filename2, MAX_LINE_LENGTH);
    if (bytes <= 0) {
        const char msg[] = "error: read failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    filename2[bytes - 1] = '\0';
    
    pid_t pid1 = fork();
    if (pid1 == -1) {
        const char msg[] = "error: fork failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    if (pid1 == 0) {
        close(pipe1[1]);
        dup2(pipe1[0], STDIN_FILENO);
        close(pipe1[0]);
        close(pipe2[0]);
        close(pipe2[1]);
        
        execl("./child1", "child1", filename1, NULL);
        const char msg[] = "error: execl child1 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    pid_t pid2 = fork();
    if (pid2 == -1) {
        const char msg[] = "error: fork failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    if (pid2 == 0) {
        close(pipe2[1]);
        dup2(pipe2[0], STDIN_FILENO);
        close(pipe2[0]);
        close(pipe1[0]);
        close(pipe1[1]);        
        execl("./child2", "child2", filename2, NULL);
        const char msg[] = "error: execl child2 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    close(pipe1[0]);
    close(pipe2[0]);   
    char line[MAX_LINE_LENGTH];
    int line_count = 1;   
    write_string(STDOUT_FILENO, enter strings (empty line to exit):\n");
    
    while (1) {
        char prompt[32];
        strcpy(prompt, "String ");       
        int n = line_count;
        char num_str[16];
        char *num_ptr = num_str;
        if (n == 0) {
            *num_ptr++ = '0';
        } else {
            char temp[16];
            char *temp_ptr = temp;
            while (n > 0) {
                *temp_ptr++ = '0' + (n % 10);
                n /= 10;
            }
            while (temp_ptr > temp) {
                *num_ptr++ = *--temp_ptr;
            }
        }
        *num_ptr = '\0';        
        strcat(prompt, num_str);
        strcat(prompt, ": ");
        write_string(STDOUT_FILENO, prompt);       
        bytes = read(STDIN_FILENO, line, MAX_LINE_LENGTH);
        if (bytes <= 0) {
            break;
        }
        
        if (bytes == 1 && line[0] == '\n') {
            break;
        }
        
        if (line[bytes - 1] == '\n') {
            line[bytes - 1] = '\0';
            bytes--;
        }
        
        if (bytes > 0) {
            if (line_count % 2 == 1) {
                write(pipe1[1], line, bytes);
                write(pipe1[1], "\n", 1);
            } else {
                write(pipe2[1], line, bytes);
                write(pipe2[1], "\n", 1);
            }
        }
        
        line_count++;
    }
    
    close(pipe1[1]);
    close(pipe2[1]);    
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);   
    write_string(STDOUT_FILENO, "parent process finished.\n");
    return 0;
}
