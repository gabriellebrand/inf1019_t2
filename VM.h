

typedef struct table PageTable;
typedef struct swapRequest SwapRequest;

int pid[4];
int swapmemID;
int semID;

PageTable *Table[4];
SwapRequest *swapmem;


/*prototipo das funcoes do semaforo*/
int setSemValue();
void delSemValue();
int semaforoP();
int semaforoV();

/*prototipo das funcoes de inicializacao das memorias compartilhadas*/
SwapRequest* inicializaSwapmem (int *swapmemID);
PageTable* initTable (int *TableID);
void initMemory();

void liberaTable (PageTable* table, int tableID);
void emptyTable(PageTable *table);


void sigusr1Handler (int signal);

/* prototipo das funcoes utilizadas pelos processos */
void request (int procID, unsigned int addr, char rw);
void sigusr2Handler (int signal);