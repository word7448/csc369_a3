#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/param.h>
#include "ext2.h"
#include "utils.h"

#define MAXNAME 128
int align (int, int);

void print_mode(unsigned short); //it's easy enough to convert an int to a drwxr-x... string

unsigned char *disk;

//obviously based on ex18 startrer code
int main(int argc, char **argv) 
{

    if(argc != 4) 
	{
        fprintf(stderr, "Usage: ext2_cp <disk name> <host path> <image path>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) 
	{
		perror("mmap");
		exit(1);
    }

	//open host file and get its size
	FILE *host = fopen(argv[2], "r");
	if(host == NULL)
	{
		printf("couldn't open the host file\n");
		return 99;
	}
	fseek(host, 0, SEEK_END);
	int file_size = ftell(host);
	fseek(host, 0, SEEK_SET);

	//figure out the file name
	char name[MAXNAME];
	int slength;
	char *token = strtok(argv[2]+1, "/");
	while(token != NULL)
	{
		strncpy(name, token, 128);
		slength = strlen(token);
		if(slength > 128)
		{
			printf("name is too long for this program to work\n");
			return(99);
		}
		name[slength] = 0;
		token = strtok(NULL, "/");
	}
	printf("the file name is: %s of length %d\n", name, (int)strlen(name));

	//get the location of the inode table
	//it seems to be necessary get the base address of the table as a void* first and then
	//	setting an ext2_inode pointer to it to avoid conflicting assumptions later on when adding
	//	to the table base address
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int inode_table = grpdesc->bg_inode_table;
	void *table_base = disk + EXT2_BLOCK_SIZE*inode_table;
	struct ext2_inode *itable = table_base;

	//figure out the record size needed
	//align size to 4 bytes according to http://www.nongnu.org/ext2-doc/ext2.html#IFDIR-REC-LEN
	int rec_size = align(8 + strlen(name) + 1, 4); //+1 for null terminator
	printf("record needs %d bytes\n", rec_size);	

	//find a place to write the new entry
	int folder_inode = find_inode(argv[3]);
	if(folder_inode == 0)
	{
		printf("target location does not exist");
		return 99;
	}

	int import_inode = illoc();
	printf("import_inode %d\n", import_inode);
	struct ext2_inode *folder = itable + folder_inode -1;
	struct ext2_dir_entry_2 *record, *import;
	void *rspot, *next, *t2, *end;
	int i = 0, done = 0, search_block;

	//copy and pasted multipurpose while loop :-)
	while (folder->i_block[i] != 0)
	{
		//check all available blocks
		printf("iterating through datablock %d/12\n", i);
		search_block = folder->i_block[i];
		end = (void*)(disk + EXT2_BLOCK_SIZE * (search_block + 1));
		rspot = disk + EXT2_BLOCK_SIZE * search_block;
		record = rspot;

		while(rspot < end )
		{
			//iterate through all records, looking for the record at the end of the block
			next = rspot + record->rec_len;
			if (next == end)
			{
				//check first that there is place for the new entry
				printf("the end spot has been found\n");
				int required = align(8 + record->name_len + 1, 4);
				int available = record->rec_len - required;
				if(available >= rec_size)
				{
					//write in the new record at the end where there's place
					printf("found a spot with %d to spare\n", available);
					record->rec_len = required;
					t2 = rspot + required; //originally t2 was temp2
					import = (struct ext2_dir_entry_2*)t2;
					import->inode = import_inode;
					import->rec_len = available;
					import->name_len = strlen(name);
					import->file_type = 1;
					strcpy(import->name, name);
					done = 1;
					break;
				}
			}
			rspot = rspot + record->rec_len;
			record = rspot;
		}
		i++; //increment data block counter
	}

	//on the rare case you need a new data block for recording more folder entries
	if(!done)
	{
		balloc(1, &folder->i_block[i]);
		t2 = disk + EXT2_BLOCK_SIZE*folder->i_block[i];
		import = (struct ext2_dir_entry_2*)t2;
		import->inode = import_inode;
		import->rec_len = 1024;
		import->name_len = strlen(name);
		import->file_type = 1;
		strcpy(import->name, name);
	}

	//write the new inode information
	struct ext2_inode *new_inode = (struct ext2_inode*)(itable + import_inode - 1);
	new_inode->i_mode = (unsigned short)33188; //chmod 644
	new_inode->i_uid = (unsigned short)0; //root is the owner and group
	new_inode->i_size = (unsigned int)file_size;
	new_inode->i_atime = (unsigned int)0; //the unix epoch.
	new_inode->i_ctime = (unsigned int)0;
	new_inode->i_mtime = (unsigned int)0;
	new_inode->i_dtime = (unsigned int)0;
	new_inode->i_gid = (unsigned short)0; //root group
	new_inode->i_links_count = (unsigned short)1;
	new_inode->i_blocks = (unsigned int)align(file_size, 512);
	//new_inode->i_faddr = 0;
	balloc(file_size, (unsigned int*)&new_inode->i_block);

	//write the actual data
	char buffer[1024];
	int remainder = file_size;
	void *data_block;
	for(i=0; i<12; i++)
	{
		if(remainder < 0)
		{
			break;
		}
		fread(buffer, 1, MIN(remainder, 1024), host);
		printf("==>copying data to block %d\n", new_inode->i_block[i]);
		data_block = disk + EXT2_BLOCK_SIZE*new_inode->i_block[i];
		memcpy(data_block, buffer, MIN(remainder, 1024));
		remainder = remainder - 1024;
	}
	
	//only do the indirect block ordeal when necessary
	if(file_size > 12288)
	{
		void *extended_spot = disk + EXT2_BLOCK_SIZE*new_inode->i_block[12];
		int *extended = extended_spot;
		i = 0;

		while(remainder > 0)
		{
			fread(buffer, 1, MIN(remainder, 1024), host);
			printf("==>copying data to block %d\n", extended[i]);
			data_block = disk + EXT2_BLOCK_SIZE*extended[i];
			memcpy(data_block, buffer, MIN(remainder, 1024));
			remainder = remainder - 1024;
			i++;
		}
	}
    return 0;
}





