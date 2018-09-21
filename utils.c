#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "ext2.h"
#include "utils.h"
#include "disk.h"

unsigned int find_inode(char *path)
{
	char path_copy[strlen(path)+1];
	strcpy(path_copy, path);
	printf("looking for %s\n", path_copy);
	const char sep[2] = "/";
	char *token = strtok(path_copy, sep);

	//get locations of the bitmaps and inode table
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int inode_table = grpdesc->bg_inode_table;
	void *table_base = disk + EXT2_BLOCK_SIZE*inode_table; //to avoid incorrect assumptions of pointer arithmetic
	int search_node = EXT2_ROOT_INO; //the first search node is the root

	//setup searching variables
	struct ext2_inode *itable = table_base;
	int i, search_block, done, result;
	struct ext2_dir_entry_2 *record; //treat folder data block as array of entries
	char rname[128];

	while(token != NULL)
	{
		printf("searching inode %d\n", search_node);
		done = 0;//reset done variable for each round
		for(i=0; i<12; i++) //at most 12 blocks (no indirects for folders)
		{
			printf("searching dir block %d\n", i);
			if((itable+(search_node-1))->i_block[i] == 0)
			{//still haven't found the folder/file of interest and no more folder blocks to search...
			 //got a bad path
				return 0; //placeholder error code
			}

			//do the search
			search_block = (itable+(search_node-1))->i_block[i];
			void *rspot = disk + EXT2_BLOCK_SIZE * search_block;
			record = rspot;
			while(rspot <  (void*)(disk + EXT2_BLOCK_SIZE * (search_block + 1)))
			{
				//it is possible to do !strcmp(...) but that doesn't make much sense from a human readable standpoint
				strncpy(rname, record->name, record->name_len);
				rname[record->name_len] = 0;
				result = strcmp(rname, token);
				printf("\tLooking for entry %s, looking @ record for: %s\n", token, rname);
				if(result == 0)
				{	//inode of interest has been found. let's record it
					search_node = record->inode;
					done = 1;
					break;
				}
				rspot = rspot + record->rec_len;
				record = rspot;
			}

			//if the inode of interest has been found, there's no point searching the rest of the blocks
			if(done)
			{
				break;
			}
		}
		token = strtok(NULL, sep);
	}
	return search_node;	
}

//block allocation
//directly writes the block list to the requester's variable
void balloc (int size, unsigned int *result)
{
	//figure out the amount of blocks needed
	int check = size / 1024;
	int blocks = (check*1024 == size) ? check : check+1; //always round up 209 style
	printf("requested %d bytes, getting %d blocks for it\n", size, blocks);

	//get block bitmap location
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int block_bitmap = grpdesc->bg_block_bitmap;
	//subtract available blocks from master count
	grpdesc->bg_free_inodes_count = grpdesc->bg_free_inodes_count - blocks;

	//treat the bitmap as a char array for easy reading and writing
	unsigned char *bitmap = disk + EXT2_BLOCK_SIZE*block_bitmap;
	int i, bitcounter=0, bytecounter=0, found=0;
	unsigned char test = 0;

	for(i=0; i<13; i++)
	{
		if((i+1) > blocks)//write the remaining spots as 0
		{
			printf("already got the required blocks. don't need an entry for %d\n", i+1);
			result[i] = 0;
		}
		else
		{
			//here begins the problem of wanting to work with single bits but getting them in bytes (groups of 8)
			while(bitcounter<128)
			{
				////printf("spot %d examining overall bit %d in byte %d\n", i, bitcounter, bytecounter);
				//printf("testing byte %d with pattern %d\n", bitmap[bytecounter], test);
				test = 1 << (bitcounter % 8); //test each bit in the byte to see which one is free
				if (!(bitmap[bytecounter] & test))//if it's not taken...
				{
					printf("got a spot @ %d for %i\n", bitcounter+1, i);
					bitmap[bytecounter] = bitmap[bytecounter] | test; //write that bit in the byte as taken
					result[i] = bitcounter+1;
					break;
				}
				if(bitcounter % 8 ==  7) //when you've seen all 8 bits in this byte, move on to the next one
				{
					bytecounter++;
				}
				bitcounter++;
			}
			if(bitcounter >= 128)
			{
				printf("virtual disk not big enough for the request\n");
				exit(-1);
			}
		}
	}

	//if the indirect block is needed, make it happen
	if(blocks >= 13)
	{
		printf("================>>>>>>>>>>extended blocking procedure<<<<<<<<<<<<<<=================\n");

		int remainder = blocks - 12;
		printf("ext block %d\n", result[12]);
		void *indirect_spot = disk + EXT2_BLOCK_SIZE *result[12];
		unsigned int *indirect = (unsigned int*)indirect_spot;
		for(i=0; i<remainder; i++)
		{
			//here begins the problem of wanting to work with single bits but getting them in bytes (groups of 8)
			//bit and byte counter should start where it left off as the variable values are still good
			while(bitcounter<128)
			{
				//printf("!!!EXTENDED spot %d examining overall bit %d in byte %d\n", i, bitcounter, bytecounter);
				//printf("testing byte %d with pattern %d\n", bitmap[bytecounter], test);
				test = 1 << (bitcounter % 8); //test each bit in the byte to see which one is free
				if (!(bitmap[bytecounter] & test))//if it's not taken...
				{
					printf("spot %d recorded in the indirect @ %d\n", bitcounter+1, i);
					bitmap[bytecounter] = bitmap[bytecounter] | test; //write that bit in the byte as taken
					indirect[i] = bitcounter+1;
					break;
				}
				if(bitcounter % 8 ==  7) //when you've seen all 8 bits in this byte, move on to the next one
				{
					bytecounter++;
				}
				bitcounter++;
			}

			if(bitcounter >= 128)
			{
				printf("virtual disk not big enough for the request\n");
				exit(-1);
			}
		}
	}
}

//get first free inode
unsigned int illoc ()
{
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int inode_bitmap = grpdesc->bg_inode_bitmap;
	unsigned char *ibitmap = disk + EXT2_BLOCK_SIZE * inode_bitmap;
	int bitcounter=0, bytecounter=0, test=0;
	//here begins the problem of wanting to work with single bits but getting them in bytes (groups of 8)
	//bit and byte counter should start where it left off as the variable values are still good
	while(bitcounter < 32)
	{
		//printf("trying to get a free inode by examining overall bit %d in byte %d\n", bitcounter, bytecounter);
		//printf("testing byte %d with pattern %d\n", ibitmap[bytecounter], test);
		test = 1 << (bitcounter % 8); //test each bit in the byte to see which one is free
		if (!(ibitmap[bytecounter] & test))//if it's not taken...
		{
			printf("got a spot @ %d\n", bitcounter+1);
			ibitmap[bytecounter] = ibitmap[bytecounter] | test; //write that bit in the byte as taken
			break;
		}
		else
		{
			printf("spot %d already taken\n", bitcounter+1);
		}
		if(bitcounter % 8 ==  7) //when you've seen all 8 bits in this byte, move on to the next one
		{
			bytecounter++;
		}
		bitcounter++;
	}
	
	grpdesc->bg_free_inodes_count--;
	if(bitcounter >= 32)
	{
		printf("no more empty inodes left\n");
		exit(-1);
	}
	return bitcounter+1;
}

unsigned int get_parent (char *path)
{
	char path_copy[strlen(path)+1];
	strcpy(path_copy, path);
	printf("looking for parent of %s\n", path_copy);
	//figure out the file name
	//copied from import.c but this trick will also work here
	char name[128];
	int slength;
	char *token = strtok(path_copy, "/");
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
	char parent_path[1024];
	strncpy(parent_path, path, strlen(path) - slength);
	parent_path[strlen(path) - slength] = 0; //null terminate
	printf("parent: %s of %s\n", parent_path, path);
	return find_inode(parent_path);
}

int align (int raw, int factor)
{
	int div = raw / factor;
	int check = div * factor;
	if(check == raw)
	{
		return raw;
	}
	else
	{
		return (div+1)*factor;
	}
}





