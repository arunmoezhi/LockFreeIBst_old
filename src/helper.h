#include"bitManipulations.h"

struct node* R;
struct node* S;
unsigned long numOfNodes;

static inline bool CAS(struct node* parent, int which, struct node* oldChild, struct node* newChild)
{
	return(parent->child[which].compare_and_swap(newChild,oldChild) == oldChild);
}

static inline void BTS(struct node* array, int bit)
{
    asm("lock bts %1,%0" : "+m" (*array) : "r" (bit));
		return;
}

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->markAndKey = key;
  node->child[LEFT] = setNull(NULL); 
  node->child[RIGHT] = setNull(NULL);
	node->readyToReplace=false;
  return(node);
}

void createHeadNodes()
{
  R = newLeafNode(INF_R);
  R->child[RIGHT] = newLeafNode(INF_S);
  S = R->child[RIGHT];
}

void nodeCount(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  numOfNodes++;
	
	nodeCount(node->child[LEFT]);
  nodeCount(node->child[RIGHT]);
}

unsigned long size()
{
  numOfNodes=0;
  nodeCount(S->child[LEFT]);
  return numOfNodes;
}

void printKeysInOrder(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  printKeysInOrder(getAddress(node)->child[LEFT]);
  printf("%10x\t%10lu\t%10x\t%10x\t%10d\n",node,getKey(getAddress(node)->markAndKey),(struct node*) getAddress(node)->child[LEFT],(struct node*) getAddress(node)->child[RIGHT],getAddress(node)->readyToReplace);
  printKeysInOrder(getAddress(node)->child[RIGHT]);
}

void printKeys()
{
  printKeysInOrder(R);
  printf("\n");
}

bool isValidBST(struct node* node, unsigned long min, unsigned long max)
{
  if(isNull(node))
  {
    return true;
  }
  if(node->markAndKey > min && node->markAndKey < max && isValidBST(node->child[LEFT],min,node->markAndKey) && isValidBST(node->child[RIGHT],node->markAndKey,max))
  {
    return true;
  }
  return false;
}

bool isValidTree()
{
  return(isValidBST(S->child[LEFT],0,MAX_KEY));
}