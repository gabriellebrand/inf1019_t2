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
#define MAXFRAME 10
#define ZERAR 100
#define MAXTABLE 100

int counter = 0;

union semUn {
	int
	val;
	struct semid_ds *buf;
	short * array;
};

struct table
{
	unsigned short page;
	unsigned short frame;
	int modificado;
	int referenciado;
	int valido;
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

	//acha o primeiro elemento valido para ser comparado
	for (i=0; i<4; i++)
		for (j=1; j<MAXTABLE; j++) {
			if (Table[i][j]->valido) {
				*menori = i;
				*menorj = j;
				break;
			}
	}

	for (i=0; i<4; i++)
		for (j=1; j<MAXTABLE; j++) {
			if (Table[i][j]->valido) {
			//printf("\ntable[i]%04x table[menor]%04x menorj = %d menori = %d\n", Table[i][j]->page, Table[*menori][*menorj]->page, *menorj, *menori);
				if ((Table[i][j]->referenciado <  Table[*menori][*menorj]->referenciado)//Escolhe o menos referenciado
					||(Table[i][j]->referenciado == Table[*menori][*menorj]->referenciado && Table[i][j]->modificado <  Table[*menori][*menorj]->modificado) //Preferencia para nao modificado
					||(Table[i][j]->referenciado == Table[*menori][*menorj]->referenciado && Table[i][j]->modificado == Table[*menori][*menorj]->modificado && i==procID)) //Preferencia para pagina do mesmo processo
				{
					*menori=i;
					*menorj=j;

				//	printf("\ntrocou\n");
				}
			}	
		}			
}

int isOnMemory (PageTable *table, unsigned short page, unsigned short *frame, char rw)
{
	unsigned short i, j;
	for (j = 0; j < 4; j++) {
		for (i=0; i<MAXTABLE i++)
		{
			if (table[i]->page == page && table[i]->valido)
			{
				*frame = table[i]->frame;
				if (rw=='W')
					table[i]->modificado = 1;
				table[i]->referenciado++;

				return 1;
			}
		}
	}
	return 0;
}

int freeFrame (unsigned short *free)
{
	char frame[MAXFRAME];
	unsigned short i, j;

	//checa se todos os frames da memoria fisica estao ocupados

	//se tiver algum disponivel, faz uma busca 
	for (i=0; i<4; i++)
		for (j=0; j<MAXTABLE; j++) {
			if (Table[i][j]->valido)
				frame[Table[i][j]->frame] = OCUPADO;
		}

	i=0;
	while (frame[i] == OCUPADO && i < MAXFRAME) {i++;} //Acha o primeiro desocupado

	if (i == MAXFRAME)
		return 0;
	
	*free = i;
	return 1;
}

void zerarReferenciado ()
{
	int i, j;
	for (i=0; i<4; i++)
		for (j=0; j<MAXTABLE; j++)
			Table[i][j]->referenciado = 0;
}

int procuraPagina(PageTable *table, unsigned short page) {
	int i;

	//verifica se ja foi colocado na page table
	for (i=0;i<MAXTABLE; i++) {
		if (table[i]->page == page)
			return i;
	}

	//retorna a primeira linha livre
	for (i = 0; table[i]->page != 0; i++);
	return i;
}

void swap (int procID, unsigned short page, char rw)
{
	unsigned short freeframe;
	int menori, menorj, pageOnTable;
	counter++;
	//printf("\n[GM][swap] swapmem frame = %04x\n", swapmem->frame);
	sleep(1);

	if (counter>ZERAR)
		zerarReferenciado();

	pageOnTable = procuraPagina(Table[procID], page);

	if (freeFrame(&freeframe)) //Existe frame livre
	{
		Table[procID][pageOnTable]->page = page;
		Table[procID][pageOnTable]->frame = freeframe;
		Table[procID][pageOnTable]->modificado = (rw == 'W');
		Table[procID][pageOnTable]->referenciado = 1;
		Table[procID][pageOnTable]->valido = 1;

		//printf("\n[GM][swap] swapmem frame = %04x", swapmem->frame);
		swapmem->frame = freeframe;
		//printf("\n[GM][swap]freeframe = %04x, frame = %04x\n", freeframe, swapmem->frame);
	}
	else //Todos os frames estao ocupados
	{
		leastFrequentlyUsed(procID, &menori, &menorj);
		if (Table[menori][menorj]->modificado) //Swap com pagina modificada (2 segundos)
		{
			printf("Swap com pagina modificada\n");
			sleep(1);
		}
		printf("Swap-out %04x (processo %d)\n", Table[menori][menorj]->page, menori);

		Table[menori][menorj]->valido = 0;

			Table[procID][pageOnTable]->page = page;
			Table[procID][pageOnTable]->modificado = (rw == 'W');
			Table[procID][pageOnTable]->referenciado = 1;
			//passa o frame do que tá saindo pro que tá entrando
			Table[procID][pageOnTable]->frame = Table[menori][menorj]->frame;
			Table[procID][pageOnTable]->valido = 1;
			swapmem->frame = Table[menori][menorj]->frame;

		
		if (menori != procID)
			kill(pid[menori], SIGUSR2); //Avisa q perdeu uma pagina

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

//Aloca uma tabela de paginas com MAXFRAME entradas
PageTable* inicializaTable (int *TableID)
{
	*TableID = shmget (IPC_PRIVATE, sizeof(PageTable)*MAXTABLE, IPC_CREAT|S_IRWXU);
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
	int i, j;
	for (i=0; i<4; i++)
	{
		for (j=0; j<MAXTABLE; j++)
			printf("%04x %04x %d %d\n", Table[i][j].page, Table[i][j]->frame, Table[i][j]->modificado, Table[i][j]->referenciado);
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
	
	if (!isOnMemory(Table[procID], page, &frame, rw))
	{
		//printf("[isOnMemory] frame = %04x\n", frame);
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
