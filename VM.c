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

#define OCUPADO 1
#define LIVRE 0
#define MAXFRAME 256
#define MAXTABLE 65536
#define ZERAR 5
#define LIMITE 512

int nFaults = 0;
int nWrites = 0;

union semUn 
{
	int val;
	struct semid_ds *buf;
	short * array;
};

struct line
{
	unsigned short frame;
	int M;
	int R;
	int V;
};

struct table
{
	struct line line[MAXTABLE];
	char dead; // 1 -> processo ja morreu | 0 -> processo ainda esta executando
};

struct memoryFrame
{
	unsigned short page;
	unsigned char status;
	int process;
};

//memoria compartilhada o GM saiba qual pagina precisa de page fault
struct swapRequest
{
	int procID;
	unsigned short page;
	unsigned short frame;
	char rw;
};

struct memoryFrame memframe[MAXFRAME];

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

void printMetrics() {
	printf("\n---- METRICAS ----\n\tTEMPO:\t%d\n\tDELTA T:\t%d\n\tPAGE FAULTS:\t%d\n\tESCRITAS:\t%d\n", LIMITE, ZERAR, nFaults, nWrites);
}

void printMemory ()
{
	int i;
	printf("PAGE\t FRAME\t M\t R\t (P)\t STATUS\n");
	for (i=0; i<MAXFRAME; i++)
	{	
		//só vai imprimir os indices da tabela que ja foi preenchido com paginas
		printf("%04x\t %04x\t %d\t %d\t %d\t %d\n", memframe[i].page, i, Table[memframe[i].process]->line[memframe[i].page].M, Table[memframe[i].process]->line[memframe[i].page].R, memframe[i].process, memframe[i].status);
	}
}

void sigusr2Handler (int signal) {
	printf("[PROC] Processo perdeu pagina pagina\n" /*%04x\n, swapOut->page*/);
}

void leastFrequentlyUsed (int procID, int *menori, int *menorj)
{
	int i;

	//inicializa com os valores do frame 0
	*menori = memframe[0].process;
	*menorj = memframe[0].page;

	for (i=0; i < MAXFRAME; i++) {
		if (Table[memframe[i].process]->dead == 1) {
			*menori=memframe[i].process;
			*menorj=memframe[i].page;
			return;
		}
			if ((Table[memframe[i].process]->line[memframe[i].page].R  <  Table[*menori]->line[*menorj].R) //Escolhe o menos referenciado
				||(Table[memframe[i].process]->line[memframe[i].page].R == Table[*menori]->line[*menorj].R && Table[memframe[i].process]->line[memframe[i].page].M <  Table[*menori]->line[*menorj].M) //Preferencia para nao modificado
				||(Table[memframe[i].process]->line[memframe[i].page].R == Table[*menori]->line[*menorj].R && Table[memframe[i].process]->line[memframe[i].page].M  == Table[*menori]->line[*menorj].M && memframe[i].process==procID)) //Preferencia para pagina do mesmo processo
			{
				*menori=memframe[i].process;
				*menorj=memframe[i].page;
			}
	}	
				
}

/*
* Percorre todas as linhas da tabela, procurando se a page em questao já está na memoria
* Se estiver na memoria, retorna 1 e retorna o frame onde a pagina esta na memoria
* Faz uma busca sequencial, procurando a página, visto que o indice da tabela nao é o mesmo numero da pagina
*/

int isOnMemory (PageTable *table, unsigned short page, unsigned short *frame, char rw)
{

	if (table->line[page].V == 1)
	{
			//retorna por referencia o frame de onde esta a pagina
			*frame = table->line[page].frame;
			//se a nova solicitacao for de escrita, deve modificar o bit para 1
			//DUVIDA: se a nova solicitacao for de leitura, deve-se manter o bit anterior?
			if (rw=='W')
				table->line[page].M = 1;
			
			//soma +1 no numero de referencias
			table->line[page].R++;
			return 1;			
	}
	return 0;
}

/*
* Procura um frame livre disponivel na memoria.
* Se achar um frame livre, retorna 1 e o respectivo frame por referencia. 
* Se nao, retorna 0.
*/

int freeFrame (unsigned short *free)
{
	unsigned short i;

	//checa se todos os frames da memoria fisica estao ocupados

	for (i = 0; memframe[i].status == OCUPADO && i < MAXFRAME; i++); //Acha o primeiro desocupado

	if (i == MAXFRAME) // nao existe frame livre
		return 0;

	*free = i;
	return 1;
}

void swap (int procID, unsigned short page, char rw)
{
	unsigned short freeframe;
	int menori, menorj;
	
	nFaults++;

	//[DUVIDA]PRA QUE ESSE SLEEP?
	//sleep(1);

	if (freeFrame(&freeframe)) //Existe frame livre
	{
		memframe[freeframe].status = OCUPADO;
		Table[procID]->line[page].frame = freeframe;
		//printf("frame livre %04x\n", freeframe);

		memframe[freeframe].process = procID;
		memframe[freeframe].page = page;

		//[DUVIDA]pra que isso?
		swapmem->frame = freeframe;
	}
	else // Todos os frames estao ocupados
	{
		leastFrequentlyUsed(procID, &menori, &menorj);
		printf("\t[GM] Page-out %04x (processo %d)\n", menorj, menori);
		if (Table[menori]->line[menorj].M) //Page out com pagina modificada (2 segundos)
		{
			printf("\t[GM] Salva pagina modificada\n");
			nWrites++;
			sleep(1); //precisa pausar + 1 segundo
		}

		//invalida o BV da pagina que sofreu page out
		Table[menori]->line[menorj].V = 0;

		kill(pid[menori], SIGUSR2); //Avisa que perdeu uma pagina

		memframe[Table[menori]->line[menorj].frame].process = procID;
		memframe[Table[menori]->line[menorj].frame].page = page;
		//passa o frame do que tá saindo pro que tá entrando
		Table[procID]->line[page].frame = Table[menori]->line[menorj].frame;	
	}

	//termina de configurar os valores da page pós page fault
	Table[procID]->line[page].M = (rw == 'W');
	Table[procID]->line[page].R = 1;
	Table[procID]->line[page].V = 1;

}

/*
	Essa funcao trata o handler de uma page fault.
*/
void sigusr1Handler (int signal)
{
	//primeiro dá STOP no processo que solicitou o page fault
	kill(pid[swapmem->procID], SIGSTOP);

	//printMemory();
	swap (swapmem->procID, swapmem->page, swapmem->rw);

	kill(pid[swapmem->procID], SIGCONT);
}

//inicializa a area de memoria que sera usada para armazenar informacoes da pagina que sofreu page fault ou page out
SwapRequest* inicializaSwapmem (int *swapmemID)
{
	*swapmemID = shmget (IPC_PRIVATE, sizeof(swapmemID), IPC_CREAT|S_IRWXU);
	return (SwapRequest*) shmat (*swapmemID, 0, 0);
}

//Aloca uma tabela de paginas com MAXFRAME entradas
PageTable* initTable (int *TableID)
{
	*TableID = shmget (IPC_PRIVATE, sizeof(PageTable), IPC_CREAT|S_IRWXU);

	PageTable *table;
	table = (PageTable*) shmat (*TableID, 0, 0);

	table->dead = 0;

	return table;
}

void initMemory() {
	int i;
	for (i=0;i<MAXFRAME;i++)
		memframe[i].status = LIVRE;

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

void askForSwap (int procID, unsigned short page, char rw)
{	
	/*	confira a area de memoria com a page que solicitou o page fault
		assim o GM saber qual foi o processo que pediu page fault e qual a page */
	swapmem->procID = procID;
	swapmem->page = page;
	swapmem->rw = rw;

	/* envia sinal de page fault para o GM */
	kill(getppid(), SIGUSR1);
	sleep(1);	//Garante que ele recebe o sigstop antes de sair da funcao
}

void trans (int procID,  unsigned int addr, char rw)
{
	unsigned short page, offset, frame = 10;
	page = (addr>>16);
	offset = addr;
	
	(*counter)++;
	//printf("counter = %d\n", *counter);
	if (!isOnMemory(Table[procID], page, &frame, rw))
	{
		printf("\t[GM]Page fault para endereco %08x\n", addr);

		//semaforoP();
		askForSwap(procID, page, rw);
		//semaforoV();
		printf("[PROC]\t\t%c :%08x -> %04x%04x\n",rw, addr, Table[procID]->line[page].frame, offset);
	} else {
		printf("[PROC]\t\t%c :%08x -> %04x%04x\n",rw, addr, frame, offset);
	}
}

void checkCounter(int *counter) {
	int i;
	if (((*counter)%ZERAR)==0) 
	{
		for(i=0;i<MAXFRAME;i++) 
		{
			Table[memframe[i].process]->line[memframe[i].page].R = 0;
		}
	}
	if (*counter >= LIMITE) 
	{
		printMetrics();
		exit(1);
	}
}


/*remove todas as paginas do processo recem morto que estavam na memoria fisica*/
void removeProcess(int procID) {
	int i;
	for (i=0;i<MAXFRAME;i++) {
		if (memframe[i].process == procID)
			memframe[i].status = LIVRE;
	}
	Table[procID]->dead = 1;
}




