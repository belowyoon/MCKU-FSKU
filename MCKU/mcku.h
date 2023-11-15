#include <string.h>
#include <stdbool.h>
#define FREE_LIST_SIZE 64

extern struct pcb *current;
extern char *ptbr;

struct pcb{
	char pid;
	FILE *fd;
	char *pgtable;
	/* Add more fields if needed */
};

struct pte{
	unsigned int pfn : 6;
	bool unused;
	bool present;
};

struct pcb* proc_list;
int proc_num;
int curr_proc_num;
bool free_list[FREE_LIST_SIZE] = {0};


void ku_scheduler(char pid){
	//printf("scheduler %d\n",curr_proc_num);
	if (curr_proc_num != 0){
		char next_pid = pid;
		while (1) {
			next_pid = (next_pid + 1) % proc_num;
			current = &proc_list[next_pid];
			if (current->pgtable != NULL)
				break;
		}
		ptbr = current->pgtable;
	}
	else {
		//no process left then the current is 0
		current = 0;
	}
	//printf("scheduler end\n");
}


void ku_pgfault_handler(char va){

	//printf("fault handler\n");
	//get vpn
	int pt_index = (va & 0xF0) >> 4;
	//get page table entry
	char *pte = ptbr + pt_index;
	for(int i = 0; i < FREE_LIST_SIZE; i++){
		if (free_list[i] == 0){
			free_list[i] = 1;
			char num = ((i & 0x3F) << 2) | 0x01;
			*pte = num;
			//printf("pte: %02x\n", num);
			break;
		}
	}
	//printf("fault handler end\n");
}

void free_list_update(char * pgtable){
	// for(int i = 0; i < FREE_LIST_SIZE; i++){
	// 	printf("%d",free_list[i]);
	// }
	// printf("\n");
	for (int j = 0; j < 16; j++){
		if((pgtable[j]) != 0){
			int k = (pgtable[j] & 0xFC) >> 2;
			free_list[k] = 0;
		}
	}
	// for(int i = 0; i < FREE_LIST_SIZE; i++){
	// 	printf("%d",free_list[i]);
	// }
	// printf("\n");
}


void ku_proc_exit(char pid){

	//printf("exit start %d\n", pid);
	curr_proc_num--;
	
	if (proc_list[pid].pgtable != NULL){
		//update free_list
		free_list_update(proc_list[pid].pgtable);

		//free pgtable
		free(proc_list[pid].pgtable);
		proc_list[pid].pgtable = NULL;
	}
	if (proc_list[pid].fd != NULL)
		fclose(proc_list[pid].fd);

	if (curr_proc_num == 0)
		free(proc_list);
	//printf("exit end\n");
}


void file_init(int nprocs, char *flist) {
	char filename[200];
    FILE *file;
    int i = 0;

    //open proc.txt
    file = fopen(flist, "r");
    if (file == NULL) {
        exit(1);
    }

    // get file from proc.txt
    while (fgets(filename, 200, file) != NULL) {
        // remove "\n"
        filename[strcspn(filename, "\n")] = '\0';

		//get file and save
		proc_list[i].fd = fopen(filename, "r");
        if (proc_list[i].fd == NULL) {
            continue;
        }
		i++;
    }

    // close proc.txt
    fclose(file);
}

void ku_proc_init(int nprocs, char *flist){
	//memory allocate to proc_list
	proc_list = (struct pcb *)malloc(sizeof(struct pcb) *nprocs);
	proc_num = nprocs;
	curr_proc_num = nprocs;

	//initialize pid and pgtable
	for (int i = 0; i < nprocs; i++){
		proc_list[i].pid = i;

		struct pte* temp = malloc(16 * sizeof(struct pte));
		for (int j = 0; j < 16; j++){
			temp[j].pfn = 0;
			temp[j].unused = 0;
			temp[j].present = 0;
		}
		proc_list[i].pgtable = (char *)temp;
	}

	//initialize file
	file_init(nprocs,flist);

	current = proc_list;
	ptbr = proc_list[0].pgtable;
	//printf("process initialized\n");
}