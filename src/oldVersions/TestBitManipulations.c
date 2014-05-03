#include"header.h"
#include"helper.h"

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->lChild = setNull(NULL); 
  node->rChild = setNull(NULL);
	node->secDoneFlag=false;
  return(node);
}

static inline void print(struct node* node)
{
	printf("------------------------\n");
	printf("raw node addr : %x\n",node);
	printf("actual   addr : %x\n",getAddress(node));
	printf("is NULL       : %d\n",isNull(node));
	printf("delete flag   : %d\n",isDeleteFlagSet(node));
	printf("promote flag  : %d\n",isPromoteFlagSet(node));
	printf("is marked     : %d\n",isNodeMarked(node));
	printf("raw key       : %lu\t0x%x\n",getAddress(node)->key,getAddress(node)->key);
	printf("actual key    : %lu\t0x%x\n",getKey(getAddress(node)->key),getKey(getAddress(node)->key));
	printf("isKeyMarked   : %d\n",isKeyMarked(getAddress(node)->key));
}

int main()
{
	struct node* node = newLeafNode(MAX_KEY);
	print(node);
	node = setNull(node);
	print(node);
	node = setDeleteFlag(node);
	print(node);
	node = setPromoteFlag(node);
	print(node);
	getAddress(node)->key = setReplaceFlagInKey(getAddress(node)->key);
	print(node);	
	node = unmarkNode(node);
	print(node);
	/*
	getAddress(node)->lChild = newLeafNode(MAX_KEY-1);
	print(getAddress(node)->lChild);
	getAddress(node)->lChild = setDeleteFlag(getAddress(node)->lChild);
	getAddress(node)->lChild = setNull(getAddress(node)->lChild);
	print(getAddress(node)->lChild);
	getAddress(node)->lChild = getAddress(getAddress(node)->lChild);
	print(getAddress(node)->lChild);
	*/
	return 0;
}