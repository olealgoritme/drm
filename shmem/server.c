#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// 3840 x 1080 x 4 x 2 = ~33MB
#define SH_MEM_SIZE 33177600

int main(int argc, char *argv[]) {

    int shmID;
    char *shmdev = "/xewmem";
    void *addr;

    pid_t pid;

    pid = getpid();

    // create/truncate shared memory object
    shmID = shm_open(shmdev, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if(!shmID) {
        perror("shm_open");
        exit(-1);
    }


    // shared memory is initalized with size 0, lets fix that
    ftruncate(shmID, SH_MEM_SIZE);

    // shared memory attach
    addr = mmap(NULL, SH_MEM_SIZE, PROT_WRITE, MAP_SHARED, shmID, 0);
    if(addr == MAP_FAILED) {
        perror("cant mmap");
        exit(-1);
    }

    // copy arbitrary string to shared virtual memory address via pointer (instead of using offset)
    char *str = "Hello There!";
    memcpy(addr, str, (sizeof(str) * 2));
    printf("Shared BO: /dev/shm%s\n", shmdev);
    printf("PID: %d\n", pid);

    for(;;) {
        sleep(1);
    }

    ftruncate(shmID, 0);
    munmap(addr, SH_MEM_SIZE);
    shm_unlink(shmdev);
    return 0;
}
