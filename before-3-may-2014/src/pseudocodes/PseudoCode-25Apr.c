boolean primarySeekForDelete(deleteSeekRecord, deleteKey)
{
	while(true)
	{
		loop until a null node is reached OR deleteKey is found
		if keyfound
		{
			update deleteSeekRecord with pnode,node,lastUnmarkedEdge
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
boolean secondarySeekForDelete(node,nRChild,secDeleteSeekRecord)
{
	if special case //null bit set on nRChild's lChild
		return true
	else
		keep going left until the secondary node is located //secondary node's lchild should be NULL
		once the secondary node is located, populate the secDeleteSeekRecord
		with secondary node,its left & right child, its parent and the lastSecondaryUnmarkedEdge
		return false
}

delete(key)
{
	while(true)
	{
		isKeyFound = primarySeekForDelete(deleteSeekRecord, key)
		if(!isKeyFound)
		{
			if(mode == INJECTION) return(false); else return(true);
		}		
		if(mode == INJECTION)
		{
			if(!doInjection(deleteSeekRecord, key))
			{
				continue //restart primary seek
			}
		}	
		if(storedNode != node) then return(true);		
		if(mode == DISCOVERY)
		{
			if(!doDiscovery()) //returns false if primary node already removed. returns true is secondary node is removed
			{
				return(true) //node successfully removed
			}
		}		
		if(mode == CLEANUP)
		{
			doCleanup(deleteSeekRecord, key);
			return(true);
		}
	}//end while
}

bool doInjection(deleteSeekRecord, key)
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

bool doDiscovery()
{
	while(true)
	{
		nrChild := node->childrenArray[RIGHT];
		if(nrChild == NULL)
		{
			if(!isKeyMarked(node))
					type := SIMPLE; return(true);
			else
					secDoneFlag := true; mode := CLEANUP; key := node->key; return(true);
		}		
		isSplCase = secondarySeekForDelete(node,nRChild,secDeleteSeekRecord);	
		if(!isKeyMarked(node))
		{
			switch(doSecondaryInjection(secDeleteSeekRecord))
			{
					case 0: (secondaryInjection unsuccessful) continue;
					case 1: (secondaryInjection is succesful) subMode := SECONDARY_CLEANUP;
					case 2: (secDoneFlag already set) return(true); 
					case 4: (node already removed) return(false);			
			}
		}
		else
		{
			subMode := SECONDARY_CLEANUP
		}
		
		/******************************/
		if(subMode == SECONDARY_CLEANUP)
		{
			if(secondary node found has promote flag set & address stored matches with primary node)
			{
				if(doSecondaryCleanup(isSplCase,secDeleteSeekRecord) == 0)
				{
					secDoneFlag :=true; mode :=CLEANUP; key := node->key; return(true);
				}
			}
			else
			{
				secDoneFlag := true; mode := CLEANUP; key = node->key; return(true);
			}
		}		
		/******************************/
	}//end while
}

int doSecondaryInjection(secDeleteSeekRecord)
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
				if(primary node secDoneFlag not set)
				{					
					if(primary node key is not marked and rnode->childrenArray[LEFT] deleteFlag is set)
					{
						help operation at secondaryLastUnmarkedEdge
						if secondaryLastUnmarkedEdge does not exist, then help node->childrenArray[RIGHT] //simplehelp(node,nrChild)
					}
					return(0); //secondaryInjection unsuccessful								
				}
				else //primary node secDoneFlag is set
				{
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

bool doSecondaryCleanup(isSplCase,secDeleteSeekRecord)
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

void doCleanup(deleteSeekRecord, key)
{
	if(type == COMPLEX)
	{
		oldNodeAddr = address of node
		create a fresh copy of node
		newNodeKey as <0,kRN>
		newNodeLChild as <node's lChildAddr,0,0,0>
		newNodeRChild = <node's rChildAddr,x,0,0>	//copy null flag	
	}
	while(true)
	{
		childIndex := getChildIndex(pnode,node);
		if(type == SIMPLE)
		{
			if both l & r child of node are NULL
				try CAS(pnode->childrenArray[childIndex],<node,0,0,0>,<node,1,0,0>)
			else
				try CAS(pnode->childrenArray[childIndex],<node,0,0,0>,<nonNullChild,0,0,0>)
			if CAS SUCCEEDED return(); else help;
		}
		else
		{		
			try CAS(pnode->childrenArray[childIndex],<node,0,0,0>,<newNode,0,0,0>)
			if CAS SUCCEEDED return();
			else
			{
				if oldNodeAddr != address returned by CAS
					then someone helped me install a fresh copy.return();
				else 
					CAS has failed coz the edge is marked.
					if lastUnmarkedEdge is not (pnode,node) then help						
					do primarySeekForDelete(newNodeKey) //do primary seek with new key	
					if the new key is not found then someone has installed a fresh copy. return();						
					if key is found and newNodeAddr != oldNodeAddr then someone has installed a fresh copy. return();
			}
		}	
	}//end while
}

int getChildIndex(pnode,node)
{
	if(node->key < pnode->key)
		return LEFT;
	else
		return RIGHT;
}