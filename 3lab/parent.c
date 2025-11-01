#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <errno.h> 


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

void print_error(const char *msg) {
    printf("%s: errno = %d (%s)\n", msg, errno, strerror(errno));
}


int main() {
    char shm_name[64];
    char sem_name[64];
    pid_t main_pid = getpid();
    
    // Создаем уникальные имена для shared memory и семафоров
    snprintf(shm_name, sizeof(shm_name), "/lab_shm_%d", main_pid);
    snprintf(sem_name, sizeof(sem_name), "/lab_sem_%d", main_pid);
    
    // Создаем shared memory
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0644);
    if (shm_fd == -1) {
        print_error("shm_open failed");
        const char msg[] = "error: shm_open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
        return 0;
    }
    
    // Устанавливаем размер shared memory
    if (ftruncate(shm_fd, sizeof(shared_data_t) * MAX_CHILDREN) == -1) {
        const char msg[] = "error: ftruncate failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
        return 0;
    }
    
    // Memory mapping
    shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t) * MAX_CHILDREN, 
                                     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        const char msg[] = "error: mmap failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
        return 0;
    }
    
    // Инициализируем shared memory
    for (int i = 0; i < MAX_CHILDREN; i++) {
        shared_data[i].length = 0;
        shared_data[i].active = 1;
        memset(shared_data[i].data, 0, MAX_LINE_LENGTH);
    }
    
    // Создаем семафор
    sem_t *semaphore = sem_open(sem_name, O_CREAT, 0644, 1);
    if (semaphore == SEM_FAILED) {
        const char msg[] = "error: sem_open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
        shm_unlink(shm_name);
        exit(EXIT_FAILURE);
    }
    
    char filename1[MAX_LINE_LENGTH];
    char filename2[MAX_LINE_LENGTH];
    
    write_string(STDOUT_FILENO, "enter filename for child1: ");
    ssize_t bytes = read(STDIN_FILENO, filename1, MAX_LINE_LENGTH);
    if (bytes <= 0) {
        const char msg[] = "error: read failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        sem_close(semaphore);
        sem_unlink(sem_name);
        munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
        shm_unlink(shm_name);
        return 0;
    }
    filename1[bytes - 1] = '\0';
    
    write_string(STDOUT_FILENO, "enter filename for child2: ");
    bytes = read(STDIN_FILENO, filename2, MAX_LINE_LENGTH);
    if (bytes <= 0) {
        const char msg[] = "error: read failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        sem_close(semaphore);
        sem_unlink(sem_name);
        munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
        shm_unlink(shm_name);
        return 0;
    }
    filename2[bytes - 1] = '\0';
    
    // Запускаем дочерние процессы
    pid_t pid1 = fork();
    if (pid1 == -1) {
        const char msg[] = "error: fork failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        sem_close(semaphore);
        sem_unlink(sem_name);
        munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
        shm_unlink(shm_name);
        return 0;
    }
    
    if (pid1 == 0) {
        // Дочерний процесс 1
        char shm_name_child[64];
        char sem_name_child[64];
        snprintf(shm_name_child, sizeof(shm_name_child), "/lab_shm_%d", main_pid);
        snprintf(sem_name_child, sizeof(sem_name_child), "/lab_sem_%d", main_pid);
        
        execl("./child", "child1", filename1, shm_name_child, sem_name_child, "0", NULL);
        const char msg[] = "error: execl child1 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    pid_t pid2 = fork();
    if (pid2 == -1) {
        const char msg[] = "error: fork failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        sem_close(semaphore);
        sem_unlink(sem_name);
        munmap(shared_data, sizeof(shared_data_t) * MAX_CHILDREN);
        shm_unlink(shm_name);
    
        return 0;
    }
    
    if (pid2 == 0) {
        // Дочерний процесс 2
        char shm_name_child[64];
        char sem_name_child[64];
        snprintf(shm_name_child, sizeof(shm_name_child), "/lab_shm_%d", main_pid);
        snprintf(sem_name_child, sizeof(sem_name_child), "/lab_sem_%d", main_pid);
        
        execl("./child", "child2", filename2, shm_name_child, sem_name_child, "1", NULL);
        const char msg[] = "error: execl child2 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    // Ждем немного чтобы дочерние процессы успели запуститься
    sleep(1);
    
    char line[MAX_LINE_LENGTH];
    int line_count = 1;
    
    write_string(STDOUT_FILENO, "enter strings (empty line to exit):\n");
    
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
            int child_index = (line_count % 2 == 1) ? 0 : 1;
            
            // Захватываем семафор
            sem_wait(semaphore);
            
            // Копируем данные в shared memory
            strncpy(shared_data[child_index].data, line, bytes);
            shared_data[child_index].data[bytes] = '\0';
            shared_data[child_index].length = bytes;
            shared_data[child_index].active = 1;
            
            // Освобождаем семафор
            sem_post(semaphore);
        }
        
        line_count++;
        
        // Небольшая задержка чтобы дать дочерним процессам время на обработку
        usleep(100000);
    }
    
    // Сигнализируем дочерним процессам о завершении
    sem_wait(semaphore);
    for (int i = 0; i < MAX_CHILDREN; i++) {
        shared_data[i].active = 0;
    }
    sem_post(semaphore);
    
    // Ждем завершения дочерних процессов
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    
    write_string(STDOUT_FILENO, "parent process finished.\n");

}