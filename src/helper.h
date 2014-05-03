#include"bitManipulations.h"

struct node* grandParentHead;
struct node* parentHead;
unsigned long numOfNodes;

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->childrenArray[LEFT] = setNull(NULL); 
  node->childrenArray[RIGHT] = setNull(NULL);
	node->secDoneFlag=false;
  return(node);
}

static inline struct node* newReplaceNode(unsigned long key, struct node* lChild, struct node* rChild)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->childrenArray[LEFT] = lChild; 
  node->childrenArray[RIGHT] = rChild;
	node->secDoneFlag=false;
  return(node);
}

// Atomically perform i|=j. Return previous value of i.
void btsOnDeleteFlag( tbb::atomic<struct node*>& i)
{
	struct node* j = (struct node*) ((uintptr_t) (struct node*) i | DELETE_BIT);
	struct node* o = i;                  // Atomic read (o = "old value")
	while( ((uintptr_t)o| (uintptr_t)j)!= (uintptr_t)o ) 
	{         // Loop exits if another thread sets the bits
		struct node* k = o;
		o = i.compare_and_swap((struct node*)((uintptr_t)k|(uintptr_t)j),k);
		if( o==k ) break;       // Successful swap
	}
	return;
}

void btsOnPromoteFlag( tbb::atomic<struct node*>& i)
{
	struct node* j = (struct node*) ((uintptr_t) (struct node*) i | PROMOTE_BIT);
	struct node* o = i;                  // Atomic read (o = "old value")
	while( ((uintptr_t)o| (uintptr_t)j)!= (uintptr_t)o ) 
	{         // Loop exits if another thread sets the bits
		struct node* k = o;
		o = i.compare_and_swap((struct node*)((uintptr_t)k|(uintptr_t)j),k);
		if( o==k ) break;       // Successful swap
	}
	return;
}

void createHeadNodes()
{
  grandParentHead = newLeafNode(MAX_KEY-1);
  grandParentHead->childrenArray[LEFT] = newLeafNode(MAX_KEY-1);
  parentHead = grandParentHead->childrenArray[LEFT];
}

int getChildIndex(struct node* p, unsigned long key)
{
	if(key < getKey(getAddress(p)->key))
	{
		return LEFT;
	}
	else
	{
		return RIGHT;
	}
}
void nodeCount(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  numOfNodes++;
	
	nodeCount(node->childrenArray[LEFT]);
  nodeCount(node->childrenArray[RIGHT]);
}

unsigned long size()
{
  numOfNodes=0;
  nodeCount(parentHead->childrenArray[LEFT]);
  return numOfNodes;
}

void printKeysInOrder(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  printKeysInOrder(getAddress(node)->childrenArray[LEFT]);
  printf("%10x\t%10lu\t%10x\t%10x\t%10d\n",node,getKey(getAddress(node)->key),(struct node*) getAddress(node)->childrenArray[LEFT],(struct node*) getAddress(node)->childrenArray[RIGHT],getAddress(node)->secDoneFlag);
  printKeysInOrder(getAddress(node)->childrenArray[RIGHT]);
}

void printKeys()
{
  printKeysInOrder(parentHead);
  printf("\n");
}

bool isValidBST(struct node* node, unsigned long min, unsigned long max)
{
  if(isNull(node))
  {
    return true;
  }
  if(node->key > min && node->key < max && isValidBST(node->childrenArray[LEFT],min,node->key) && isValidBST(node->childrenArray[RIGHT],node->key,max))
  {
    return true;
  }
  return false;
}

bool isValidTree()
{
  return(isValidBST(parentHead->childrenArray[LEFT],0,MAX_KEY));
}