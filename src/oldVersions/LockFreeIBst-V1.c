#include"header.h"
void help();
struct node* grandParentHead=NULL;
struct node* parentHead=NULL;
unsigned long numOfNodes;

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->lChild = NULL; 
  node->rChild = NULL;
	node->secDoneFlag=false;
  return(node);
}

void createHeadNodes()
{
  grandParentHead = newLeafNode(MAX_KEY-1);
  grandParentHead->lChild = newLeafNode(MAX_KEY-1);
  parentHead = grandParentHead->lChild;
}

// Atomically perform i|=j. Return previous value of i.
void btsOnDeleteFlag1( tbb::atomic<struct node*>& i)
{
	struct node* j = (struct node*) ((uintptr_t) (struct node*) i | 2);
	struct node* o = i;                  // Atomic read (o = "old value")
	while( ((uintptr_t)o| (uintptr_t)j)!= (uintptr_t)o ) 
	{         // Loop exits if another thread sets the bits
		struct node* k = o;
		o = i.compare_and_swap((struct node*)((uintptr_t)k|(uintptr_t)j),k);
		if( o==k ) break;       // Successful swap
	}
	return;
}

void btsOnPromoteFlag1( tbb::atomic<struct node*>& i)
{
	struct node* j = (struct node*) ((uintptr_t) (struct node*) i | 1);
	struct node* o = i;                  // Atomic read (o = "old value")
	while( ((uintptr_t)o| (uintptr_t)j)!= (uintptr_t)o ) 
	{         // Loop exits if another thread sets the bits
		struct node* k = o;
		o = i.compare_and_swap((struct node*)((uintptr_t)k|(uintptr_t)j),k);
		if( o==k ) break;       // Successful swap
	}
	return;
}

static inline void btsOnDeleteFlag(struct node** p)
{
  void** ptr = (void**) p;
  __sync_or_and_fetch(ptr,2);
  return;
}

static inline void btsOnPromoteFlag(struct node** p)
{
	void** ptr = (void**) p;
	__sync_or_and_fetch(ptr,1);
	return;
}

static inline unsigned long getKey(unsigned long key)
{
	if(key >= 0xFFFFFFFF)
	{
		printf("rawkey:%x\n",key);
		printf("getkey:%x\n",key & 0x7FFFFFFF);
		assert(key < 0xFFFFFFFF);
	}
	return (key & 0x7FFFFFFF);
}

static inline struct node* getAddress(struct node* p)
{
	return (struct node*)((uintptr_t) p & ~((uintptr_t) 3));
}

static inline bool isKeyMarked(unsigned long key)
{
	return ((key & 0x80000000) == 0x80000000);
}

static inline unsigned long setReplaceFlagInKey(unsigned long key)
{
	return (key | 0x80000000);
}

static inline bool isDeleteFlagSet(struct node* p)
{
	return ((uintptr_t) p & 2) != 0;
}

static inline bool isPromoteFlagSet(struct node* p)
{
	return ((uintptr_t) p & 1) != 0;
}

static inline bool isNodeMarked(struct node* p)
{
	return ((uintptr_t) p & 3) != 0;
}

static inline bool isNull(struct node* p)
{
	return getAddress(p) == NULL;
}

static inline struct node* setDeleteFlag(struct node* p)
{
	return (struct node*) ((uintptr_t) p | 2);
}

static inline struct node* setPromoteFlag(struct node* p)
{
	return (struct node*) ((uintptr_t) p | 1);
}

void simpleHelp()
{
	
}

unsigned long lookup(struct threadArgs* tData, unsigned long target)
{
  struct node* node;
  unsigned long lastRightKey;
  struct node* lastRightNode;
	unsigned long nodeKey;
  tData->readCount++;
	
  while(true)
  {
    node = grandParentHead;
		nodeKey = getKey(node->key);
		lastRightNode = node;
    lastRightKey = nodeKey;
		
    while( !isNull(node) ) //Loop until a child of a leaf node which is null is reached
    {
			node = getAddress(node);
			nodeKey = getKey(node->key);
      if(target < nodeKey)
      {
        node = node->lChild;
      }
      else if (target > nodeKey)
      {
				lastRightNode = node;
        lastRightKey = nodeKey;
        node = node->rChild;
      }
      else
      {
        tData->successfulReads++;
        return(1);
      }
    }
    if(getKey(lastRightNode->key) == lastRightKey)
    {
      tData->unsuccessfulReads++;
      return(0);
    }
    tData->readRetries++;
  }
}

bool insert(struct threadArgs* tData, unsigned long insertKey)
{
  struct node* pnode;
  struct node* node;
  struct node* replaceNode;
  unsigned long lastRightKey;
  struct node* lastRightNode;
	unsigned long nodeKey;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
  tData->insertCount++;
  
  while(true)
  {
    while(true)
    {
      pnode = grandParentHead;
      node = parentHead;
			nodeKey = node->key;
      replaceNode = NULL;
			lastRightNode = node;
      lastRightKey = nodeKey;  
			lastUnmarkedPnode = pnode;
			lastUnmarkedNode = node;

      while( !isNull(node) ) //Loop until a child of a leaf node which is null is reached
      {
				if(!isNodeMarked(node))
				{
					lastUnmarkedPnode = pnode;
					lastUnmarkedNode = node;
				}
				nodeKey = getKey(getAddress(node)->key);
        if(insertKey < nodeKey)
        {
          pnode = node;
          node = getAddress(node)->lChild;
        }
        else if (insertKey > nodeKey)
        {
					lastRightNode = node;
          lastRightKey = nodeKey;
          pnode = node;
          node = getAddress(node)->rChild;
        }
        else
        {
          tData->unsuccessfulInserts++;
          return(false);
        }
      }
      if(getKey(getAddress(lastRightNode)->key) == lastRightKey)
      {
        break;  
      }
    }

    if(!tData->isNewNodeAvailable) //reuse nodes
    {  
      tData->newNode = newLeafNode(insertKey);
      tData->isNewNodeAvailable = true;
      replaceNode = tData->newNode;
    }
    else
    {
      replaceNode = tData->newNode;
      replaceNode->key = insertKey;
    }
		node = getAddress(node);
    if(insertKey < getKey(getAddress(pnode)->key)) //left case
    {
      if(getAddress(pnode)->lChild.compare_and_swap(replaceNode,node) == node)
      {
        tData->isNewNodeAvailable = false;
        tData->successfulInserts++;
        return(true);
      }
    }
    else //right case
    {
      if(getAddress(pnode)->rChild.compare_and_swap(replaceNode,node) == node)
      {
        tData->isNewNodeAvailable = false;
        tData->successfulInserts++;
        return(true);
      }
    }
		if(isNodeMarked(node))
		{
			help();
		}
    tData->insertRetries++;
  }
}

bool primarySeekForDelete(struct delSeekRecord* dsr, unsigned long deleteKey)
{
	struct node* pnode;
  struct node* node;
	unsigned long lastRightKey;
  struct node* lastRightNode;
	unsigned long nodeKey;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
	bool keyFound;

	while(true) //primary seek
	{
		pnode = grandParentHead;
		node = parentHead;
		nodeKey = getKey(getAddress(node)->key);
		lastRightNode = node;
		lastRightKey = nodeKey;  
		lastUnmarkedPnode = pnode;
		lastUnmarkedNode = node;
		keyFound = false;
	
		while( !isNull(node) ) //Loop until a child of a leaf node which is null is reached
		{
			if(!isNodeMarked(node))
			{
				lastUnmarkedPnode = pnode;
				lastUnmarkedNode = node;
			}
			nodeKey = getKey(getAddress(node)->key);
			if(deleteKey < nodeKey)
			{
				pnode = node;
				node = getAddress(node)->lChild;
			}
			else if (deleteKey > nodeKey)
			{         
				lastRightNode = node;
				lastRightKey = nodeKey;
				pnode = node;
				node = getAddress(node)->rChild;
			}
			else //key to be deleted is found
			{
				keyFound = true;
				break;
			}
		}
		if(keyFound)
		{
			break;
		}
		if(getKey(getAddress(lastRightNode)->key) == lastRightKey)
		{
			return(false); //key not found
		}
	}
	dsr->pnode = pnode;
	dsr->node = node;
	dsr->lastUnmarkedPnode = lastUnmarkedPnode;
	dsr->lastUnmarkedNode = lastUnmarkedNode;
	assert(getKey(getAddress(node)->key) == deleteKey);
	return(true);
}

bool secondarySeekForDelete(struct node* node, struct node* nRChild, struct sDelSeekRecord* sdsr)
{
	struct node* rpnode;
	struct node* rnode;
	struct node* lcrnode;
	struct node* rcrnode;
	struct node* secondaryLastUnmarkedPnode = NULL;
	struct node* secondaryLastUnmarkedNode = NULL;
	bool isSplCase=false;
	
	rpnode = node;
	rnode = nRChild;
	lcrnode = getAddress(rnode)->lChild;
	rcrnode = getAddress(rnode)->rChild;
	
	if(!isNull(lcrnode) && getAddress(lcrnode) != getAddress(node))
	{
		while(!isNull(lcrnode) && getAddress(lcrnode) != getAddress(node))
		{
			if(!isNodeMarked(lcrnode))
			{
				secondaryLastUnmarkedPnode = rnode;
				secondaryLastUnmarkedNode = lcrnode;
			}
			rpnode = rnode;
			rnode = lcrnode;
			lcrnode = getAddress(lcrnode)->lChild;
		}
		rcrnode = getAddress(rnode)->rChild;
	}
	else //spl case in complex delete
	{
		isSplCase=true;
	}

	sdsr->rpnode = rpnode;
	sdsr->rnode = rnode;
	sdsr->lcrnode = lcrnode;
	sdsr->rcrnode = rcrnode;
	sdsr->secondaryLastUnmarkedPnode = secondaryLastUnmarkedPnode;
	sdsr->secondaryLastUnmarkedNode = secondaryLastUnmarkedNode;
	assert(!isNull(rpnode));
	assert(!isNull(rnode));
	if(isSplCase)
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

void simpleHelp1(struct node* pnode, struct node* node)
{
	assert(isMarked(getAddress(node)->lChild));
	btsOnDeleteFlag1(getAddress(node)->rChild);
	struct node* nlChild = getAddress(node)->lChild;
	struct node* nrChild = getAddress(node)->rChild;
	if(!isNull(nlChild) && !isNull(nrChild)) //complex delete
	{
		if(getAddress(node)->secDoneFlag)
		{
			assert(isKeyMarked(getAddress(node)->key));
			
			struct node* newNode = (struct node*) malloc(sizeof(struct node));
			newNode->key = getKey(getAddress(node)->key);
			newNode->lChild = getAddress(getAddress(node)->lChild);
			newNode->rChild = getAddress(getAddress(node)->rChild);
			newNode->secDoneFlag = false;
			
			if(getAddress(pnode)->rChild.compare_and_swap(setDeleteFlag(newNode),node) == node)
			{
				tData->successfulDeletes++;
				tData->complexDeleteCount++;
			}
		}
	}
	else //simple delete
	{
		if(isNull(nlChild))
		{
			if(getAddress(pnode)->rChild.compare_and_swap(nrChild,node) == node)
			{
				tData->successfulDeletes++;
				tData->simpleDeleteCount++;
			}
		}
		else
		{
			if(getAddress(pnode)->rChild.compare_and_swap(nlChild,node) == node)
			{
				tData->successfulDeletes++;
				tData->simpleDeleteCount++;
			}
		}
	}
	return;
}

bool remove(struct threadArgs* tData, unsigned long deleteKey)
{
	bool isSimpleDelete;	
	struct node* pnode;
	struct node* node;
	struct node* nlChild;
	struct node* nrChild;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
	
	struct node* storedNode = NULL;
	bool CLEANUP = false;
	bool isLeft = true;
	
	tData->deleteCount++;
	
	while(true)
	{
		isSimpleDelete = false;
		pnode = NULL;
		node = NULL;
		nlChild = NULL;
		nrChild = NULL;
		lastUnmarkedPnode = NULL;
		lastUnmarkedNode = NULL;	
		
		if(!primarySeekForDelete(tData->dsr, deleteKey))
		{
			tData->unsuccessfulDeletes++;
			return(false);
		}
		
		pnode = tData->dsr->pnode;
		node = tData->dsr->node;
		lastUnmarkedPnode = tData->dsr->lastUnmarkedPnode;
		lastUnmarkedNode = tData->dsr->lastUnmarkedNode;
		
		nlChild = getAddress(node)->lChild;
		
		if(getAddress(pnode)->lChild == node) //left case
		{
			isLeft = true;
		}
		else if(getAddress(pnode)->rChild == node) //right case
		{
			isLeft = false;
		}
		else
		{
			continue;
		}

		if(!CLEANUP)
		{
			if(getAddress(node)->lChild.compare_and_swap(setDeleteFlag(getAddress(nlChild)),getAddress(nlChild)) == getAddress(nlChild))
			{
				CLEANUP = true;
				storedNode = node;
				nlChild = getAddress(node)->lChild;
			}
			else // CAS FAILED
			{	
				continue;
			}
		}
		
		assert(getAddress(node) == getAddress(storedNode));
		btsOnDeleteFlag1(getAddress(node)->rChild);
		//btsOnDeleteFlag1((struct node**)&getAddress(node)->rChild);
		nrChild = getAddress(node)->rChild;
		assert(CLEANUP);
		assert(isDeleteFlagSet(nlChild));
		assert(isDeleteFlagSet(nrChild));
		
		if(!isNull(nlChild) && !isNull(nrChild)) // complex delete
		{
			struct node* rpnode;
			struct node* rnode;
			struct node* lcrnode;
			struct node* rcrnode;
			struct node* secondaryLastUnmarkedPnode;
			struct node* secondaryLastUnmarkedNode;
			bool isSplCase;
			while(true) //secondary seek
			{
				rpnode = NULL;
				rnode = NULL;
				lcrnode = NULL;
				rcrnode = NULL;
				secondaryLastUnmarkedPnode = NULL;
				secondaryLastUnmarkedNode = NULL;
				isSplCase = false;
				
				nrChild = getAddress(node)->rChild;
				
				if(isNull(nrChild))
				{
					isSimpleDelete = true;
					break;
				}
				
				isSplCase = secondarySeekForDelete(node, nrChild,tData->sdsr);
				rpnode = tData->sdsr->rpnode;
				rnode = tData->sdsr->rnode;
				lcrnode = tData->sdsr->lcrnode;
				rcrnode = tData->sdsr->rcrnode;
				secondaryLastUnmarkedPnode = tData->sdsr->secondaryLastUnmarkedPnode;
				secondaryLastUnmarkedNode = tData->sdsr->secondaryLastUnmarkedNode;
				
				if(!isKeyMarked(getAddress(node)->key)) //key is unmarked
				{
					struct node* CASoutput;
					CASoutput = getAddress(rnode)->lChild.compare_and_swap(setPromoteFlag(getAddress(node)),getAddress(lcrnode));
					if(CASoutput != getAddress(lcrnode)) //CAS FAILED
					{
						if(isPromoteFlagSet(CASoutput))
						{	
							if(getAddress(CASoutput) != getAddress(node))
							{
								break; //restart primary seek
							}
						}
						else
						{
							if(!isNull(CASoutput))
							{
								continue; //restart secondary seek
							}
							else
							{
								assert(isDeleteFlagSet(getAddress(rnode)->lChild));
								//help operation at secondaryLastUnmarkedEdge
								//if secondaryLastUnmarkedEdge doesn't exist, help node->rChild
									if(secondaryLastUnmarkedPnode == NULL)
									{
										simpleHelp1(node,nrChild);
									}
								continue; //restart secondary seek
							}
						}
					}
					//CAS SUCCEEDED
					btsOnPromoteFlag1(getAddress(rnode)->rChild); //set promote flag on rnode->rChild using BTS
					//btsOnPromoteFlag1((struct node**)&getAddress(rnode)->rChild); //set promote flag on rnode->rChild using BTS
					rcrnode = getAddress(rnode)->rChild;
					assert(isPromoteFlagSet(rcrnode));
					assert(!isKeyMarked(getAddress(rnode)->key));
					getAddress(node)->key = setReplaceFlagInKey(getAddress(rnode)->key); //promote rnode key and mark node key
				}
				
				assert(isKeyMarked(getAddress(node)->key));
				//Here key is marked. PromoteFlag is set in rnode's children
				if(!isPromoteFlagSet(rcrnode))
				{
					printf("deleteKey\t:%d\n",deleteKey);
					printf("isLeft\t:%d\n",isLeft);
					printf("isSimpleDelete\t:%d\n",isSimpleDelete);
					printf("pnode\t:%x\n",pnode);
					printf("node\t:%x\n",node);
					printf("nodeKey\t:%x\n",getAddress(node)->key);
					printf("secDoneFlag\t:%d\n",getAddress(node)->secDoneFlag);
					printf("nlChild\t:%x\n",nlChild);
					printf("nrChild\t:%x\n",nrChild);
					printf("node->lChild\t:%x\n",(struct node*) getAddress(node)->lChild);
					printf("node->rChild\t:%x\n",(struct node*) getAddress(node)->rChild);
					printf("rpnode\t:%x\n",rpnode);
					printf("rnode\t:%x\n",rnode);
					printf("rnodeKey\t:%x\n",getAddress(rnode)->key);
					printf("lcrnode\t:%x\n",lcrnode);
					printf("rcrnode\t:%x\n",rcrnode);
					printf("rnode->lChild\t:%x\n",(struct node*) getAddress(rnode)->lChild);
					printf("rnode->rChild\t:%x\n",(struct node*) getAddress(rnode)->rChild);
					assert(false);
				}
				if(!isSplCase)
				{
					if(getAddress(rpnode)->lChild.compare_and_swap(getAddress(rcrnode),getAddress(rnode)) == getAddress(rnode)) // try removing secondary node
					{
						getAddress(node)->secDoneFlag = true;
					}
					else //CAS FAILED
					{
						//help operation at secondaryLastUnmarkedEdge
						//if secondaryLastUnmarkedEdge doesn't exist, then override CASinvariant and help node->rChild
						if(secondaryLastUnmarkedPnode == NULL)
						{
							simpleHelp1(node,nrChild);
						}
						continue; //restart secondary seek
					}
				}
				else
				{
					assert(getAddress(node)->rChild.compare_and_swap(setDeleteFlag(getAddress(rcrnode)),nrChild) == nrChild);
					getAddress(node)->secDoneFlag = true;
				}
				
				//Here key is marked. secDoneFlag is true
				struct node* oldNodeAddr;
				while(true)
				{
					
					assert(isKeyMarked(getAddress(node)->key));
					assert(getAddress(node)->secDoneFlag);
					
					struct node* newNode = (struct node*) malloc(sizeof(struct node));
					newNode->key = getKey(getAddress(node)->key);
					newNode->lChild = getAddress(getAddress(node)->lChild);
					newNode->rChild = getAddress(getAddress(node)->rChild);
					newNode->secDoneFlag = false;
					
					struct node* pCASoutput;
					if(isLeft)
					{
						pCASoutput = getAddress(pnode)->lChild.compare_and_swap(newNode,getAddress(node));
					}
					else
					{
						pCASoutput = getAddress(pnode)->rChild.compare_and_swap(newNode,getAddress(node));
					}
					if(pCASoutput == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->complexDeleteCount++;
						return(true);
					}
					else //CAS FAILED
					{
						if(getAddress(pCASoutput) != getAddress(node))
						{
							return(true);
						}
						else
						{
							//CAS has failed coz the edge is marked. Help at lastUnmarkedEdge.
							//If lastUnmarkedEdge is not (pnode,node) then help
							oldNodeAddr = getAddress(node);
							if(!primarySeekForDelete(tData->dsr, getKey(getAddress(node)->key)))
							{
								//someone else installed a fresh copy
								tData->unsuccessfulDeletes++;
								return(false);
							}
							if(oldNodeAddr != getAddress(tData->dsr->node))
							{
								//someone else installed a fresh copy
								tData->unsuccessfulDeletes++;
								return(false);
							}
							pnode = tData->dsr->pnode;
							node = tData->dsr->node;
							lastUnmarkedPnode = tData->dsr->lastUnmarkedPnode;
							lastUnmarkedNode = tData->dsr->lastUnmarkedNode;
						}
					}
				}
			}
		}
		else //not a complex delete
		{
			isSimpleDelete = true;
		}
		if(isSimpleDelete)
		{
			assert(isDeleteFlagSet(nlChild));
			assert(isDeleteFlagSet(nrChild));
			if(isLeft)
			{
				if(isNull(nlChild))
				{
					if(getAddress(pnode)->lChild.compare_and_swap(getAddress(nrChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
				else
				{
					if(getAddress(pnode)->lChild.compare_and_swap(getAddress(nlChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
			}
			else
			{
				if(isNull(nlChild))
				{
					if(getAddress(pnode)->rChild.compare_and_swap(getAddress(nrChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
				else
				{
					if(getAddress(pnode)->rChild.compare_and_swap(getAddress(nlChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
			}
		}
		
	}
}

void help()
{
	
}

void nodeCount(struct node* node)
{
	//assert(!isDeleteFlagSet(node));
	assert(!isPromoteFlagSet(node));
  if(isNull(node))
  {
    return;
  }
  numOfNodes++;
	
	nodeCount(node->lChild);
  nodeCount(node->rChild);
}

unsigned long size()
{
  numOfNodes=0;
  nodeCount(parentHead->lChild);
  return numOfNodes;
}

void printKeysInOrder(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  printKeysInOrder(node->lChild);
  printf("%lu\t",node->key);
  printKeysInOrder(node->rChild);

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
  if(node->key > min && node->key < max && isValidBST(node->lChild,min,node->key) && isValidBST(node->rChild,node->key,max))
  {
    return true;
  }
  return false;
}

bool isValidTree()
{
  return(isValidBST(parentHead->lChild,0,MAX_KEY));
}