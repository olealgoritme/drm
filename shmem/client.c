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
    char *readBuf[1000];
    pid_t pid;

    pid = getpid();

    // open shared memory object
    shmID = shm_open(shmdev, O_RDONLY, S_IRUSR | S_IWUSR);
    if(!shmID) {
        perror("shm_open");
        exit(-1);
    }

    // shared memory mapping
    addr = mmap(NULL, SH_MEM_SIZE, PROT_READ, MAP_SHARED, shmID, 0);
    if(addr == MAP_FAILED) {
        perror("cant mmap");
        exit(-1);
    }

    // copy from mem addr -> array
    memcpy(readBuf, addr, sizeof(readBuf));
    printf("Shared BO: /dev/shm%s\n", shmdev);
    printf("PID: %d\n", pid);
    printf("READ: %s\n", readBuf);
    

    return 0;
}
