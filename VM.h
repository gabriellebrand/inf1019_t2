
typedef struct table PageTable;
typedef struct swapRequest SwapRequest;
int pid[4];
SwapRequest *swapmem;
int swapmemID;

int semID;
PageTable Table[4];

int setSemValue();
void delSemValue();
int semaforoP();
int semaforoV();
SwapRequest* inicializaSwapmem (int *swapmemID);
PageTable* initTable (int *TableID);
void liberaTable (PageTable* table, int tableID);
void request (int procID, unsigned int addr, char rw);
void sigusr1Handler (int signal);

void emptyTable(PageTable *table);

