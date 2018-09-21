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

#define MAXNAME 128

void print_mode(unsigned short); //it's easy enough to convert an int to a drwxr-x... string
unsigned char *disk;

//obviously based on ex18 startrer code
int main(int argc, char **argv) 
{

    if(argc != 4) 
	{
        fprintf(stderr, "Usage: ext2_ln <disk name> <original path> <link path>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) 
	{
		perror("mmap");
		exit(1);
    }

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
	printf("the file name is: %s of length %d\n", name, strlen(name));

	//get the location of the inode table
	//it seems to be necessary get the base address of the table as a void* first and then
	//	setting an ext2_inode pointer to it to avoid conflicting assumptions later on when adding
	//	to the table base address
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int inode_table = grpdesc->bg_inode_table;
	void *table_base = disk + EXT2_BLOCK_SIZE*inode_table;
	struct ext2_inode *itable = table_base;
    unsigned int *inode_bitmap = &(grpdesc->bg_inode_bitmap);
    
	//if the directory doesn't exist nothing to do
	//otherwise set it up
	unsigned int orig_index = find_inode(argv[2]);

    //Insert new inode in next available inode location
    unsigned int inew_index = illoc();
    struct ext2_inode *new_inode = itable + inew_index - 1;
    struct ext2_inode *orig_inode = itable + orig_index - 1;
    
    //If the file is a directory the raise error

    //update bitmap
    *inode_bitmap = *inode_bitmap | (1 << (inew_index - 1));

    //Increase link count
    orig_inode->i_links_count++;
    
    //Copy everything
    memcpy(new_inode, orig_inode, sizeof(struct ext2_inode));
    new_inode->i_mode = EXT2_FT_SYMLINK;
    //Change the name of the directory to the specified name
    int i =0;
    while(orig_inode->i_block[i]){
        struct ext2_dir_entry_2 *orig_entry = 
        (struct ext2_dir_entry_2 *) new_inode->i_block[i];
        struct ext2_dir_entry_2 *new_entry = 
        (struct ext2_dir_entry_2 *) new_inode->i_block[i];
        
        new_entry 
    }

   /* 
    int i = 0;
    unsigned int data;
    while(orig_inode->i_block[i]) {
        //Copy everything in the datablock here
        
        
        
        new_inode->i_block[i] = orig_inode->i_block[i];
        i++;
    }*/
    
    return 0;
}

void print_mod(unsigned short value)
{
	char temp;
	//directory flag
	temp = value & 16384 ? 'd' : '-';
	printf ("%c", temp);

	//owner permission bits
	temp = value & 256 ? 'r' : '-';
	printf ("%c", temp);
	temp = value & 128 ? 'w' : '-';
	printf ("%c", temp);
	temp = value & 64 ? 'x' : '-';
	printf ("%c", temp);

	//group permission bits
	temp = value & 32 ? 'r' : '-';
	printf ("%c", temp);
	temp = value & 16 ? 'w' : '-';
	printf ("%c", temp);
	temp = value & 8 ? 'x' : '-';
	printf ("%c", temp);

	//everyone else permission bits
	temp = value & 4 ? 'r' : '-';
	printf ("%c", temp);
	temp = value & 2 ? 'w' : '-';
	printf ("%c", temp);
	temp = value & 1 ? 'x' : '-';
	printf ("%c", temp);
}
