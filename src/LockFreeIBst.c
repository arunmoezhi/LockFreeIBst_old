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
				node = getAddress(node);
				nodeKey = getKey(node->key);
        if(insertKey < nodeKey)
        {
          pnode = node;
          node = node->lChild;
        }
        else if (insertKey > nodeKey)
        {
					lastRightNode = node;
          lastRightKey = nodeKey;
          pnode = node;
          node = node->rChild;
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
    if(insertKey < getKey(pnode->key)) //left case
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
	bool keyFound=false;

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
	return(true);
}

bool secondarySeekForDelete(struct node* node, struct sDelSeekRecord* sdsr)
{
  struct node* rpnode;
	struct node* rnode;
	struct node* lcrnode;
	struct node* rcrnode;
	struct node* secondaryLastUnmarkedPnode = NULL;
	struct node* secondaryLastUnmarkedNode = NULL;
	bool isSplCase=false;
	
	rpnode = node;
	rnode = getAddress(node)->rChild;
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
	unsigned long lastRightKey;
  struct node* lastRightNode;
	unsigned long nodeKey;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
	struct node* storedNode;
	bool CLEANUP = false;
	bool RESTART = false;
	bool isLeft = true;
	bool isSimpleDelete=false;
	tData->deleteCount++;
	
	while(true)
	{
		RESTART = false;
		if(!primarySeekForDelete(tData->dsr, deleteKey))
		{
			tData->unsuccessfulDeletes++;
			return(false);
		}
		pnode = tData->dsr->pnode;
		node = tData->dsr->node;
		lastUnmarkedPnode = tData->dsr->lastUnmarkedPnode;
		lastUnmarkedNode = tData->dsr->lastUnmarkedNode;

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
			RESTART = true;
		}
		if(!RESTART)
		{
			if(!CLEANUP)
			{
				struct node* nlChild = getAddress(node)->lChild;
				struct node* nlChildWithDFlagSet = setDeleteFlag(nlChild);
				printf("%x\n",nlChild);
				printf("%x\n",nlChildWithDFlagSet);
				if(getAddress(node)->lChild.compare_and_swap(nlChildWithDFlagSet, getAddress(nlChild)) != getAddress(nlChild)) //setting DFlag on node's lChild
				{
					help(); //CAS failed so help
					RESTART = true;
				}
				else //CAS succeeded
				{					
					CLEANUP = true;
					storedNode = node;
				}
			}

			if(!RESTART)
			{
				if(storedNode != node) //Someone else removed the node for me
				{
					return(true);
				}
				
				printf("%x\n",(struct node*) getAddress(node)->rChild);
				btsOnDeleteFlag((struct node**)&getAddress(node)->rChild);
				printf("%x\n",(struct node*) getAddress(node)->rChild);
				//examine which case applies
				if(!isNull(getAddress(node)->lChild) && !isNull(getAddress(node)->rChild)) //possible complex delete
				{
					struct node* rpnode;
					struct node* rnode;
					struct node* lcrnode;
					struct node* rcrnode;
					struct node* secondaryLastUnmarkedPnode;
					struct node* secondaryLastUnmarkedNode;
					bool isSplCase;
					bool assumeCASsucceeded = false;
					bool SECONDARY_RESTART = false;
					while(true)
					{
						isSplCase = secondarySeekForDelete(node, tData->sdsr);
						rpnode = tData->sdsr->rpnode;
						rnode = tData->sdsr->rnode;
						lcrnode = tData->sdsr->lcrnode;
						rcrnode = tData->sdsr->rcrnode;
						secondaryLastUnmarkedPnode = tData->sdsr->secondaryLastUnmarkedPnode;
						secondaryLastUnmarkedNode = tData->sdsr->secondaryLastUnmarkedNode;
						if(!isKeyMarked(getAddress(node)->key)) //if node's key is marked then someone else has done the below steps for this thread
						{
							struct node* nodeAddrWithPromoteFlagSet = setPromoteFlag(getAddress(node));
							struct node* CASoutput;
							CASoutput = getAddress(rnode)->lChild.compare_and_swap(nodeAddrWithPromoteFlagSet,NULL);
							if(CASoutput != NULL) //CAS failed
							{
								if(isPromoteFlagSet(CASoutput))
								{
									if(getAddress(CASoutput) == getAddress(node))
									{
										assumeCASsucceeded = true;
									}
									else
									{
										//restart primary seek. assert(node->secFlag == DONE)
										RESTART = true;
										break; //start from primary seek
									}
								}
								else
								{
									if(!isNull(CASoutput))
									{
										//restart secondary seek
										SECONDARY_RESTART = true;
									}
									else
									{
										//assert(rnode->lChild's delete flag is set)
										//help operation at secondaryLastUnmarkedEdge
										//if secondaryLastUnmarkedEdge does not exist, then help node->rChild
										RESTART = true;
										break; //start from primary seek
									}
								}
							}
							else //CAS succeeded
							{
								assumeCASsucceeded = true;
							}
							if(assumeCASsucceeded)
							{
								printf("%x\n",(struct node*) getAddress(rnode)->rChild);
								btsOnPromoteFlag((struct node**)&getAddress(rnode)->rChild); //set promote flag on rnode->rChild using BTS
								printf("%x\n",(struct node*) getAddress(rnode)->rChild);		
								printf("%lu\n",getKey(getAddress(node)->key));
								getAddress(node)->key = setReplaceFlagInKey(getAddress(rnode)->key); //node's key changed from <0,kN> to <1,kRN>
								printf("%lu\n",getKey(getAddress(node)->key));
							}	
						}

						if(!SECONDARY_RESTART)
						{
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
									RESTART = true;
									break; //start from primary seek
								}
							}
							if(!(getAddress(node)->rChild == NULL && !(getAddress(node)->secDoneFlag)))
							{
								printf("1\n");
								struct node* newNode = (struct node*) malloc(sizeof(struct node));
								printf("newNode Addr: %x\n",newNode);
								newNode->key = getKey(getAddress(node)->key);
								newNode->lChild = getAddress(getAddress(node)->lChild);
								if(isSplCase)
								{
									newNode->rChild = getAddress(getAddress(rnode)->rChild);
								}
								else
								{
									newNode->rChild = getAddress(getAddress(node)->rChild);
								}
								struct node* PCASoutput;
								if(isLeft)
								{
									printf("2l\n");
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
									printf("2r\n");
									printf("%x\n",(struct node*)(getAddress(pnode)->rChild));
									printf("%x\n",getAddress(getAddress(pnode)->rChild)->key);
									PCASoutput = getAddress(pnode)->rChild.compare_and_swap(newNode,getAddress(node));
									printf("%x\n",(struct node*)(getAddress(pnode)->rChild));
									printf("%x\n",getAddress(getAddress(pnode)->rChild)->key);
									if( PCASoutput == getAddress(node))
									{
										printf("success\n");
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
									RESTART = true;
									break; //start from primary seek
								}								
							}
							else
							{
								isSimpleDelete = true;
								break;
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
					printf("in simple delete");
					if(isLeft)
					{
						printf(" left case");
						if(isNull(getAddress(node)->lChild))
						{
							printf(" lChild is NULL\n");
							if(getAddress(pnode)->lChild.compare_and_swap(getAddress(getAddress(node)->rChild),getAddress(node)) == getAddress(node))
							{
								tData->successfulDeletes++;
								tData->simpleDeleteCount++;
								return(true);
							}
							else
							{
								//if lastUnmarkedEdge is (pnode,node) then restart
								//else help
								RESTART = true;
								printf("restarting in simple delete left case lChild is NULL");
							}
						}
						else
						{
							printf(" rChild is NULL\n");
							if(getAddress(pnode)->lChild.compare_and_swap(getAddress(getAddress(node)->lChild),getAddress(node)) == getAddress(node))
							{
								tData->successfulDeletes++;
								tData->simpleDeleteCount++;
								return(true);
							}
							else
							{
								//if lastUnmarkedEdge is (pnode,node) then restart
								//else help
								RESTART = true;
								printf("restarting in simple delete left case rChild is NULL");
							}
						}
					}
					else
					{
						printf(" right case");
						if(isNull(getAddress(node)->lChild))
						{
							printf(" lChild is NULL\n");
							if(getAddress(pnode)->rChild.compare_and_swap(getAddress(getAddress(node)->rChild),getAddress(node)) == getAddress(node))
							{
								tData->successfulDeletes++;
								tData->simpleDeleteCount++;
								return(true);
							}
							else
							{
								//if lastUnmarkedEdge is (pnode,node) then restart
								//else help
								printf("restarting in simple delete right case lChild is NULL");
								RESTART = true;
							}
						}
						else
						{
							printf(" rChild is NULL\n");
							if(getAddress(pnode)->rChild.compare_and_swap(getAddress(getAddress(node)->lChild),getAddress(node)) == getAddress(node))
							{
								tData->successfulDeletes++;
								tData->simpleDeleteCount++;
								return(true);
							}
							else
							{
								//if lastUnmarkedEdge is (pnode,node) then restart
								//else help
								RESTART = true;
								printf("restarting in simple delete right case rChild is NULL");
							}
						}
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

