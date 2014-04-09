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

static inline unsigned long getKey(unsigned long key)
{
	return (key & 0x7FFFFFFFUL);
}

static inline struct node* getAddress(struct node* p)
{
	return (struct node*)((uintptr_t) p & ~((uintptr_t) 3));
}

static inline bool getReplaceFlagInKey(unsigned long key)
{
	return ((key & 80000000UL) == 80000000UL);
}

static inline unsigned long setReplaceFlagInKey(unsigned long key)
{
	return (key | 80000000UL);
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
      if(getKey(lastRightNode->key) == lastRightKey)
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
      if(pnode->lChild.compare_and_swap(replaceNode,node) == node)
      {
        tData->isNewNodeAvailable = false;
        tData->successfulInserts++;
        return(true);
      }
    }
    else //right case
    {
      if(pnode->rChild.compare_and_swap(replaceNode,node) == node)
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

bool remove(struct threadArgs* tData, unsigned long deleteKey)
{
	return false;
}

/*
bool remove(struct threadArgs* tData, unsigned long deleteKey)
{
  struct node* pnode;
  struct node* node;
  unsigned long lastRightKey;
  struct node* lastRightNode;
  bool keyFound;
  tData->deleteCount++;
  
  while(true)
  {
    while(true)
    {
      pnode = grandParentHead;
      node = parentHead;
      lastRightKey = node->key;
      lastRightNode = node;
      keyFound = false;

      while( !isNull(node) ) //Loop until a child of a leaf node which is null is reached
      {
				node = getAddress(node);
        if(deleteKey < node->key)
        {
          pnode = node;
          node = node->lChild;
        }
        else if (deleteKey > node->key)
        {
          lastRightKey = node->key;
          lastRightNode = node;
          pnode = node;
          node = node->rChild;
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
      if(lastRightNode->key == lastRightKey)
      {
        break;
      }
    }
		if(getAddress(pnode->lChild) == node)
		{
			node = pnode->lChild;
		}
		else if(getAddress(pnode->rChild) == node)
		{
			node = pnode->rChild;
		}
    if(!isNull(node))
    {
      if(deleteKey == getAddress(node)->key)
      {
				if(pnode->lChild == node) //left case
				{
					struct node* nlChild = getAddress(node)->lChild;
					struct node* nrChild = getAddress(node)->rChild;
					if(isNull(nlChild)) //simple delete
					{
						if(lockEdge(pnode, node, 1))
						{
							if(lockEdge(node, nlChild, 1))
							{
								if(lockEdge(node, nrChild, 0)) //3 locks are obtained
								{
									if(getAddress(node)->key == deleteKey)
									{
										if(isNull(nrChild)) //00 case
										{
											pnode->lChild = setNullBit(pnode->lChild);
											unlockLChild(pnode);
											tData->successfulDeletes++;
											tData->simpleDeleteCount++;
											return(true);
										}
										else //01 case
										{
											pnode->lChild = getAddress(nrChild);
											tData->successfulDeletes++;
											tData->simpleDeleteCount++;
											return(true);
										}
									}
									else
									{
										unlockLChild(pnode);
										unlockLChild(node);
										unlockRChild(node);
									}
								}
								else
								{
									unlockLChild(pnode);
									unlockLChild(node);
								}
							}
							else
							{
								unlockLChild(pnode);
							}
						}
					}
					else if(isNull(nrChild)) //10 case
					{
						if(lockEdge(pnode, node, 1))
						{
							if(lockEdge(node, nlChild, 1))
							{
								if(lockEdge(node, nrChild, 0)) //3 locks are obtained
								{
									if(getAddress(node)->key == deleteKey)
									{
										pnode->lChild = getAddress(nlChild);
										tData->successfulDeletes++;
										tData->simpleDeleteCount++;
										return(true);	
									}
									else
									{
										unlockLChild(pnode);
										unlockLChild(node);
										unlockRChild(node);
									}
								}
								else
								{
									unlockLChild(pnode);
									unlockLChild(node);
								}
							}
							else
							{
								unlockLChild(pnode);
							}
						}
					}
					else //11 case
					{
						struct node* rpnode;
						struct node* rnode;
						struct node* lcrnode;
						struct node* rcrnode;
						rpnode = node;
						rnode = nrChild;
						lcrnode = getAddress(rnode)->lChild;
						rcrnode = getAddress(rnode)->rChild;
						if(!isNull(lcrnode))
						{
							while(!isNull(lcrnode))
							{
								rpnode = rnode;
								rnode = lcrnode;
								lcrnode = getAddress(lcrnode)->lChild;
							}
							rcrnode = getAddress(rnode)->rChild;
							if(lockEdge(rpnode, rnode, 1))
							{
                if(lockEdge(rnode, lcrnode, 1))
                {
								  if(lockEdge(rnode, rcrnode, 0))
								  {
										if(lockEdge(node,nrChild,0))
										{
											if(getAddress(node)->key == deleteKey)
											{
												getAddress(node)->key = getAddress(rnode)->key;
												if(isNull(rcrnode))
												{
													getAddress(rpnode)->lChild = setNullBit(getAddress(rpnode)->lChild);
													unlockLChild(rpnode);
												}
												else
												{
													getAddress(rpnode)->lChild = getAddress(rcrnode);
												}
												unlockRChild(node);
												tData->successfulDeletes++;
												tData->complexDeleteCount++;
												return(true);
											}
											else
											{
												unlockLChild(rpnode);
												unlockLChild(rnode);
												unlockRChild(rnode);
												unlockRChild(node);
											}
										}
										else
										{
											unlockLChild(rpnode);
											unlockLChild(rnode);
											unlockRChild(rnode);
										}
								  }
								  else
								  {
								  	unlockLChild(rpnode);
                    unlockLChild(rnode);
								  }
                }
                else
                {
								  unlockLChild(rpnode);
                }
							}
						}
						else //spl case in complex delete
						{
              if(lockEdge(rnode, lcrnode, 1))
              {
							  if(lockEdge(rnode, rcrnode, 0))
							  {
									if(lockEdge(node,nrChild,0))
									{
										if(getAddress(node)->key == deleteKey)
										{
											getAddress(node)->key = getAddress(rnode)->key;
											if(isNull(rcrnode))
											{
												getAddress(node)->rChild = setNullBit(getAddress(node)->rChild);
												unlockRChild(node);
											}
											else
											{
												getAddress(node)->rChild = getAddress(rcrnode);
											}
											tData->successfulDeletes++;
											tData->complexDeleteCount++;
											return(true);
										}
										else
										{
											unlockLChild(rnode);
											unlockRChild(rnode);
											unlockRChild(node);
										}
									}
									else
									{
										unlockLChild(rnode);
										unlockRChild(rnode);
									}
							  }
							  else
							  {
                  unlockLChild(rnode);
							  }
              }
						}
					}
				}
				else //right case
				{
					struct node* nlChild = getAddress(node)->lChild;
					struct node* nrChild = getAddress(node)->rChild;
					if(isNull(nlChild)) //simple delete
					{
						if(lockEdge(pnode, node, 0))
						{
							if(lockEdge(node, nlChild, 1))
							{
								if(lockEdge(node, nrChild, 0)) //3 locks are obtained
								{
									if(getAddress(node)->key == deleteKey)
									{
										if(isNull(nrChild)) //00 case
										{
											pnode->rChild = setNullBit(pnode->rChild);
											unlockRChild(pnode);
											tData->successfulDeletes++;
											tData->simpleDeleteCount++;
											return(true);
										}
										else //01 case
										{
											pnode->rChild = getAddress(nrChild);
											tData->successfulDeletes++;
											tData->simpleDeleteCount++;
											return(true);
										}
									}
									else
									{
										unlockRChild(pnode);
										unlockLChild(node);
										unlockRChild(node);
									}
								}
								else
								{
									unlockRChild(pnode);
									unlockLChild(node);
								}
							}
							else
							{
								unlockRChild(pnode);
							}
						}
					}
					else if(isNull(nrChild)) //10 case
					{
						if(lockEdge(pnode, node, 0))
						{
							if(lockEdge(node, nlChild, 1))
							{
								if(lockEdge(node, nrChild, 0)) //3 locks are obtained
								{
									if(getAddress(node)->key == deleteKey)
									{
										pnode->rChild = getAddress(nlChild);
										tData->successfulDeletes++;
										tData->simpleDeleteCount++;
										return(true);	
									}
									else
									{
										unlockRChild(pnode);
										unlockLChild(node);
										unlockRChild(node);
									}
								}
								else
								{
									unlockRChild(pnode);
									unlockLChild(node);
								}
							}
							else
							{
								unlockRChild(pnode);
							}
						}
					}
					else //11 case
					{
						struct node* rpnode;
						struct node* rnode;
						struct node* lcrnode;
						struct node* rcrnode;
						rpnode = node;
						rnode = nrChild;
						lcrnode = getAddress(rnode)->lChild;
						rcrnode = getAddress(rnode)->rChild;
						if(!isNull(lcrnode))
						{
							while(!isNull(lcrnode))
							{
								rpnode = rnode;
								rnode = lcrnode;
								lcrnode = getAddress(lcrnode)->lChild;
							}
							rcrnode = getAddress(rnode)->rChild;
							if(lockEdge(rpnode, rnode, 1))
							{
                if(lockEdge(rnode, lcrnode, 1))
                {
								  if(lockEdge(rnode, rcrnode, 0))
								  {
										if(lockEdge(node,nrChild,0))
										{
											if(getAddress(node)->key == deleteKey)
											{
												getAddress(node)->key = getAddress(rnode)->key;
												if(isNull(rcrnode))
												{
													getAddress(rpnode)->lChild = setNullBit(getAddress(rpnode)->lChild);
													unlockLChild(rpnode);
												}
												else
												{
													getAddress(rpnode)->lChild = getAddress(rcrnode);
												}
												unlockRChild(node);
												tData->successfulDeletes++;
												tData->complexDeleteCount++;
												return(true);
											}
											else
											{
												unlockLChild(rpnode);
												unlockLChild(rnode);
												unlockRChild(rnode);
												unlockRChild(node);
											}
										}
										else
										{
											unlockLChild(rpnode);
											unlockLChild(rnode);
											unlockRChild(rnode);
										}
								  }
								  else
								  {
								  	unlockLChild(rpnode);
                    unlockLChild(rnode);
								  }
                }
                else
                {
								  unlockLChild(rpnode);
                }
							}
						}
						else //spl case in complex delete
						{
              if(lockEdge(rnode, lcrnode, 1))
              {
							  if(lockEdge(rnode, rcrnode, 0))
							  {
									if(lockEdge(node,nrChild,0))
									{
										if(getAddress(node)->key == deleteKey)
										{
											getAddress(node)->key = getAddress(rnode)->key;
											if(isNull(rcrnode))
											{
												getAddress(node)->rChild = setNullBit(getAddress(node)->rChild);
												unlockRChild(node);
											}
											else
											{
												getAddress(node)->rChild = getAddress(rcrnode);
											}
											tData->successfulDeletes++;
											tData->complexDeleteCount++;
											return(true);
										}
										else
										{
											unlockLChild(rnode);
											unlockRChild(rnode);
											unlockRChild(node);
										}
									}
									else
									{
										unlockLChild(rnode);
										unlockRChild(rnode);
									}
							  }
							  else
							  {
                  unlockLChild(rnode);
							  }
              }
						}
					}
				}
      }
      else //if(deleteKey == node->key)
      {
        tData->unsuccessfulDeletes++;
        return(false);
      }
    }
    else // for if(node != NULL)
    {
      tData->unsuccessfulDeletes++;
      return(false);
    } 
    tData->deleteRetries++;
  } // end of infinite while loop
}
*/
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

