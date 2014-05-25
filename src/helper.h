#include"bitManipulations.h"

struct node* R;
struct node* S;
unsigned long numOfNodes;

static inline bool CAS(struct node* parent, int which, struct node* oldChild, struct node* newChild)
{
	return(parent->child[which].compare_and_swap(newChild,oldChild) == oldChild);
	//struct node* addr = parent->child[which];
	//return(__sync_bool_compare_and_swap(&addr,oldChild,newChild));
}

static inline void BTS(struct node* array, int bit)
{
    asm("lock bts %1,%0" : "+m" (*array) : "r" (bit));
		
		/*
		bool flag=false;
		while(!flag)
		{
			asm("lock bts %2,%1; setb %0" : "=q" (flag) : "m" (*array), "r" (bit));
		}
		*/
		
		
		//array = (struct node*) ((uintptr_t) array | bit);
		return;
}

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
	#ifdef ENABLE_ASSERT
		assert((uintptr_t)node%8==0);
	#endif
  node->markAndKey = key;
  node->child[LEFT] = setNull(NULL); 
  node->child[RIGHT] = setNull(NULL);
	node->readyToReplace=false;
	node->ownerId = -1;
	node->oldKey = key;
  return(node);
}

void createHeadNodes()
{
  R = newLeafNode(INF_R);
  R->child[RIGHT] = newLeafNode(INF_S);
  S = R->child[RIGHT];
}

int getChildIndex(struct node* p, unsigned long key)
{
	if(key < getKey(getAddress(p)->markAndKey))
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
  printf("%10x\t%10lu(%10lu)\t%10x\t%10x\t%10d\t%10d\n",node,getKey(getAddress(node)->markAndKey),getAddress(node)->oldKey,(struct node*) getAddress(node)->child[LEFT],(struct node*) getAddress(node)->child[RIGHT],getAddress(node)->readyToReplace,getAddress(node)->ownerId);
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


// Atomically perform i|=j. Return previous value of i.
void btsOnDFlag( tbb::atomic<struct node*>& i)
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

void btsOnPFlag( tbb::atomic<struct node*>& i)
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

static inline void delete_BTS_using_CAS(struct node* parent)
{
	struct node* oldChild;
	struct node* newChild;
	oldChild = parent->child[RIGHT];
	newChild = setDFlag(oldChild);
	while(parent->child[RIGHT].compare_and_swap(newChild,oldChild) != oldChild)
	{
		oldChild = parent->child[RIGHT];
		if(isDFlagSet(oldChild))
		{
			return;
		}
		newChild = setDFlag(oldChild);
	}
	return;
}

static inline void promote_BTS_using_CAS(struct node* parent)
{
	struct node* oldChild;
	struct node* newChild;
	oldChild = parent->child[RIGHT];
	newChild = setPFlag(oldChild);
	while(parent->child[RIGHT].compare_and_swap(newChild,oldChild) != oldChild)
	{
		oldChild = parent->child[RIGHT];
		if(isPFlagSet(oldChild))
		{
			return;
		}
		newChild = setPFlag(oldChild);
	}
	return;
}