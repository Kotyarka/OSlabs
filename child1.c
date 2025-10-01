#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE_LENGTH 1024

void write_string(int fd, const char *str) {
    write(fd, str, strlen(str));
}

void reverse_string(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char msg[] = "usage: child1 filename\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        const char msg[] = "error: open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    char line[MAX_LINE_LENGTH];
    ssize_t bytes_read;    
    pid_t pid = getpid();
    char pid_str[16];
    char *pid_ptr = pid_str;
    int n = pid;
    if (n == 0) {
        *pid_ptr++ = '0';
    } else {
        char temp[16];
        char *temp_ptr = temp;
        while (n > 0) {
            *temp_ptr++ = '0' + (n % 10);
            n /= 10;
        }
        while (temp_ptr > temp) {
            *pid_ptr++ = *--temp_ptr;
        }
    }
    *pid_ptr = '\0';    
    write_string(STDOUT_FILENO, "child1 (PID: ");
    write_string(STDOUT_FILENO, pid_str);
    write_string(STDOUT_FILENO, ") launched, file: ");
    write_string(STDOUT_FILENO, argv[1]);
    write_string(STDOUT_FILENO, "\n");
    
    while ((bytes_read = read(STDIN_FILENO, line, MAX_LINE_LENGTH)) > 0) {
        if (bytes_read > 0 && line[bytes_read - 1] == '\n') {
            line[bytes_read - 1] = '\0';
            bytes_read--;
        }
        
        if (bytes_read > 0) {
            reverse_string(line);            
            write_string(STDOUT_FILENO, "child1: ");
            write_string(STDOUT_FILENO, line);
            write_string(STDOUT_FILENO, "\n");            
            write_string(file_fd, line);
            write_string(file_fd, "\n");
        }
    }
    
    close(file_fd);
    write_string(STDOUT_FILENO, "child1 finished.\n");
    return 0;
}
