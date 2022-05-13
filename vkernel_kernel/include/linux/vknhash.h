#ifndef _VKNHASH_H
#define _VKNHASH_H

#define NOT_FOUND 0x8000

struct Vkernel_hashnode {
	unsigned long ino;
	unsigned short v_mode;
	struct Vkernel_hashnode *next; 
};

struct Vkernel_hashmap {
	unsigned long size; 
	unsigned int bits;
	struct Vkernel_hashnode **hashArr; 
};

extern struct Vkernel_hashmap *CreateVkernel_hashmap(unsigned int bits);
extern int InsertVkernel_hashmap(struct Vkernel_hashmap *vknhash,
				 unsigned long ino, unsigned short v_mode);
extern unsigned short GetVkernel_hashmap(struct Vkernel_hashmap *vknhash,
					 unsigned long ino);
extern void DeleteVkernel_hashmap(struct Vkernel_hashmap *vknhash);
extern int RemoveVkernel_hashmap(struct Vkernel_hashmap *vknhash,
				 unsigned long ino);
extern void PrintVkernel_hashmap(struct Vkernel_hashmap *vknhash);
extern struct inode *path_to_inode(const char *filename);
extern int vkn_permission(struct inode *inode, int mask, int flag);
#endif