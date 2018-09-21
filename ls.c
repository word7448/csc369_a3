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
        fprintf(stderr, "Usage: ext2_ls <disk name> <path>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) 
	{
		perror("mmap");
		exit(1);
    }

	//get the location of the inode table
	//it seems to be necessary get the base address of the table as a void* first and then
	//	setting an ext2_inode pointer to it to avoid conflicting assumptions later on when adding
	//	to the table base address
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int inode_table = grpdesc->bg_inode_table;
	void *table_base = disk + EXT2_BLOCK_SIZE*inode_table;
	struct ext2_inode *itable = table_base;

	//if the directory doesn't exist nothing to do
	//otherwise set it up
	unsigned int inode = find_inode(argv[2]);
	if(inode == 0)
	{
		printf("path does not exist");
		return (99); //temporary error code
	}
	struct ext2_inode *folder = (struct ext2_inode*)(itable+(inode-1));
	printf("inode of interest is %d\n", inode);

	//various variables used when iterating through the directory records
	struct ext2_dir_entry_2 *record; //the record itself
	struct ext2_inode *corr; //short for corresponding
	struct passwd *user_info; //used for getting user name from user id
	struct group *group_info; //used for getting group name from group id
	char rname[128]; //the actual file name
	char *htime; //used for getting a human readable date from a timestamp int
	int i=0, search_block, slength;

	//for extra long folders you wanna go through ALL its data blocks
	//loop courtesy of utils.c find_inode
	while (folder->i_block[i] != 0)
	{
		printf("iterating through datablock %d/12\n", i);
		search_block = folder->i_block[i];
		void *rspot = disk + EXT2_BLOCK_SIZE * search_block;
		record = rspot;
		while(rspot <  (void*)(disk + EXT2_BLOCK_SIZE * (search_block + 1)))
		{
			strncpy(rname, record->name, record->name_len); //retrieve name
			rname[record->name_len] = 0; //null terminate name string
			corr = (struct ext2_inode*)(itable+(record->inode)-1); //find corresponding inode to get the other information
			user_info = getpwuid(corr->i_uid); //get user's name from uid
			group_info = getgrgid(corr->i_gid); //get group's name from gid
			htime = ctime((const time_t*)&corr->i_mtime); //get readable date from time stamp
			slength = strlen(htime); 
			htime[slength-1] = 0; //ctime newlines its stuff, undo that
			print_mode(corr->i_mode); //print the permissions first
			printf(" %d %s\t%s\t%d\t%s %s\n", corr->i_links_count, user_info->pw_name, group_info->gr_name, corr->i_size, htime, rname);
			rspot = rspot + record->rec_len; //move raw pointer to next directory record
			record = rspot; //set struct to the new record
		}
		i++; //increment data block counter
	}

    return 0;
}

void print_mode(unsigned short value)
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






