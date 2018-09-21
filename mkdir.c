#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <pwd.h> //for getting user name from user id
#include <grp.h> //for getting group name from group id
#include <time.h> //for converting a time stamp to a human readable string
#include "ext2.h"
#include "utils.h"
void print_mode(unsigned short); //it's easy enough to convert an int to a drwxr-x... string

unsigned char *disk;

//obviously based on ex18 startrer code
int main(int argc, char **argv) 
{

    if(argc != 3) 
	{
        fprintf(stderr, "Usage: ext2_ls <disk name> <new folder location>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) 
	{
		perror("mmap");
		exit(1);
    }

	//check if folder location already exists
	int exists = find_inode(argv[2]);
	if(exists)
	{
		printf("folder already exists\n");
		return 99;
	}

	//figure out the folder name
	//	always do a copy before strtok... strtok mutiliates its string
	char argv2[strlen(argv[2])+1];
	strcpy(argv2, argv[2]);
	char name[128];
	int slength;
	char *token = strtok(argv2, "/");
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

	int parent_inode = get_parent(argv[2]);
	if(parent_inode == 0)
	{
		printf("you can't making a directory in a non existant location\n");
		return 99;
	}

	//same old setting up the inode table for use
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int inode_table = grpdesc->bg_inode_table;
	void *table_base = disk + EXT2_BLOCK_SIZE*inode_table;
	struct ext2_inode *itable = table_base;

	//size needed for the new folder record
	//	along with variables for looping through the directory entries of the parent
	int rec_size = align(8 + strlen(name) + 1, 4); //+1 for null terminator
	printf("record needs %d bytes\n", rec_size);
	int i=0, done=0, search_block;
	struct ext2_inode *parent = itable + parent_inode -1;
	struct ext2_dir_entry_2 *record, *new_entry;
	void *rspot, *next, *t2, *end;

	//copy and pasted multipurpose while loop :-)
	int new_inode = illoc();
	while (parent->i_block[i] != 0)
	{
		//check all available blocks
		printf("iterating through datablock %d/12\n", i);
		search_block = parent->i_block[i];
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
					printf("the end spot has %d extra space\n", available);
					record->rec_len = required;
					t2 = rspot + required; //originally t2 was temp2
					new_entry = (struct ext2_dir_entry_2*)t2;
					new_entry->inode = new_inode;
					new_entry->rec_len = available;
					new_entry->name_len = strlen(name);
					new_entry->file_type = 2;
					strcpy(new_entry->name, name);
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
		balloc(1, &parent->i_block[i]);
		t2 = disk + EXT2_BLOCK_SIZE*parent->i_block[i];
		new_entry = (struct ext2_dir_entry_2*)t2;
		new_entry->inode = new_inode;
		new_entry->rec_len = 1024;
		new_entry->name_len = strlen(name);
		new_entry->file_type = 2;
		strcpy(new_entry->name, name);
	}

	//write the new inode information
	struct ext2_inode *new = (struct ext2_inode*)(itable + new_inode - 1);
	new->i_mode = (unsigned short)16877; //chmod 755
	new->i_uid = (unsigned short)0; //root is the owner and group
	new->i_size = (unsigned int)1024;
	new->i_atime = (unsigned int)0; //the unix epoch.
	new->i_ctime = (unsigned int)0;
	new->i_mtime = (unsigned int)0;
	new->i_dtime = (unsigned int)0;
	new->i_gid = (unsigned short)0; //root group
	new->i_links_count = (unsigned short)2; //the . and ..
	new->i_blocks = (unsigned int)2;
	balloc(1, (unsigned int*)&new->i_block);
	parent->i_links_count = parent->i_links_count + 1;

	//write the . entry that comes standard with each folder
	void *dotspot = disk + EXT2_BLOCK_SIZE*new->i_block[0];
	struct ext2_dir_entry_2 *dot = dotspot;
	dot->inode = new_inode;
	dot->rec_len = 12;
	dot->name_len = 1;
	dot->file_type = 2;
	char dotname [2] = ".";
	strcpy(dot->name, dotname);

	//write the .. entry that also comes standard with each folder
	void *dotdotspot = dotspot + 12;
	struct ext2_dir_entry_2 *dotdot = dotdotspot;
	dotdot->inode = parent_inode;
	dotdot->rec_len = 1024-12;
	dotdot->name_len = 2;
	dotdot->file_type = 2;
	char dotdotname [3] = "..";
	strcpy(dotdot->name, dotdotname);
	
	return 0;
}











