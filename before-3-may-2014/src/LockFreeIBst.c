#include"header.h"
#include"helper.h"

unsigned long lookup(struct threadArgs* t, unsigned long target)
{
	struct node* node;
	unsigned long nodeKey;
	struct node* lastRightNode;
	unsigned long lastRightKey;
	t->readCount++;
	
	while(true)
	{
		node = grandParentHead;
		nodeKey = getKey(getAddress(node)->key);
		lastRightNode = node;
		lastRightKey = getKey(getAddress(node)->key);
		
		while(!isNull(node))
		{
			nodeKey = getKey(getAddress(node)->key);
			if(target < nodeKey)
			{
				node = getAddress(node)->childrenArray[LEFT];
			}
			else if (target > nodeKey)
			{
				lastRightNode = node;
				lastRightKey = getKey(getAddress(lastRightNode)->key);
				node = getAddress(node)->childrenArray[RIGHT];
			}
			else
			{
				t->successfulReads++;
				return(1);
			}
		}
		if(getKey(getAddress(lastRightNode)->key) == lastRightKey)
		{
			t->unsuccessfulReads++;
			return(0);
		}
		t->readRetries++;
	}
}

bool insert(struct threadArgs* t, unsigned long insertKey)
{
	struct node* pnode;
	struct node* node;
	unsigned long nodeKey;
	struct node* replaceNode;
	struct node* lastRightNode;
	unsigned long lastRightKey;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
	int childIndex;
	t->insertCount++;
	
	while(true) //retry till the operation is completed
	{
		while(true) //retry till an injection point is found
		{
			pnode = grandParentHead;
			node = parentHead;
			replaceNode = NULL;
			lastRightNode = node;
			lastRightKey = getAddress(node)->key;
			lastUnmarkedPnode = pnode;
			lastUnmarkedNode = node;
			
			while(!isNull(node)) //locate the injection point
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
					node = getAddress(node)->childrenArray[LEFT];
				}
				else if (insertKey > nodeKey)
				{
					lastRightNode = node;
					lastRightKey = getKey(getAddress(lastRightNode)->key);
					pnode = node;
					node = getAddress(node)->childrenArray[RIGHT];
				}
				else
				{
					t->unsuccessfulInserts++;
					return(false);
				}
			}
			if(getKey(getAddress(lastRightNode)->key) == lastRightKey)
			{
				break;
			}
		}
		
		if(!t->isNewNodeAvailable) //reuse nodes
		{
			t->newNode = newLeafNode(insertKey);
			t->isNewNodeAvailable = true;
			replaceNode = t->newNode;
		}
		else
		{
			replaceNode = t->newNode;
			replaceNode->key = insertKey;
		}

		childIndex = getChildIndex(pnode,insertKey);
		
		if(getAddress(pnode)->childrenArray[childIndex].compare_and_swap(replaceNode,node) == node)
		{
			t->isNewNodeAvailable = false;
			t->successfulInserts++;
			return(true);
		}
		if(isNodeMarked(node))
		{
			help();
		}
		t->insertRetries++;
	}
}

bool primarySeekForDelete(struct primarySeekRecord* psr, unsigned long key)
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
	
		while( !isNull(node)) //Loop until a child of a leaf node which is null is reached
		{
			if(!isNodeMarked(node))
			{
				lastUnmarkedPnode = pnode;
				lastUnmarkedNode = node;
			}
			nodeKey = getKey(getAddress(node)->key);
			if(key < nodeKey)
			{
				pnode = node;
				node = getAddress(node)->childrenArray[LEFT];
			}
			else if (key > nodeKey)
			{         
				lastRightNode = node;
				lastRightKey = nodeKey;
				pnode = node;
				node = getAddress(node)->childrenArray[RIGHT];
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
	psr->pnode = pnode;
	psr->node = node;
	psr->lastUnmarkedPnode = lastUnmarkedPnode;
	psr->lastUnmarkedNode = lastUnmarkedNode;
	return(true);
}

bool secondarySeekForDelete(struct node* node, struct node* nRChild, struct opRecord* opr)
{
	struct node* rpnode;
	struct node* rnode;
	struct node* lcrnode;
	struct node* rcrnode;
	struct node* secLastUnmarkedPnode = NULL;
	struct node* secLastUnmarkedNode = NULL;
	bool isSplCase=false;
	
	rpnode = node;
	rnode = nRChild;
	lcrnode = getAddress(rnode)->childrenArray[LEFT];
	rcrnode = getAddress(rnode)->childrenArray[RIGHT];
	
	if(!isNull(lcrnode))	
	{
		while(!isNull(lcrnode))
		{
			if(!isNodeMarked(lcrnode))
			{
				secLastUnmarkedPnode = rnode;
				secLastUnmarkedNode = lcrnode;
			}
			rpnode = rnode;
			rnode = lcrnode;
			lcrnode = getAddress(lcrnode)->childrenArray[LEFT];
		}
		rcrnode = getAddress(rnode)->childrenArray[RIGHT];
	}
	else //spl case in complex delete
	{
		isSplCase=true;
	}

	opr->ssr->rpnode = rpnode;
	opr->ssr->rnode = rnode;
	opr->ssr->rnodeChildrenArray[LEFT] = lcrnode;
	opr->ssr->rnodeChildrenArray[RIGHT] = rcrnode;
	opr->ssr->secLastUnmarkedPnode = secLastUnmarkedPnode;
	opr->ssr->secLastUnmarkedNode = secLastUnmarkedNode;
	
	return(isSplCase);
}

bool doInjection(struct primarySeekRecord* psr, struct opRecord* opr)
{
	struct node* nlChild;
	struct node* nrChild;
	nlChild = getAddress(psr->node)->childrenArray[LEFT];
	if(getAddress(psr->node)->childrenArray[LEFT].compare_and_swap(setDeleteFlag(nlChild),unmarkNode(nlChild)) != unmarkNode(nlChild))
	{
		help();
		return(false);
	}
	else //CAS SUCCEEDED
	{
		opr->storedNode = psr->node;
		nlChild = getAddress(psr->node)->childrenArray[LEFT];
		btsOnDeleteFlag(getAddress(psr->node)->childrenArray[RIGHT]);
		nrChild = getAddress(psr->node)->childrenArray[RIGHT];
		if(isNull(nlChild))
		{
			opr->opType = SIMPLE;
		}
		else if(!isNull(nrChild))
		{
			opr->opType = COMPLEX;
		}
		else
		{
			if(isKeyMarked(getAddress(psr->node)->key))
			{
				opr->opType = COMPLEX;
			}
			else
			{
				opr->opType = SIMPLE;
			}
		}
		
		if(opr->opType == COMPLEX)
		{
			opr->mode = DISCOVERY;
		}
		else
		{
			opr->mode = CLEANUP;
		}
		return(true);
	}
}

bool doCleanup(struct primarySeekRecord* psr, struct opRecord* opr)
{
	return(false);
}

bool doDiscovery(struct primarySeekRecord* psr, struct opRecord* opr)
{
	struct node* nrChild;
	while(true)
	{
		nrChild = getAddress(psr->node)->childrenArray[RIGHT];
		if(isNull(nrChild))
		{
			if(!isKeyMarked(getAddress(psr->node)->key))
			{
				opr->opType = SIMPLE; opr->mode = CLEANUP; 
			}
		}
	}
}

bool remove(struct threadArgs* t, unsigned long key)
{
	t->opr->key = key;
	t->opr->storedNode = NULL;
	t->opr->lastUnmarkedPnode = NULL;
	t->opr->lastUnmarkedNode = NULL;
	t->opr->opType = SIMPLE;
	t->opr->mode = INJECTION;
	t->opr->nodeType = PRIMARY;
	t->opr->ssr = NULL;
	
	while(true)
	{
		if(!primarySeekForDelete(t->psr,t->opr->key)) //keyNotFound
		{
			if(t->opr->mode == INJECTION)
			{
				return(false);
			}
			else
			{
				return(true);
			}
		}
		
		if(t->opr->mode == INJECTION)
		{
			if(!doInjection(t->psr, t->opr))
			{
				continue; //injection failed. So restart primary seek
			}
		}
		
		if(t->opr->storedNode != t->psr->node)
		{
			return(true);
		}
		
		if(t->opr->mode == DISCOVERY)
		{
			//returns false if primary node already removed
			//returns true is secondary node is removed
			if(!doDiscovery(t->psr,t->opr))
			{
				return(true); //primary node successfully removed
			}
		}
		
		if(t->opr->mode == CLEANUP)
		{
			doCleanup(t->psr, t->opr);
			return(true);
		}
	}	
}

void simpleHelp(struct node* pnode, struct node* node, struct threadArgs* t)
{
	return;
}

void help()
{
	return;
}