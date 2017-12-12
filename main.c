#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <errno.h>
#include "VM.h"

int pid[4]={0,0,0,0};
int  TableID[4] = {0,0,0,0};

void intHandler(int signal) {
	int i;

	for (i=0; i<4; i++)
		liberaTable(Table[i], TableID[i]);
	delSemValue();

	exit(1);
}

int main (void)
{
	unsigned int addr;
	char rw;
	int procs = 4, pid_id, status, i, j=0;
	FILE *file[4];

	swapmemID = 0;

	char *filename[]={"compilador.log","matriz.log","compressor.log", "simulador.log"};
	char *printstr[]={"[COMPILADOR]\t%08x %c %d\n","[MATRIZ]\t%08x %c %d\n","[COMPRESSOR]\t%08x %c %d\n","[SIMULADOR]\t%08x %c %d\n"};
	
	signal(SIGUSR1, sigusr1Handler);
	signal(SIGINT, intHandler);
	signal(SIGUSR2, sigusr2Handler);

	//initMemory();
	
	//configura semaforo
	semID = semget (8761, 1, 0666 | IPC_CREAT);
	setSemValue(semID);
	
	//inicializa area de memoria que armazena a pagina de page out / page fault
	swapmem = inicializaSwapmem (&swapmemID);
	swapOut = inicializaSwapmem (&swapOutID);
	countID = shmget (IPC_PRIVATE, sizeof(int), IPC_CREAT|S_IRWXU);
	counter = (int*) shmat (countID, 0, 0);

	for (i=0; i<4; i++)
	{
		Table[i] = initTable(&TableID[i]);
		if ((pid[i]=fork())==0)
		{
			
			j = 0;
			file[i] = fopen(filename[i], "r");
			sleep(2); //garante q o GM inicializou o seu loop infinito
			
			//if (i==1) {
			while (j<=10)
			{
				
				fscanf(file[i], "%x %c ", &addr, &rw);
				semaforoP();
				printf(printstr[i], addr, rw, j);
				request(i, addr, rw);
				printf("\n");
				semaforoV();
				
				sleep(1);
				j++;
				

			} //}
			printf("processo morreu \n");

			//esvaziaTabela(Table[i]);
			//while(1);
			//while (fscanf(file[i], "%x %c ", &addr, &rw) == 2);
			//shmdt(Table[i]);

			fclose (file[i]);
			
			exit(0);


		}
	}

	//while(1);

	while (procs) {
		//sleep(1);
		checkCounter(counter);
		if ((pid_id = waitpid(-1, &status, WNOHANG)) > 0){
			for(i=0;i<4;i++) {
				if(pid[i] == pid_id){
					procs--;
					break;
				}
			}
			printf("waitpid = %d\n", i);
			removeProcess(i);
		}
		//
	}

	for (i=0; i<4; i++)
		liberaTable(Table[i], TableID[i]);
	
	delSemValue();
	
	return 0;
}
