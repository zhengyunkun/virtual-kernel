#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/vknhash.h>
#include <linux/fs.h>
#include <linux/syscalls.h>

#define myMalloc kmalloc
#define myCalloc kcalloc
#define myFree kfree

/**
 * @description: create a hashmap which number is 1<bits
 * @param {bits}    bits of the number of node
 * @return {*}   created hash map
 */
struct Vkernel_hashmap *CreateVkernel_hashmap(unsigned int bits)
{
	struct Vkernel_hashmap *vknhash = (struct Vkernel_hashmap *)myCalloc(
		1, sizeof(struct Vkernel_hashmap), GFP_KERNEL);
	vknhash->hashArr = (struct Vkernel_hashnode **)myCalloc(
		1UL << bits, sizeof(struct Vkernel_hashnode *), GFP_KERNEL);
	if (vknhash == NULL || vknhash->hashArr == NULL) {
		return NULL;
	}
	vknhash->size = 1UL << bits;
	vknhash->bits = bits;
	return vknhash;
}
EXPORT_SYMBOL(CreateVkernel_hashmap);

/**
 * @description:     insert a pair of keyvalue
 * @param {*vknhash} 
 * @param {*ino}     key
 * @param {*v_mode}   value
 * @return {int}     fail = -1, success = 0
 * @attention if ino is same, v_mode will be overwrited by new value
 */
int InsertVkernel_hashmap(struct Vkernel_hashmap *vknhash, unsigned long ino,
			  unsigned short v_mode)
{
	struct Vkernel_hashnode *node = NULL;
	int index = 0;
	if (vknhash == NULL) {
		return -1;
	}
	node = (struct Vkernel_hashnode *)myCalloc(
		1, sizeof(struct Vkernel_hashnode), GFP_KERNEL);
	if (node == NULL) {
		return -1;
	}
	node->ino = ino;
	node->v_mode = v_mode;
	node->next = NULL;
	index = hash_long(ino, vknhash->bits);
	if (vknhash->hashArr[index] == NULL) {
		vknhash->hashArr[index] = node;
	} else {
		struct Vkernel_hashnode *temp = vknhash->hashArr[index];
		struct Vkernel_hashnode *prev = temp;
		while (temp != NULL) {
			if (temp->ino == node->ino) {
				temp->v_mode = v_mode;
				return 0;
			}
			prev = temp;
			temp = temp->next;
		}
		prev->next = node;
	}
	return 0;
}
EXPORT_SYMBOL(InsertVkernel_hashmap);

/**
 * @description:     find v_mode by ino
 * @param {*vknhash} hash_map
 * @param {*ino}     ino
 * @return {*}       v_mode
 */
unsigned short GetVkernel_hashmap(struct Vkernel_hashmap *vknhash,
				  unsigned long ino)
{
	int index = 0;
	struct Vkernel_hashnode *temp = NULL;

	if (vknhash == NULL)
		return NOT_FOUND;

	index = hash_long(ino, vknhash->bits);
	temp = vknhash->hashArr[index];
	while (temp != NULL) {
		if (temp->ino == ino) {
			return temp->v_mode;
		}
		temp = temp->next;
	}
	return NOT_FOUND;
}
EXPORT_SYMBOL(GetVkernel_hashmap);
/**
 * @description:     kfree hashmap
 * @param {*vknhash} 
 * @return {}
 */
void DeleteVkernel_hashmap(struct Vkernel_hashmap *vknhash)
{
	int i = 0;
	if (vknhash == NULL)
		return;
	for (i = 0; i < vknhash->size; i++) {
		struct Vkernel_hashnode *temp = vknhash->hashArr[i];
		struct Vkernel_hashnode *prev = temp;
		while (temp != NULL) {
			prev = temp;
			temp = temp->next;
			myFree(prev);
		}
	}
	myFree(vknhash->hashArr);
	myFree(vknhash);
	vknhash->hashArr = NULL;
	vknhash = NULL;
}
EXPORT_SYMBOL(DeleteVkernel_hashmap);
/**
 * @brief remove hashnode by i_ino
 * @param vknhash 
 * @param ino
 * @return
 */
int RemoveVkernel_hashmap(struct Vkernel_hashmap *vknhash, unsigned long ino)
{
	int index = hash_long(ino, vknhash->bits);
	struct Vkernel_hashnode *temp = vknhash->hashArr[index];
	if (temp == NULL)
		return -1;

	if (temp->ino == ino) {
		vknhash->hashArr[index] = temp->next;
		myFree(temp);
		return 0;
	} else {
		struct Vkernel_hashnode *prev = temp;
		temp = temp->next;
		while (temp != NULL) {
			if (temp->ino == ino) {
				prev->next = temp->next;
				myFree(temp);
				return 0;
			}
			prev = temp;
			temp = temp->next;
		}
	}
	return -1;
}
EXPORT_SYMBOL(RemoveVkernel_hashmap);
/**
 * @description: printk hash map
 * @param {*vknhash}
 * @return {}
 */
void PrintVkernel_hashmap(struct Vkernel_hashmap *vknhash)
{
	struct Vkernel_hashnode *node = NULL;
	int i = 0;
	printk("===========PrintVkernel_hashmap==========");
	for (i = 0; i < vknhash->size; i++) {
		node = vknhash->hashArr[i];
		if (node != NULL) {
			struct Vkernel_hashnode *temp = node;
			while (temp != NULL) {
				printk("[%d]: %ld -> %x", i, temp->ino,
				       temp->v_mode);
				temp = temp->next;
			}
		}
	}
	printk("===============End===============");
}
EXPORT_SYMBOL(PrintVkernel_hashmap);

/**
 * path_to_inode - find the inode of the path
 * @filename: the path to the file
 * @return: the inode of the path
 */
struct inode *path_to_inode(const char *filename)
{
	struct inode *inode = NULL;
	struct file *file = NULL;
	file = filp_open(filename, O_DIRECTORY, 0);

	if (IS_ERR(file)) {
		file = filp_open(filename, O_RDONLY, 0444);
		if (IS_ERR(file)) {
			//printk("The path %s is error!\n", filename);
			return NULL;
		}
	}

	inode = file->f_inode;
	return inode;
}
EXPORT_SYMBOL(path_to_inode);
/**
 * vkn_permission: vkernel permission check
 * @inode:	inode to check access rights for
 * @mask:	right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC, ...)
 * @flag:   1 = vkernel_reg, 0 = vkernel_dir
 * @return: 0 = success
 */
int vkn_permission(struct inode *inode, int mask, int flag)
{
	struct Vkernel_hashmap *vknhash = NULL;
	unsigned short v_mode = 0;
	vknhash = flag ? task_active_pid_ns(current)->vknhash_reg : task_active_pid_ns(current)->vknhash_dir;
	v_mode = GetVkernel_hashmap(vknhash, inode->i_ino);
	if (v_mode != NOT_FOUND) {
		if ((mask & ~v_mode & (MAY_READ | MAY_WRITE | MAY_EXEC)) != 0){
			printk("vkn_permission: vkernel denied currentpid = %d, ino = %ld, mask = %x.\n",current->pid, inode->i_ino, mask);
			return -EACCES;
		}
	}
	return 0;
}
EXPORT_SYMBOL(vkn_permission);
