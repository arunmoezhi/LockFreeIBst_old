#include"header.h"
#include"helper.h"

static inline void SetBit(struct node* array, int bit)
//static inline void SetBit(tbb::atomic<struct node*> array, int bit) 
{
    asm("bts %1,%0" : "+m" (*array) : "r" (bit));
		return;
}


static inline void print(struct node* node)
{
	printf("------------------------\n");
	printf("raw node addr : %x\n",node);
	printf("actual   addr : %x\n",getAddress(node));
	printf("is NULL       : %d\n",isNull(node));
	printf("delete flag   : %d\n",isDFlagSet(node));
	printf("promote flag  : %d\n",isPFlagSet(node));
	printf("is marked     : %d\n",isNodeMarked(node));
	printf("raw key       : %lu\t0x%x\n",getAddress(node)->markAndKey,getAddress(node)->markAndKey);
	printf("actual key    : %lu\t0x%x\n",getKey(getAddress(node)->markAndKey),getKey(getAddress(node)->markAndKey));
	printf("isKeyMarked   : %d\n",isKeyMarked(getAddress(node)->markAndKey));
}

int main()
{
	struct node* node = newLeafNode(MAX_KEY);
	//print(node);
	node = setNull(node);
	//print(node);
	node = setDFlag(node);
	//print(node);
	node = setPFlag(node);
	//print(node);
	getAddress(node)->markAndKey = setReplaceFlagInKey(getAddress(node)->markAndKey);
	//print(node);	
	node = addressWithNullBit(node);
	//print(node);

	getAddress(node)->child[0] = newLeafNode(MAX_KEY-1);
	//print(getAddress(node)->child[0]);
	getAddress(node)->child[0] = setDFlag(getAddress(node)->child[0]);
	getAddress(node)->child[0] = setNull(getAddress(node)->child[0]);
	//print(getAddress(node)->child[0]);
	getAddress(node)->child[0] = addressWithNullBit(getAddress(node)->child[0]);
	getAddress(node)->child[0] = getAddress(getAddress(node)->child[0]);
	print(getAddress(node)->child[0]);
	SetBit((struct node*) &(getAddress(node)->child[0]), 0);
	print(getAddress(node)->child[0]);
	SetBit((struct node*) &(getAddress(node)->child[0]), 1);
	print(getAddress(node)->child[0]);
	SetBit((struct node*) &(getAddress(node)->child[0]), 2);
	print(getAddress(node)->child[0]);

	return 0;
}