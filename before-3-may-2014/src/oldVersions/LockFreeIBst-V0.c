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
  grandParentHead = newLeafNode(MAX_KEY);
  grandParentHead->lChild = newLeafNode(MAX_KEY);
  parentHead = grandParentHead->lChild;
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
		nodeKey = node->key;
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
	assert(!isNull(node));
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
	
	if(!isNull(lcrnode))
	{
		while(!isNull(lcrnode))
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
	
	if(isSplCase)
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

bool remove(struct threadArgs* tData, unsigned long deleteKey)
{
	struct node* pnode;
	struct node* node;
	struct node* nlChild;
	struct node* nrChild;
	unsigned long lastRightKey;
	struct node* lastRightNode;
	unsigned long nodeKey;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
	struct node* storedNode = NULL;
	bool CLEANUP = false;
	bool isLeft = true;
	bool isSimpleDelete=false;
	tData->deleteCount++;
	
	while(true)
	{
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
		if(getAddress(node)->secDoneFlag)
		{
			assert(isDeleteFlagSet(getAddress(node)->lChild));
			assert(isDeleteFlagSet(getAddress(node)->rChild));
		}
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
			struct node* nlChildWithDFlagSet = setDeleteFlag(nlChild);
			if(getAddress(node)->lChild.compare_and_swap(nlChildWithDFlagSet, getAddress(nlChild)) != getAddress(nlChild)) //setting DFlag on node's lChild
			{
				help(); //CAS failed so help
				continue;
			}
			else //CAS succeeded
			{					
				assert(isNodeMarked(getAddress(node)->lChild));
				assert(getAddress(node)->lChild == nlChildWithDFlagSet);
				CLEANUP = true;
				storedNode = node;
				assert(!isNull(storedNode));
			}
		}

		if(getAddress(storedNode) != getAddress(node)) //Someone else removed the node for me
		{
			printf("node			: %x\n",node);
			printf("storednode: %x\n",storedNode);
			assert(false);
			return(true);
		}				
		btsOnDeleteFlag((struct node**)&getAddress(node)->rChild);
		nrChild = getAddress(node)->rChild;
		assert(isDeleteFlagSet(getAddress(node)->lChild));
		assert(isDeleteFlagSet(getAddress(node)->rChild));
		assert(getAddress(node)->rChild == nrChild);
		//examine which case applies
		if(!isNull(nlChild) && !isNull(nrChild)) //possible complex delete
		{
			struct node* rpnode;
			struct node* rnode;
			struct node* lcrnode;
			struct node* rcrnode;
			struct node* secondaryLastUnmarkedPnode;
			struct node* secondaryLastUnmarkedNode;
			bool isSplCase;
			bool isFirstTime=true;
			struct node* oldrpnode;
			struct node* oldrnode;
			struct node* oldlcrnode;
			struct node* oldrcrnode;
			while(true) //secondary seek
			{
				nrChild = getAddress(node)->rChild;
				if(!isNull(nrChild))
				{
					isSplCase = secondarySeekForDelete(node, nrChild,tData->sdsr);
				}
				else
				{
					isSimpleDelete = true;
					break;
				}
				rpnode = tData->sdsr->rpnode;
				rnode = tData->sdsr->rnode;
				lcrnode = tData->sdsr->lcrnode;
				rcrnode = tData->sdsr->rcrnode;
				secondaryLastUnmarkedPnode = tData->sdsr->secondaryLastUnmarkedPnode;
				secondaryLastUnmarkedNode = tData->sdsr->secondaryLastUnmarkedNode;
				assert(isNull(lcrnode));
				if(isFirstTime)
				{
					oldrpnode = rpnode;
					oldrnode = rnode;
					oldlcrnode = lcrnode;
					oldrcrnode = rcrnode;
					isFirstTime = false;
				}
				else
				{
					if(isKeyMarked(getAddress(node)->key))
					{
						assert(getAddress(oldrpnode) == getAddress(rpnode));
						assert(getAddress(oldrnode) == getAddress(rnode));
						assert(getAddress(oldlcrnode) == getAddress(lcrnode));
						assert(getAddress(oldrcrnode) == getAddress(rcrnode));
					}
				}
				if(!isKeyMarked(getAddress(node)->key)) //if node's key is marked then someone else has done the below steps for this thread
				{						
					struct node* nodeAddrWithPromoteFlagSet = setPromoteFlag(getAddress(node));
					struct node* CASoutput;
					CASoutput = getAddress(rnode)->lChild.compare_and_swap(nodeAddrWithPromoteFlagSet,NULL);
					if(CASoutput != NULL) //CAS failed
					{
						if(isPromoteFlagSet(CASoutput))
						{
							if(getAddress(CASoutput) != getAddress(node))
							{
								//restart primary seek. assert(node->secFlag == DONE)
								break; //start from primary seek
							}
							else
							{
								printf("ERROR");
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
								//if secondaryLastUnmarkedEdge does not exist, then help node->rChild
								continue; //restart secondary seek
							}
						}
						printf("CANT BE HERE");
					}
					else //CAS succeeded
					{
						btsOnPromoteFlag((struct node**)&getAddress(rnode)->rChild); //set promote flag on rnode->rChild using BTS
						getAddress(node)->key = setReplaceFlagInKey(getAddress(rnode)->key); //node's key changed from <0,kN> to <1,kRN>						
					}						
				}
				assert(isKeyMarked(getAddress(node)->key));
				if(!getAddress(node)->secDoneFlag)
				{
					if(!isPromoteFlagSet(getAddress(rnode)->lChild))
					{
						printf("pnode\t\t:%x\n",pnode);
						printf("node\t\t:%x\n",node);
						printf("nlChild\t\t:%x\n",(struct node*) getAddress(node)->lChild);
						printf("nrChild\t\t:%x\n",(struct node*) getAddress(node)->rChild);
						printf("rnode\t\t:%x\n",rnode);
						printf("rnlChild\t\t:%x\n",(struct node*) getAddress(rnode)->lChild);
						printf("rnrChild\t\t:%x\n",(struct node*) getAddress(rnode)->rChild);
						assert(false);
					}
					assert(isPromoteFlagSet(getAddress(rnode)->rChild));
				}

				if(!isSplCase)
				{
					//try removing secondary node
					if(getAddress(rpnode)->lChild.compare_and_swap(getAddress(getAddress(rnode)->rChild),getAddress(rnode)) == getAddress(rnode))
					{
						getAddress(node)->secDoneFlag = true;
					}
					else
					{
						//help operation at secondaryLastUnmarkedEdge
						//if secondaryLastUnmarkedEdge does not exist, then override CASinvariant and help node->rChild
						continue; //restart secondary seek
					}
				}
				else
				{
					assert(getAddress(node)->rChild.compare_and_swap(setDeleteFlag(getAddress(rnode)->rChild),rnode) == nrChild);
					getAddress(node)->secDoneFlag = true;
				}
				if(getAddress(node)->secDoneFlag)
				{
					struct node* newNode = (struct node*) malloc(sizeof(struct node));
					newNode->key = getKey(getAddress(node)->key);
					newNode->lChild = getAddress(getAddress(node)->lChild);
					newNode->rChild = getAddress(getAddress(node)->rChild);
					newNode->secDoneFlag=false;
					struct node* PCASoutput;
					if(isLeft)
					{
						PCASoutput = getAddress(pnode)->lChild.compare_and_swap(newNode,getAddress(node));
						if( PCASoutput == getAddress(node))
						{
							tData->successfulDeletes++;
							tData->complexDeleteCount++;
							return(true);
						}
					}
					else
					{
						PCASoutput = getAddress(pnode)->rChild.compare_and_swap(newNode,getAddress(node));
						if( PCASoutput == getAddress(node))
						{
							tData->successfulDeletes++;
							tData->complexDeleteCount++;
							return(true);
						}
					}
					if(getAddress(PCASoutput) != getAddress(node))
					{
						return(true);
					}
					else
					{
						//CAS has failed coz the edge is marked. Help at lastUnmarkedEdge.
						//If lastUnmarkedEdge is (pnode,node) then restart
						assert(getAddress(node)->secDoneFlag);
						break; //start from primary seek
					}								
				}
			}
		}
		else //simple delete
		{
			isSimpleDelete = true;
		}
		if(isSimpleDelete)
		{
			if(isLeft)
			{
				if(isNull(getAddress(node)->lChild))
				{
					if(getAddress(pnode)->lChild.compare_and_swap(getAddress(getAddress(node)->rChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
				else
				{
					if(getAddress(pnode)->lChild.compare_and_swap(getAddress(getAddress(node)->lChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
			}
			else
			{
				if(isNull(getAddress(node)->lChild))
				{
					if(getAddress(pnode)->rChild.compare_and_swap(getAddress(getAddress(node)->rChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
				else
				{
					if(getAddress(pnode)->rChild.compare_and_swap(getAddress(getAddress(node)->lChild),getAddress(node)) == getAddress(node))
					{
						tData->successfulDeletes++;
						tData->simpleDeleteCount++;
						return(true);
					}
				}
			}
			//if lastUnmarkedEdge is (pnode,node) then restart, else help			
		}
	}
}

void help()
{
	
}

void nodeCount(struct node* node)
{
	//assert(isDeleteFlagSet(node));
	assert(isPromoteFlagSet(node));
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