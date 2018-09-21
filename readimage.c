#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "utils.h"

unsigned char *disk;
void byte2bits (unsigned char*);

int main(int argc, char **argv) 
{

    if(argc != 4) 
	{
        fprintf(stderr, "Usage: readimg <image file name> <path for find_inode> <# for balloc>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) 
	{
		perror("mmap");
		exit(1);
    }

	//print super block information
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE*1);
	int inodes = sb->s_inodes_count;
    printf("Inodes: %d\n", inodes);
	int blocks = sb->s_blocks_count;
    printf("Blocks: %d\n", blocks);
	printf("Block group:\n");    

	//print group description information
	struct ext2_group_desc *grpdesc = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE*2);
	unsigned int block_bitmap = grpdesc->bg_block_bitmap;
	unsigned int inode_bitmap = grpdesc->bg_inode_bitmap;
	unsigned int inode_table = grpdesc->bg_inode_table;
	printf("\tblock bitmap: %u\n", block_bitmap);
	printf("\tinode bitmap: %u\n", inode_bitmap);
	printf("\tinode table: %hu\n", inode_table);
	printf("\tfree blocks: %hu\n", grpdesc->bg_free_blocks_count);
	printf("\tfree inodes: %hu\n", grpdesc->bg_free_inodes_count);
	printf("\tused_dirs: %hu\n", grpdesc->bg_used_dirs_count);

	//dump block bitmap in binary
	printf("Block bitmap:");	
	int i;
	int max = blocks / 8;
	int base = EXT2_BLOCK_SIZE * block_bitmap;
	for(i=0; i < max; i++)
	{
		byte2bits(disk+base+i);
	}
	printf("\n");

	//dump inode bitmap in binary
	printf("Inode bitmap:");
	max = inodes / 8;
	base = EXT2_BLOCK_SIZE * inode_bitmap;
	for(i=0; i < max; i++)
	{
		byte2bits(disk+base+i);
	}
	printf("\n\n");

	printf("Inodes:\n");
	//needed to prevent conflicting assumptions about pointer arithmetic
	void *table_base = disk + EXT2_BLOCK_SIZE*inode_table;

	//root inode is always in the same place so this part can be hard coded
	struct ext2_inode *root = (struct ext2_inode*) table_base + (EXT2_ROOT_INO-1);
	char type = root->i_mode >> 14 == 1 ? 'd' : 'f';
	printf("[2] type: %c size: %u links: %u blocks: %u\n", type, root->i_size, root->i_links_count, root->i_blocks);
	printf("[2] Blocks:");
	i = 0;
	while(root->i_block[i])
	{//not really necessary for the rootdir i wanna see this in action now
		printf(" %u", root->i_block[i]);
		i++;
	}
	printf("\n");
	
	void *inode_bitmap_dump = disk+base; //still set as inode bitmap base
	int ibits = *((int*)inode_bitmap_dump);

	int inode_no = 12;
	ibits = ibits >> 11;
	while(inode_no <= 32)
	{
		if(ibits & 1)
		{
			struct ext2_inode *inode = (struct ext2_inode*) table_base + (inode_no-1);
			type = inode->i_mode >> 14 == 1 ? 'd' : 'f';
			printf("[%d] type: %c size: %u links: %u blocks: %u\n", inode_no, type, inode->i_size, inode->i_links_count, inode->i_blocks);
			printf("[%d] Blocks:", inode_no);
			i = 0;
			while(inode->i_block[i])
			{
				printf(" %u", inode->i_block[i]);
				i++;
			}
			printf("\n");
		}
		ibits = ibits >> 1;
		inode_no++;
	}

	//now some testing of utilities
	unsigned int myblocks[15];
	printf("inode of / %d\n", find_inode(argv[2]));
	printf("requesting %d blocks\n", atoi(argv[3]));
	balloc(atoi(argv[3]), myblocks);
	for(i=0; i<13; i++)
	{
		printf("myblocks @ %d = %d\n", i, myblocks[i]);
	}
	printf("illoc try 1 for the money %d\n", illoc());
	printf("illoc try 2 for the show %d\n", illoc());
	printf("illoc try 3 because it comes before 4 %d\n", illoc());
	printf("illoc try 4 here we go!!!! %d\n", illoc());
    return 0;
}

//dumps bytes into their bits
void byte2bits (unsigned char *byte)
{
	//all original code, no copy and paste from old assignments, no googling a solution :-)
	printf(" ");
	unsigned char value = *byte;
	char temp;
	temp = value & 1 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 2 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 4 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 8 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 16 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 32 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 64 ? '1' : '0';
	printf ("%c", temp);
	temp = value & 128 ? '1' : '0';
	printf ("%c", temp);
}






