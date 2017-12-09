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
	int status, i, j=0;
	FILE *file[4];

	swapmemID = 0;

	char *filename[]={"compilador.log","simulador.log","matriz.log","compressor.log"};
	char *printstr[]={"[COMPILADOR]\t%08x %c %d\n","[SIMULADOR]\t%08x %c %d\n","[MATRIZ]\t%08x %c %d\n","[COMPRESSOR]\t%08x %c %d\n"};
	
	signal(SIGUSR1, sigusr1Handler);
	signal(SIGINT, intHandler);
	
	semID = semget (8761, 1, 0666 | IPC_CREAT);
	setSemValue(semID);
	
	swapmem = inicializaSwapmem (&swapmemID);
	for (i=0; i<4; i++)
	{
		Table[i] = inicializaTable(&TableID[i]);
		if ((pid[i]=fork())==0)
		{
			//if (i!=1) exit(1);
			j = 0;
			sleep(1); //garante q o GM inicializou o seu loop infinito
			file[i] = fopen(filename[i], "r");

			while (j<=10)
			{
				
				fscanf(file[i], "%x %c ", &addr, &rw);
				
				semaforoP();
				printf(printstr[i], addr, rw, j);
				request(i, addr, rw);
				semaforoV();
				
				printf("\n");
				sleep(1);
				j++;
				

			}
			printf("processo morreu \n");

			esvaziaTabela(Table[i]);
			while(1);
			//shmdt(Table[i]);

			fclose (file[i]);
			
			exit(0);


		}
	}

	//while(1);

	for (i=0;i<4;i++) {
		printf("waitpid = %d\n", waitpid(-1, &status, 0));
	}

	
	for (i=0; i<4; i++)
		liberaTable(Table[i], TableID[i]);
	
	delSemValue();
	
	return 0;
}
