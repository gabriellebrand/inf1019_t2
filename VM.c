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
#include "VM.h"

#define OCUPADO 1
#define MAXTABLE 5
#define ZERAR 100

int counter = 0;

union semUn {
	int
	val;
	struct semid_ds *buf;
	short * array;
};

struct line
{
	unsigned short page;
	unsigned short frame;
	int modificado;
	int referenciado;
};

struct table
{
	struct line line[MAXTABLE];
	int fim;
};

struct swapRequest
{
	int procID;
	unsigned short page;
	unsigned short frame;
	char rw;
};

//////Funcoes de semaforo//////////
int setSemValue()
{
	union semUn semUnion;
	semUnion.val = 1;
	return semctl(semID, 0, SETVAL, semUnion);
}

void delSemValue()
{
	union semUn semUnion;
	semctl(semID, 0, IPC_RMID, semUnion);
}

int semaforoP()
{
	struct sembuf semB;
	semB.sem_num = 0;
	semB.sem_op = -1;
	semB.sem_flg = SEM_UNDO;
	semop(semID, &semB, 1);
	return 0;
}

int semaforoV()
{
	struct sembuf semB;
	semB.sem_num = 0;
	semB.sem_op = 1;
	semB.sem_flg = SEM_UNDO;
	semop(semID, &semB, 1);
	return 0;
}
///////////////////////////


void leastFrequentlyUsed (int procID, int *menori, int *menorj)
{
	int i, j;
	*menori = 0;

	while(Table[*menori]->fim == 0) {
		(*menori)++;
	}

	*menorj=0;
	for (i=0; i<4; i++)
		for (j=1; j<Table[i]->fim; j++) {
			//printf("\ntable[i]%04x table[menor]%04x menorj = %d menori = %d\n", Table[i]->line[j].page, Table[*menori]->line[*menorj].page, *menorj, *menori);
			if (  (Table[i]->line[j].referenciado <  Table[*menori]->line[*menorj].referenciado)//Escolhe o menos referenciado
				||(Table[i]->line[j].referenciado == Table[*menori]->line[*menorj].referenciado && Table[i]->line[j].modificado <  Table[*menori]->line[*menorj].modificado) //Preferencia para nao modificado
				||(Table[i]->line[j].referenciado == Table[*menori]->line[*menorj].referenciado && Table[i]->line[j].modificado == Table[*menori]->line[*menorj].modificado && i==procID)) //Preferencia para pagina do mesmo processo
			{
				*menori=i;
				*menorj=j;

			//	printf("\ntrocou\n");
			}	
		}			
}

int isOnTable (PageTable *table, unsigned short page, unsigned short *frame, char rw)
{
	unsigned short i;
	for (i=0; i<table->fim; i++)
	{
		if (table->line[i].page==page)
		{
			*frame = table->line[i].frame;
			if (rw=='W')
				table->line[i].modificado = 1;
			table->line[i].referenciado++;
			return 1;
		}
	}
	return 0;
}

int freeFrame (unsigned short *free)
{
	char frame[MAXTABLE];
	unsigned short i, j;

	//checa se todos os frames da memoria fisica estao ocupados
	if (Table[0]->fim+Table[1]->fim+Table[2]->fim+Table[3]->fim >= MAXTABLE)
		return 0;

	//se tiver algum disponivel, faz uma busca 
	for (i=0; i<4; i++)
		for (j=0; j<Table[i]->fim; j++)
			frame[Table[i]->line[j].frame] = OCUPADO;

	i=0;
	while (frame[i] == OCUPADO) {i++;} //Acha o primeiro desocupado

	*free = i;

	return 1;
}

void zerarReferenciado ()
{
	int i, j;
	for (i=0; i<4; i++)
		for (j=0; j<Table[i]->fim; j++)
			Table[i]->line[j].referenciado = 0;
}

void swap (int procID, unsigned short page, char rw)
{
	unsigned short freeframe;
	int menori, menorj;
	counter++;
	//printf("\n[GM][swap] swapmem frame = %04x\n", swapmem->frame);
	sleep(1);

	if (counter>ZERAR)
		zerarReferenciado();
	if (freeFrame(&freeframe)) //Existe frame livre
	{
		Table[procID]->line[Table[procID]->fim].page = page;
		Table[procID]->line[Table[procID]->fim].frame = freeframe;
		Table[procID]->line[Table[procID]->fim].modificado = (rw == 'W');
		Table[procID]->line[Table[procID]->fim].referenciado = 1;
		Table[procID]->fim++;

		//printf("\n[GM][swap] swapmem frame = %04x", swapmem->frame);
		swapmem->frame = freeframe;
		//printf("\n[GM][swap]freeframe = %04x, frame = %04x\n", freeframe, swapmem->frame);
	}
	else //Todos os frames estao ocupados
	{
		leastFrequentlyUsed(procID, &menori, &menorj);
		if (Table[menori]->line[menorj].modificado) //Swap com pagina modificada (2 segundos)
		{
			printf("Swap com pagina modificada\n");
			sleep(1);
		}
		printf("Swap-out %04x (processo %d)\n", Table[menori]->line[menorj].page, menori);
		if (menori == procID) //Swap de mesmo processo, sobrescreve
		{
			Table[menori]->line[menorj].page = page;
			Table[menori]->line[menorj].modificado = (rw == 'W');
			Table[menori]->line[menorj].referenciado = 1;
			swapmem->frame = Table[menori]->line[menorj].frame;
		}
		else //esta tirando de uma tabela e colocando na outra
		{
			Table[procID]->line[Table[procID]->fim].page = page;									//Adicionando entrada da tabela do processo
			Table[procID]->line[Table[procID]->fim].frame = Table[menori]->line[menorj].frame;
			Table[procID]->line[Table[procID]->fim].modificado = (rw == 'W');
			Table[procID]->line[Table[procID]->fim].referenciado = 1;
			Table[procID]->fim++;

			swapmem->frame = Table[menori]->line[menorj].frame;

			Table[menori]->line[menorj] = Table[menori]->line[Table[menori]->fim-1]; //Retirando entrada da tabela com o endereco menos referenciado
			Table[menori]->fim--;


			kill(pid[menori], SIGUSR2); //Avisa q perdeu uma pagina
		}

	}


}

void sigusr1Handler (int signal)
{
	kill(pid[swapmem->procID], SIGSTOP);
	//printf("[GM][sigusrHandler-]swapmem->frame = %04x",swapmem->frame);
	swap (swapmem->procID, swapmem->page, swapmem->rw);
	
	//printf("[GM][sigusrHandler]swapmem->frame = %04x",swapmem->frame);
	kill(pid[swapmem->procID], SIGCONT);
	
	//semaforoV();
}

SwapRequest* inicializaSwapmem (int *swapmemID)
{
	*swapmemID = shmget (IPC_PRIVATE, sizeof(swapmemID), IPC_CREAT|S_IRWXU);
	return (SwapRequest*) shmat (*swapmemID, 0, 0);
}

//Aloca uma tabela de paginas com MAXTABLE entradas
PageTable* inicializaTable (int *TableID)
{
	*TableID = shmget (IPC_PRIVATE, sizeof(PageTable), IPC_CREAT|S_IRWXU);
	return (PageTable*) shmat (*TableID, 0, 0);
}

void liberaTable (PageTable* table, int tableID)
{
	shmdt(table);
	shmctl(tableID, IPC_RMID, 0);
}

void liberaSwap ()
{
	shmdt(swapmem);
	shmctl(swapmemID, IPC_RMID, 0);
}

void imprimeTable ()
{
	int i=0, j=0;
	for (i=0; i<4; i++)
	{
		for (j=0; j<Table[i]->fim; j++)
			printf("%04x %04x %d %d\n", Table[i]->line[j].page, Table[i]->line[j].frame, Table[i]->line[j].modificado, Table[i]->line[j].referenciado);
		printf("\n");
	}
}

void askForSwap (int procID, unsigned short page, char rw)
{
	//semaforoP();
	//printf("\n[P][askForSwap]frame = %04x \n", swapmem->frame);
	swapmem->procID = procID;
	swapmem->page = page;
	swapmem->rw = rw;
	kill(getppid(), SIGUSR1);
}

void request (int procID,  unsigned int addr, char rw)
{
	unsigned short page, offset, frame = 10;
	page = (addr>>16);
	offset = addr;
	
	if (!isOnTable(Table[procID], page, &frame, rw))
	{
		//printf("[isOnTable] frame = %04x\n", frame);
		printf("\t\tPage fault para endereco %08x\n", addr);

		askForSwap(procID, page, rw);
		sleep(1);	//Garante que ele recebe o sigstop antes de sair da funcao
		frame = swapmem->frame;
		//printf("[P][request]swapmem->frame = %04x",swapmem->frame);
	}
	printf("\t\t%c :%08x -> %04x%04x\n",rw, addr, frame, offset);
	imprimeTable();
}

void esvaziaTabela(PageTable *table) {
	table->fim = 0;
}
