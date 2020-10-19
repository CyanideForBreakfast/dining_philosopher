/*
 * simulates dining philosopher's problem
 * takes n as user input and creates n philosopher processes
 * each philosopher takes some time to eat
 * algorithm tries to give maximum number of philosophers chance to eat
*/
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_PHILS 100
#define MAX_BUFFER 100
#define EAT_TIME 2

int num_of_phil;
int phil_num=0; //for each philosopher, it is id
pid_t pid=1;
int fork_statuses=0; //shared memory for fork statuses
int* fork_status_array=NULL;
int mutex=0;
char buf[MAX_BUFFER];

void close_prog();
void be_a_philosopher();
void eat();

int main(){
    num_of_phil=0;
    printf("Enter number of philosophers: ");
    scanf(" %d",&num_of_phil);

    /*
     * initialise shared memory
     * array of forks' statuses
     * each cell with a value of 1 (taken) and 0 (free) 
    */
    fork_statuses = shm_open("fork_statuses", O_CREAT | O_RDWR, S_IRWXU);
    ftruncate(fork_statuses,MAX_PHILS*sizeof(int));
    void* ptr = mmap(NULL, MAX_PHILS*sizeof(int),PROT_READ | PROT_WRITE, MAP_SHARED,fork_statuses,0);
    fork_status_array = (int*)ptr;
    for(int i=0;i<num_of_phil;i++) fork_status_array[i] = 0;
    
    /*
     * initialise semaphore
     * lock it initially
    */
    key_t mutex_key;
    union semun{
        int val;
        struct semid_ds* buf;
        ushort array[1];
    } attr;
    if((mutex_key = ftok("./diningphilosopher.c",'a'))==-1){
        perror("ftok"); 
        exit(1);
    }
    if((mutex = semget(mutex_key,1,0660 | IPC_CREAT)) == -1){
        perror("semget"); 
        exit(1);
    }
    attr.val = 0;
    if(semctl(mutex,0,SETVAL, attr)==-1){
        perror("semctl"); 
        exit(1);
    }

    
    //create num_of_phil processes
    for(int i=0;i<num_of_phil;i++){
       pid = fork();
       if(pid==0) {
           phil_num = i;
           printf("New philosopher: %d\n",getpid());
           be_a_philosopher();
           break;
           }
    }

    struct sembuf mutex_control[1];
    mutex_control[0].sem_num = 0;
    mutex_control[0].sem_flg = 0;

    mutex_control[0].sem_op = 1;
    //release semaphore lock
    if(semop(mutex,mutex_control,1)==-1){
        perror("main mutex release");
    }
    //printf("Initial lock released for %d procs\n", num_of_phil);

    signal(SIGINT,close_prog);
    pause();
    //handle signal for SIGINT for terminating all processes
}

void be_a_philosopher(){
    struct sembuf mutex_control[1];
    mutex_control[0].sem_num = 0;
    mutex_control[0].sem_flg = 0;

    while(1){
        //eat, think, repeat

        //check if forks are free - fork id and (id+1)%num_ofphil
        mutex_control[0].sem_op = -1;
        if(semop(mutex,mutex_control,1)==-1){
            perror("bap mutex check wait");
        }
        //if both forks are free
        if(fork_status_array[phil_num] == 0 && fork_status_array[(phil_num+1)%num_of_phil] == 0){
            //release lock
            fork_status_array[phil_num] = 1;
            fork_status_array[(phil_num+1)%num_of_phil] = 1;
            mutex_control[0].sem_op = 1;
            if(semop(mutex,mutex_control,1)==-1){
                perror("bap mutex release picked");
            }
            eat();
            continue;
        }
        mutex_control[0].sem_op = 1;
        if(semop(mutex,mutex_control,1)==-1){
            perror("bap mutex release not picked");
        }
    }
}

void eat(){
    struct sembuf mutex_control[1];
    mutex_control[0].sem_num = 0;
    mutex_control[0].sem_flg = 0;

    printf("%d eating.\n",getpid());
    sleep(EAT_TIME);

    //release forks
    mutex_control[0].sem_op=-1;
    if(semop(mutex,mutex_control,1)==-1){
        perror("eat mutex check wait");
    }

    fork_status_array[phil_num] = 0;
    fork_status_array[(phil_num+1)%num_of_phil] = 0;

    mutex_control[0].sem_op = 1;
    if(semop(mutex,mutex_control,1)==-1){
        perror("eat mutex release");
    }
    return;
}

void close_prog(){
    printf("Closing...\n");
    if(shm_unlink("fork_statuses")==-1){
        perror("shared memory closing failure");
    };
}