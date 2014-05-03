struct node
{
  unsigned long key;									          //format <replaceFlag, address>
  tbb::atomic<struct node*> childrenArray[K];		//format <address, deleteFlag, promoteFlag>
	bool secDoneFlag;
};

struct primarySeekRecord
{
	struct node* pnode;
  struct node* node;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
};

struct secondarySeekRecord
{
	struct node* rpnode;
  struct node* rnode;
	struct node* rnodeChildrenArray[K];
	struct node* secondaryLastUnmarkedPnode;
	struct node* secondaryLastUnmarkedNode;
};

struct opRecord
{
	struct node* lastUnmarkedPnode; //this is populated from primarySeekRecord or secondarySeekRecord based on nodeType
	struct node* lastUnmarkedNode; //this is populated from primarySeekRecord or secondarySeekRecord based on nodeType
	bool opType; //SIMPLE or COMPLEX
	int mode; //INJECTION or DISCOVERY or CLEANUP (for help it can be either DISCOVERY or CLEANUP) - default value is INJECTION
	bool nodeType; //PRIMARY or SECONDARY
	struct node* secondarySeekRecord
	struct node* freshNode; //used to install a fresh copy during CLEANUP
	bool isFreshNodeAvailable;
}

boolean primarySeekForDelete(primarySeekRecord, deleteKey)
{
	while(true)
	{
		loop until a null node is reached OR deleteKey is found
		if keyfound
		{
			update primarySeekRecord with pnode,node,lastUnmarkedEdge
			return true
		}
		else
		{
			if lastRightNode's key has not changed
				return false
			else
				restart primary seek
		}
	}//end while
}
//if special case secondary seek returns true
boolean secondarySeekForDelete(node,nRChild,opRecord)
{
	if special case //null bit set on nRChild's lChild
		return true
	else
		keep going left until the secondary node is located //secondary node's lchild should be NULL
		once the secondary node is located, populate the secondarySeekRecord
		with secondary node,its left & right child, its parent and the lastSecondaryUnmarkedEdge
		return false
}

delete(key)
{
	initialize primarySeekRecord and opRecord
	while(true)
	{
		isKeyFound = primarySeekForDelete(primarySeekRecord, key)
		if(!isKeyFound)
		{
			if(mode == INJECTION) return(false); else return(true);
		}		
		if(mode == INJECTION)
		{
			if(!doInjection(primarySeekRecord, opRecord))
			{
				continue //restart primary seek
			}
		}	
		if(storedNode != node) then return(true);		
		if(mode == DISCOVERY)
		{
			//returns false if primary node already removed
			//returns true is secondary node is removed
			if(!doDiscovery(primarySeekRecord, opRecord)) 
			{
				return(true) //node successfully removed
			}
		}		
		if(mode == CLEANUP)
		{
			doCleanup(primarySeekRecord, opRecord, key);
			return(true);
		}
	}//end while
}

bool doInjection(primarySeekRecord, opRecord)
{
	nlChild := node->childrenArray[LEFT]
	try CAS(node->childrenArray[LEFT],<nlChildAddr,x,0,0>,<nlChildAddr,x,1,0>)
	if CAS FAILED
	{
		help and return(false);
	}
	else  //CAS SUCCEEDED
	{
		storedNode := node
		nlChild := node->childrenArray[LEFT] //reread nlChild value after CAS
		nrChild := node->childrenArray[RIGHT]
		set "deleteFlag" on node's rChild using BTS
		nrChild := node->childrenArray[RIGHT] //reread nrChild value after BTS
		/******************************/
		if(nlChild == NULL)
				type := SIMPLE
		else if (nrChild != NULL)
				type := COMPLEX
		else
				if isKeyMarked(node) then type:=COMPLEX; else type:=SIMPLE
		/******************************/
		if(type == COMPLEX) then mode:=DISCOVERY; else mode:=CLEANUP
		return(true);		
	}
}

bool doDiscovery(primarySeekRecord, opRecord)
{
	while(true)
	{
		nrChild := node->childrenArray[RIGHT];
		if(nrChild == NULL)
		{
			if(!isKeyMarked(node))
					type := SIMPLE; mode := CLEANUP; return(true);
			else
					secDoneFlag := true; mode := CLEANUP; key := node->key; return(true);
		}		
		isSplCase = secondarySeekForDelete(node,nRChild,opRecord);	
		if(nrChild == node->childrenArray[RIGHT])
		{
			if(!isKeyMarked(node))
			{
				switch(doSecondaryInjection(opRecord))
				{
						case 0: (secondaryInjection unsuccessful) continue;
						case 1: (secondaryInjection is succesful); break;
						case 2: (secDoneFlag already set) return(true); 
						case 4: (node already removed) return(false);			
				}
			}
		
			if(secondary node found has promote flag set & address stored matches with primary node)
			{
				if(doSecondaryCleanup(isSplCase,opRecord))   // NEERAJ: CHECK THIS
				{
					secDoneFlag :=true; mode :=CLEANUP; key := node->key; return(true);
				}
			}
			else
			{
				secDoneFlag := true; mode := CLEANUP; key = node->key; return(true);
			}	
		}
	}//end while
}

int doSecondaryInjection(opRecord)
{
	try CAS(rnode->childrenArray[LEFT],<anyAddress,1,0,0>,<nodeAddr,1,0,1>)
	if CAS failed
	{
		/******************************/
		if promoteFlag is set
		{
			if address does not match with node address //assert(node->secDoneFlag)
				return(4) //node already removed
		}
		else
		{
			if address != NULL // (check the null bit to determine NULL values) restart secondary seek
			{	
				return(0) //secondaryInjection unsuccessful
			}
			else
			{
				if(primary node key is not marked)
				{					
					if(rnode->childrenArray[LEFT] deleteFlag is set)
					{
						help operation at secondaryLastUnmarkedEdge
						if secondaryLastUnmarkedEdge does not exist, then help node->childrenArray[RIGHT] //simplehelp(node,nrChild)
					}
					return(0); //secondaryInjection unsuccessful								
				}
				else //secondary node already deleted; may need to set the doneFlag
				{
					secDoneFlag := true; mode := CLEANUP; key = node->key;
					return(2); //secDoneFlag already set
				}
			}
		}
		/******************************/
	}
	set promote flag on rnode->childrenArray[RIGHT] using BTS
	locally store the value of rnode->childrenArray[RIGHT]
	promote key using a simple write. Node's key changes from <0,kN> to <1,kRN>
	return(1); //secondaryInjection is succesful
}

bool doSecondaryCleanup(isSplCase,opRecord)
{
	if(!isSplCase)
	{
		if(rnodeRchild != NULL)
			try CAS(rpnode->childrenArray[LEFT],<rnode,0,0,0>,<rnodeRChild,0,0,0>) //remove secondary node
		else
			try CAS(rpnode->childrenArray[LEFT],<rnode,0,0,0>,<rnode,1,0,0>) //remove secondary node
			
		if CAS FAILED, help operation at secondaryLastUnmarkedEdge
			if secondaryLastUnmarkedEdge doesn't exist, override CASinvariant and help node->rChild  //simplehelp(node,nrChild)
			return(false);								
		if CAS SUCCEEDED, return(true);
	}
	else
	{
		/******************************/
		if(rnodeRchild != NULL)
			try CAS(node->childrenArray[RIGHT],<rnode,0,1,0>,<rnodeRChild,0,1,0> //no problem if CAS fails
		else
			try CAS(node->childrenArray[RIGHT],<rnode,0,1,0>,<rnode,1,1,0> //no problem if CAS fails
		/******************************/
		return(true);
	}
}

bool doCleanup(primarySeekRecord, opRecord, key)
{
	if(type == COMPLEX)
	{
		createOrReuseFreshNode(opRecord, node, key);
	}
	
	childIndex := getChildIndex(pnode,node);
	if(type == SIMPLE)
	{
		if both l & r child of node are NULL
			try CAS(pnode->childrenArray[childIndex],<node,0,0,0>,<node,1,0,0>)
		else
			try CAS(pnode->childrenArray[childIndex],<node,0,0,0>,<nonNullChild,0,0,0>)
		
		if CAS SUCCEEDED return(true); 
		else 
		{
			if storedNode != address returned by CAS
				then someone helped me remove the node. return(true);
			else 
				CAS has failed coz the edge is marked.
				if lastUnmarkedEdge is not (pnode,node) then help and return false;			
		}
	}
	else
	{		
		try CAS(pnode->childrenArray[childIndex],<node,0,0,0>,<newNode,0,0,0>)
		if CAS SUCCEEDED isFreshNodeAvailable := FALSE; return(true);
		else
		{
			if storedNode != address returned by CAS
				then someone helped me install a fresh copy. return(true);
			else 
				CAS has failed coz the edge is marked.
				if lastUnmarkedEdge is not (pnode,node) then help and return(false);				
		}	
	}
}

void createOrReuseFreshNode(opRecord, node, key)
{
	if(!isFreshNodeAvailable)
	{
		freshNode : = create newNode
		freshNodeKey as <0,kRN>
		freshNodeLChild as <node's lChildAddr,0,0,0>
		freshNodeRChild = <node's rChildAddr,x,0,0>	//copy null flag	
		isFreshNodeAvailable := TRUE
	}
	else
	{
		freshNodeKey as <0,kRN>
		freshNodeLChild as <node's lChildAddr,0,0,0>
		freshNodeRChild = <node's rChildAddr,x,0,0>	//copy null flag	
	}
}
int getChildIndex(pnode,node)
{
	if(node->key < pnode->key)
		return LEFT;
	else
		return RIGHT;
}