#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct inode{
    unsigned int fsize;
    unsigned int blocks;
    unsigned int dptr;
    unsigned int iptr;
};

struct dataTable {
    char inum;
    char name[3];
};

char* block[64];

void print_all() {

    for (int i = 0; i < 64; i++){
        printf("block num: %d\n", i);
            int k = 0;

            for (int j = 0; j < 512; j++){
                k++;
                printf("%02x ",block[i][j]);
                if (k==16){
                    printf("\n");
                    k = 0;
                }
            }
        printf("\n");
    }

}

int find_file(char* filename) {
    struct inode rootInode = ((struct inode*)block[2])[2];
    int rootdatablock = rootInode.dptr + 4;
    for (int i = 0; i < rootInode.fsize / 4; i++) {
        if(((struct dataTable*)block[rootdatablock])[i].inum != 0) {
            //if file exist
            char *checkName = &((struct dataTable*)block[rootdatablock])[i].name;
            if (strcmp( &((struct dataTable*)block[rootdatablock])[i].name, &filename) == 0)
                return ((struct dataTable*)block[rootdatablock])[i].inum;
        }
    }
    return 0;
}

//Imap setting
void setimap(int index, bool set) {
    if (index > 64)
        return;

    int bit = 7 - (index % 8);
    index = index / 8;
    if (set) {
        block[1][index] |= (1 << bit);
    } else {
        block[1][index] &= ~(1 << bit);
    }
}

//Dmap setting
void setdmap(int index, bool set) {
    if (index > 60)
        return;

    int bit = 7 - (index % 8);
    index = index / 8 + 256;
    block[1][index] |= (1<<bit); 
    if (set) {
        block[1][index] |= (1 << bit);
    } else {
        block[1][index] &= ~(1 << bit);
    }
}

// get one free inode index
int getFreeImap() {
    char temp = block[1][0];
    int index = 0;
    for (int i = 0; i < 8; i++) {
        temp = block[1][0+i];
        for (int j = 7; j >= 0; j--){
            int bit = (temp >> j) & 1;
            if (bit != 1) {
                index = (i*8)+(7-j);
                setimap(index, true);
                return index;
            }   
        }
    }
    return index;
}

//check if there is free data block left
bool checkFreeDmap(int needed) {
    int zero = 0, check = 0;
    char temp = block[1][256];
    for (int i = 0; i < 8; i++) {
        temp = block[1][256+i];
        for (int j = 7; j >= 0; j--){
            check++;
            if (check > 60){
                printf("No space\n");
                return false;
            }
            int bit = (temp >> j) & 1;
            if (bit != 1)
                zero++;
            if (zero >= needed){
                return true;
            }
        }

    }
    printf("No space\n");
    return false;
}

//get one free data block index
int getFreeDmap() {
    char temp = block[1][256];
    int index = 0;
    for (int i = 0; i < 8; i++) {
        temp = block[1][256+i];
        for (int j = 7; j >= 0; j--){
            int bit = (temp >> j) & 1;
            if (bit != 1) {
                index = (i*8)+(7-j);
                setdmap(index, true);
                return index;
            }
        }
    }
    return 0;
}

void ku_fs_init() {

    //32kb memory initialize
    for (int i = 0; i < 64; i++) {
        block[i] = calloc(1, 512);
    }

    //setImap
    setimap(0, true);
    setimap(1, true);
    setimap(2, true);
    setdmap(0, true);
    
    //root inode initialize
    struct inode root = {244, 1, 0, 0};
    ((struct inode*)block[2])[2] = root;
}

void allocateIptr(int iptr, int size) {
    iptr += 4;
    unsigned int* blockptr = (unsigned int*)block[iptr];
    for(int i = 0; i < 128; i++) {
        if (blockptr[i] == 0) {
            blockptr[i] = getFreeDmap();  
            size--;
        }
        if (size <= 0)
            break;
    }
}

void writeBlock(char letter, unsigned int blockIndex, int offset, unsigned int size) {
    char *blockptr = (char *)block[blockIndex];
    for (int i = offset; i < size; i++)
        blockptr[i] = letter;
}

void newWrite(struct inode curr, char letter, int neededMoreBlock) {
    unsigned int size = curr.fsize;
    unsigned int dptr = curr.dptr + 4;
    unsigned int iptr= curr.iptr + 4;
    if(size < 512) {
        writeBlock(letter, dptr, 0, size);
    }
    else {
        writeBlock(letter, dptr, 0, 512);

        unsigned int* iptrBlock = (unsigned int*)block[iptr];

        for(int i = 0; i < neededMoreBlock; i++) {
            int insideDptr = iptrBlock[i];
            if(insideDptr != 0) {
                if (i != neededMoreBlock - 1 || size % 512 == 0){
                    writeBlock(letter, insideDptr + 4, 0, 512);
                } else {
                    writeBlock(letter, insideDptr + 4, 0, size % 512);
                }
            }
        }
    }

}

void existWrite(char letter, int iNode, int plusSize) {
    struct inode curr = ((struct inode*)block[iNode/32+2])[iNode%32];
    unsigned int* iptrBlock = (unsigned int*)block[curr.iptr+4];

    int totalSize = curr.fsize + plusSize;
    int neededMoreBlock = totalSize / 512 + 2 - curr.blocks;
    int offset = curr.fsize % 512;

    if (totalSize % 512 == 0)
         neededMoreBlock-=1;

    if (neededMoreBlock > 0){
        if(checkFreeDmap(neededMoreBlock)) {
            allocateIptr(curr.iptr, neededMoreBlock);
        }
    }
    
    //current block has no iptr
    if(curr.blocks < 3) {
        if(totalSize > 512) {
            writeBlock(letter, curr.dptr+4, offset, 512);
            for(int i = 0; i < neededMoreBlock; i++) {
                int insideDptr = iptrBlock[i];
                if (i != neededMoreBlock - 1 || totalSize % 512 == 0){
                    writeBlock(letter, insideDptr + 4, 0, 512);
                } else {
                    writeBlock(letter, insideDptr + 4, 0, totalSize % 512);
                }
            }
        }else {
            writeBlock(letter, curr.dptr+4, offset, totalSize);
        }
    } else {

        //current block has iptr
        int blockOffset = curr.blocks - 3;
        if(curr.fsize % 512 == 0)
            blockOffset++;
        int insideDptr = iptrBlock[blockOffset];

        //needs more block to write
        if (offset + plusSize > 512){

            for(int i = blockOffset; i <= blockOffset + neededMoreBlock; i++) {
                insideDptr = iptrBlock[i];
                
                if (i != blockOffset + neededMoreBlock || totalSize % 512 == 0){
                    writeBlock(letter, insideDptr + 4, 0, 512);
                } else {
                    writeBlock(letter, insideDptr + 4, 0, totalSize % 512);
                }
            }
        }
        else {
        
        //does not need more block to write
            if (totalSize % 512 == 0)
                writeBlock(letter, insideDptr + 4, offset, 512);
            else
                writeBlock(letter, insideDptr + 4, offset, totalSize % 512);
        }
    }

    //update inode
    curr.fsize = totalSize;
    curr.blocks += neededMoreBlock;
    ((struct inode*)block[iNode/32+2])[iNode%32] = curr;

}

void createFile(char* filename, int size) {

    int neededMoreBlock = size / 512;
    if(size % 512 == 0)
        neededMoreBlock--;

    if (checkFreeDmap(2 + neededMoreBlock)) {

        //allocate inode
        int inum = getFreeImap();
        struct inode new;
        new.fsize = size;
        new.blocks = 2 + neededMoreBlock;
        new.dptr = getFreeDmap();
        new.iptr = getFreeDmap();

        ((struct inode*)block[2])[inum] = new;

        if (neededMoreBlock > 0) {
            allocateIptr(new.iptr, neededMoreBlock);
        }

        //new dataTable entry
        struct dataTable newTable;
        newTable.inum = inum;
        strcpy(newTable.name, &filename);

        //allocate rootdir
        struct inode rootInode = ((struct inode*)block[2])[2];
        int rootdatablock = rootInode.dptr + 4;
        for (int i = 0; i < rootInode.fsize / 4; i++) {
            if(((struct dataTable*)block[rootdatablock])[i].inum == 0) {
                ((struct dataTable*)block[rootdatablock])[i] = newTable;
                break;
            }
        }

        //write in file
        char firstChar = filename;
        newWrite(new, firstChar, neededMoreBlock);
    }
}

void readBlock(unsigned int blockIndex, int readSize) {
    int count = 0;
    char *blockptr = (char *)block[blockIndex];
    for (int i = 0; i < readSize; i++){
        count++;
        if(blockptr[i] != 0)
            printf("%c", blockptr[i]);
        else
            break;
        if (count == 16) {
            printf("\n");
            count = 0;
        }
    }       
}

void readFile(int iNode, int readSize) {
    struct inode curr = ((struct inode*)block[iNode/32+2])[iNode%32];
    unsigned int* iptrBlock = (unsigned int*)block[curr.iptr+4];

    if(readSize > curr.fsize)
        readSize = curr.fsize;

    if(readSize < 512) {
        readBlock(curr.dptr + 4, readSize);
    } else {
        readBlock(curr.dptr + 4, 512);
        if (curr.blocks > 2) {
            int blockSize = readSize / 512;
            if (readSize % 512 == 0)
                blockSize -= 1;
            for (int i = 0; i < blockSize; i++) {
                int insideDptr = iptrBlock[i];
                if (i != blockSize -1 || readSize % 512 == 0){
                    readBlock(insideDptr + 4, 512);
                }
                else {
                    readBlock(insideDptr + 4, readSize % 512);
                }
            }
        }
    }
    printf("\n");
}

void deleteBlock(unsigned int blockIndex) {
    setdmap(blockIndex, false);
    blockIndex += 4;
    char *blockptr = (char *)block[blockIndex];
    for (int i = 0; i < 512; i++) {
        blockptr[i] = 0; 
    }  
}

void deleteFile(int iNode) {
    struct inode curr = ((struct inode*)block[iNode/32+2])[iNode%32];
    unsigned int* iptrBlock = (unsigned int*)block[curr.iptr+4];
    
    //data block to 0
    deleteBlock(curr.dptr);

    for (int i = 0 ; i < curr.blocks - 2; i++) {
        int insideDptr = iptrBlock[i];
        deleteBlock(insideDptr);
    }
    deleteBlock(curr.iptr);

    //inode and root dir 0
    struct inode rootInode = ((struct inode*)block[2])[2];
    int rootdatablock = rootInode.dptr + 4;
    for (int i = 0; i < rootInode.fsize / 4; i++) {
        if(((struct dataTable*)block[rootdatablock])[i].inum == iNode) {
            ((struct dataTable*)block[rootdatablock])[i].inum = 0;
        }
    }

    struct inode deleteIblock;
    deleteIblock.fsize = 0;
    deleteIblock.blocks = 0;
    deleteIblock.dptr = 0;
    deleteIblock.iptr = 0;
    ((struct inode*)block[2])[iNode] = deleteIblock;

    setimap(iNode, false);
}

void read_input(char *ilist) {
    FILE *file;
    char* filename;
    char type;
    int size;

    //open input.txt
    file = fopen(ilist, "r");
    if (file == NULL) {
        exit(1);
    }

    while(fscanf(file,"%s %c",&filename, &type, &size) != EOF) {
        int c_inode = find_file(filename);
		if (type == 'w') {
            fscanf(file, " %d", &size);
            if (c_inode == 0) {
                createFile(filename, size);
            }
            else {
                char firstChar = filename;
                existWrite(firstChar, c_inode, size);
            }
        }
        if (type == 'r') {
            fscanf(file, " %d", &size);
            if (c_inode == 0) {
                printf("No such file\n");
            }
            else {
                readFile(c_inode, size);
            }
        }
        if (type == 'd') {
            if (c_inode == 0) {
                printf("No such file\n");
            }
            else {
                deleteFile(c_inode);
            }
        }
    }
    fclose(file);
}

int main(int argc, char *argv[]) {

    //initiallize
    ku_fs_init();

    //gets input file
    read_input(argv[1]);

    //print all data in disk
    print_all();

    for (int i = 0; i < 64; i++)
        free(block[i]);

    return 0;
}