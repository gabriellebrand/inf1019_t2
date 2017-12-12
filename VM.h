typedef struct table PageTable;
typedef struct swapRequest SwapRequest;

int pid[4];
int swapmemID, countID, *counter;
int semID;

PageTable *Table[4];
SwapRequest *swapmem;


/*prototipo das funcoes do semaforo*/
int setSemValue();
void delSemValue();
int semaforoP();
int semaforoV();

/* Funcoes do GM */
void liberaTable (PageTable* table, int tableID);
void removeProcess(int procID);
void checkCounter(int *counter);
void sigusr1Handler (int signal);

/*prototipo das funcoes de inicializacao das memorias compartilhadas*/
SwapRequest* inicializaSwapmem (int *swapmemID);
PageTable* initTable (int *TableID);


/* prototipo das funcoes utilizadas pelos processos */
void trans (int procID, unsigned int addr, char rw);
void sigusr2Handler (int signal);
