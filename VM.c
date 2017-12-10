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
#define LIVRE 0
#define MAXFRAME 10
#define ZERAR 100
#define MAXTABLE 100

char memoryFrame[MAXFRAME];
int counter = 0;

union semUn 
{
	int
	val;
	struct semid_ds *buf;
	short * array;
};

struct line
{
	unsigned short page;
	unsigned short frame;
	int M;
	int R;
	int V;
};

struct table 
{
	struct line line[MAXTABLE];
	int last; //ultimo indice da tabela que ja foi preenchido com uma page
	char dead; // 1 -> processo ja morreu | 0 -> processo ainda esta executando
};

//memoria compartilhada para que o processo fique sabendo qual 
//pagina perdeu ou para que o GM saiba qual pagina precisa de page fault
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
		for (j=1; j<Table[i]->last; j++) {
			if (Table[i]->line[j].V) {
				*menori = i;
				*menorj = j;
				break;
			}
	}

	for (i=0; i<4; i++)
		for (j=1; j<Table[i]->last; j++) {
			if (Table[i]->line[j].V) {
			//printf("\ntable[i]%04x table[menor]%04x menorj = %d menori = %d\n", Table[i][j].page, Table[*menori][*menorj].page, *menorj, *menori);
				if ((Table[i]->line[j].R <  Table[*menori]->line[*menorj].R)//Escolhe o menos referenciado
					||(Table[i]->line[j].R == Table[*menori]->line[*menorj].R && Table[i]->line[j].M <  Table[*menori]->line[*menorj].M) //Preferencia para nao modificado
					||(Table[i]->line[j].R == Table[*menori]->line[*menorj].R && Table[i]->line[j].M  == Table[*menori]->line[*menorj].M && i==procID)) //Preferencia para pagina do mesmo processo
				{
					*menori=i;
					*menorj=j;

				//	printf("\ntrocou\n");
				}
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
	unsigned short i, j;

	for (i=0; i<table->last; i++)
	{
		if (table->line[i].page == page && table->line[i].V == 1)
		{
			//retorna por referencia o frame de onde esta a pagina
			*frame = table->line[i].frame;
			//se a nova solicitacao for de escrita, deve modificar o bit para 1
			//DUVIDA: se a nova solicitacao for de leitura, deve-se manter o bit anterior?
			if (rw=='W')
				table->line[i].M = 1;
			
			//soma +1 no numero de referencias
			table->line[i].R++;
			return 1;			
		}
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
	//char frame[MAXFRAME];
	unsigned short i, j;

	//checa se todos os frames da memoria fisica estao ocupados

	//se tiver algum disponivel, faz uma busca 
	// for (i=0; i<4; i++)
	// 	for (j=0; j<MAXTABLE; j++) {
	// 		if (Table[i]->line[j].V)
	// 			frame[Table[i][j].frame] = OCUPADO;
	// 	}

	i=0;
	while (memoryFrame[i] == OCUPADO && i < MAXFRAME) {i++;} //Acha o primeiro desocupado

	if (i == MAXFRAME)
		return 0;
	
	*free = i;
	return 1;
}

void zerarReferenciado ()
{
	int i, j;
	for (i=0; i<4; i++)
		if (table[i]->dead) continue;

		for (j=0; j<table[i]->end; j++)
			Table[i]->line[j].R = 0;
}

int procuraPagina(PageTable *table, unsigned short page) 
{
	int i;

	//verifica se a pagina ja esta na tabela
	for (i=0;i<table->last; i++) {
		if (table->line[i].page == page)
			return i; //retorna o indice onde ja esta a pagina
	}

	//nao encontrou, entao adiciona a pagina no ultimo indice e incrementa o contador de ultima pagina
	table->last ++;

	//retorna a primeira linha livre
	return i;
}

void swap (int procID, unsigned short page, char rw)
{
	unsigned short freeframe;
	int menori, menorj, pageIndex;
	counter++;
	//printf("\n[GM][swap] swapmem frame = %04x\n", swapmem->frame);
	sleep(1);

	if (counter>ZERAR)
		zerarReferenciado();

	pageIndex = procuraPagina(Table[procID], page);

	if (freeFrame(&freeframe)) //Existe frame livre
	{
		Table[procID]->line[pageIndex].frame = freeframe;

		//printf("\n[GM][swap] swapmem frame = %04x", swapmem->frame);
		//[DUVIDA]pra que isso?
		swapmem->frame = freeframe;
		//printf("\n[GM][swap]freeframe = %04x, frame = %04x\n", freeframe, swapmem->frame);
	}
	else // Todos os frames estao ocupados
	{
		leastFrequentlyUsed(procID, &menori, &menorj);
		if (Table[menori]->line[menorj].M) //Swap com pagina modificada (2 segundos)
		{
			printf("Page-out com pagina modificada\n");
			sleep(1); //precisa pausar + 1 segundo
		}
		printf("Page-out %04x (processo %d)\n", Table[menori]->line[menorj].page, menori);

		//invalida o BV da pagina que sofreu page out
		Table[menori]->line[menorj].valido = 0;

		//TODO: ADICIONAR UM SWAPMEM SÓ PRA PAGE OUT
		/*
		swapmem->procID =  menori;
		swapmem->rw =  Table[menori]->line[menorj].rw;
		swapmem->page = Table[menori]->line[menorj].page;
		swapmem->frame = Table[menori]->line[menorj].frame;
		*/
		if (menori != procID)
			kill(pid[menori], SIGUSR2); //Avisa que perdeu uma pagina
		
		//passa o frame do que tá saindo pro que tá entrando
		Table[procID]->line[pageIndex].frame = Table[menori][menorj].frame;	
	}

	Table[procID]->line[pageIndex].page = page;
	Table[procID]->line[pageIndex].M = (rw == 'W');
	Table[procID]->line[pageIndex].R = 1;
	Table[procID]->line[pageIndex].V = 1;

}

/*
	Essa funcao trata o handler de uma page fault.
*/
void sigusr1Handler (int signal)
{
	//primeiro dá STOP no processo que solicitou o page fault
	kill(pid[swapmem->procID], SIGSTOP);

	//printf("[GM][sigusrHandler-]swapmem->frame = %04x",swapmem->frame);

	swap (swapmem->procID, swapmem->page, swapmem->rw);
	

	//printf("[GM][sigusrHandler]swapmem->frame = %04x",swapmem->frame);
	kill(pid[swapmem->procID], SIGCONT);
	
	//semaforoV();
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
	table->last = 0;

	return table;
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

void printTable ()
{
	int i, j;
	for (i=0; i<4; i++)
	{
		printf("PAGE\t FRAME\t M\t R\t V");
		if(Table[i]-> dead) continue; //só vai imprimir as tabelas dos processos que ainda estão ativos
		for (j=0; j<Table[i]->last; j++) //só vai imprimir os indices da tabela que ja foi preenchido com paginas
			printf("%04x\t %04x\t %d\t %d\t %d\n", Table[i]->line[j].page, Table[i]->line[j].frame, Table[i]->line[j].M, Table[i]->line[j].R, Table[i]->line[j].V);
		printf("\n");
	}
}

void askForSwap (int procID, unsigned short page, char rw)
{
	//semaforoP();
	//printf("\n[P][askForSwap]frame = %04x \n", swapmem->frame);
	
	/*	confira a area de memoria com a page que solicitou o page fault
		assim o GM conseguirá saber qual foi o processo e a page de page fault */

	swapmem->procID = procID;
	swapmem->page = page;
	swapmem->rw = rw;

	/* envia sinal de page fault para o GM */
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
	//printTable();
}

void emptyTable (PageTable *table)
{
	unsigned short i;

	for (i=0; i<table->last; i++)
	{
		//zera os bits para que não interfiram no LFU
		table->line[i].M = 0;
		table->line[i].R = 0;
		table->line[i].V = 0;
	}
}
