#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 1024
#define SHARED_MEM_SIZE (MAX_LINE_LENGTH * 4)
#define MAX_CHILDREN 2

typedef struct {
    char data[MAX_LINE_LENGTH];
    int length;
    int active;
} shared_data_t;

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
    if (argc != 5) {
        const char msg[] = "usage: child filename shm_name sem_name child_index\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    char *filename = argv[1];
    char *shm_name = argv[2];
    char *sem_name = argv[3];
    int child_index = atoi(argv[4]);
    
    if (child_index < 0 || child_index >= MAX_CHILDREN) {
        const char msg[] = "error: invalid child index\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        const char msg[] = "error: open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        const char msg[] = "error: shm_open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file_fd);
        exit(EXIT_FAILURE);
    }
    
    shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t) * MAX_CHILDREN, 
                                     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        const char msg[] = "error: mmap failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file_fd);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }
    
    sem_t *semaphore = sem_open(sem_name, O_RDWR);
    if (semaphore == SEM_FAILED) {
        const char msg[] = "error: sem_open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file_fd);
        munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }
    
    char line[MAX_LINE_LENGTH];
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

    char *child_name = "child";
    if (argv[0] != NULL && strstr(argv[0], "child1") != NULL) {
        child_name = "child1";
    } else if (argv[0] != NULL && strstr(argv[0], "child2") != NULL) {
        child_name = "child2";
    }
    
    write_string(STDOUT_FILENO, child_name);
    write_string(STDOUT_FILENO, " (PID: ");
    write_string(STDOUT_FILENO, pid_str);
    write_string(STDOUT_FILENO, ") launched, file: ");
    write_string(STDOUT_FILENO, filename);
    write_string(STDOUT_FILENO, "\n");
    
    while (1) {
        sem_wait(semaphore);

        if (shared_data[child_index].active && shared_data[child_index].length > 0) {
            strncpy(line, shared_data[child_index].data, shared_data[child_index].length);
            line[shared_data[child_index].length] = '\0';
        
            shared_data[child_index].length = 0;
            
            sem_post(semaphore);
            
            reverse_string(line);
            
            // Исправленный вывод
            write_string(STDOUT_FILENO, "string ");
            char index_str[2];
            index_str[0] = '1' + child_index;  // "1" или "2"
            index_str[1] = '\0';
            write_string(STDOUT_FILENO, index_str);
            write_string(STDOUT_FILENO, ": \"");
            write_string(STDOUT_FILENO, line);
            write_string(STDOUT_FILENO, "\"\n");
            
            write_string(file_fd, line);
            write_string(file_fd, "\n");
        } else {
            if (!shared_data[child_index].active && shared_data[child_index].length == 0) {
                sem_post(semaphore);
                shared_data[child_index].active = 0;
                break;
            }
            sem_post(semaphore);
        }
        
        usleep(50000);
    }
    
    close(file_fd);
    sem_close(semaphore);
    munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
    shm_unlink(shm_name);
    
    write_string(STDOUT_FILENO, child_name);
    write_string(STDOUT_FILENO, " finished.\n");
    return 0;
}