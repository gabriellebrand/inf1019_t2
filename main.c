#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <sys/ipc.h>
#include <sys/sem.h>
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
	int wait, status, i, j=0;
	FILE *file[4];

	swapmemID = 0;

	char *filename[]={"compilador.log","matriz.log","compressor.log", "simulador.log"};
	char *printstr[]={"[COMPILADOR]\t%08x %c %d\n","[MATRIZ]\t%08x %c %d\n","[COMPRESSOR]\t%08x %c %d\n","[SIMULADOR]\t%08x %c %d\n"};
	
	signal(SIGUSR1, sigusr1Handler);
	signal(SIGINT, intHandler);
	signal(SIGUSR2, sigusr2Handler);
	
	//configura semaforo
	semID = semget (8761, 1, 0666 | IPC_CREAT);
	setSemValue(semID);
	
	//inicializa area de memoria que armazena a pagina de page out / page fault
	swapmem = inicializaSwapmem (&swapmemID);

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
				printf("\nTO AGUARDANDO P%d\n",i);
				semaforoP();
				printf(printstr[i], addr, rw, j);
				request(i, addr, rw);
				semaforoV();
				
				printf("\n");
				sleep(1);
				j++;
				

			} //}
			printf("processo morreu \n");

			emptyTable(Table[i]);
			//esvaziaTabela(Table[i]);
			//while(1);
			//while (fscanf(file[i], "%x %c ", &addr, &rw) == 2);
			//shmdt(Table[i]);

			fclose (file[i]);
			
			exit(0);


		}
	}

	//while(1);

	for (i=0;i<4;i++) {
		wait = waitpid(-1, &status, 0);
		printf("waitpid = %d\n", wait);
	}

	for (i=0; i<4; i++)
		liberaTable(Table[i], TableID[i]);
	
	delSemValue();
	
	return 0;
}
