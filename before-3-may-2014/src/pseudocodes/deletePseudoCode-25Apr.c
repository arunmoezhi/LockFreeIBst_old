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

delete pseudocode
mode := INJECTION
isSimpleDelete := false	
while(true)
{
	if(primarySeekForDelete(deleteSeekRecord, deleteKey) returned false)
		return false
	else
		use the values of pnode, node and lastUnmarkedEdge from the deleteSeekRecord

	if(mode == INJECTION)
	{
		locally store the value of lChild of node
		try CAS(node->lChild,<nlChildAddr,x,0,0>,<nlChildAddr,x,1,0>)
		if CAS FAILED
			help 
			continue from top of primary seek's while loop
		if CAS SUCCEEDED
			mode := DISCOVERY
			storedNode := node
			nlChild := node->lChild
	}
	if(storedNode != node) //Someone removed the node for me. So DONE
	set "deleteFlag" on node's rChild using BTS
	nrChild := node->rChild
	if complex delete //if both lChild and rChild are nonNULL
	{
		while(true) //secondary seek
		{
			update the local value of rChild of node
			if(nRChild != NULL) //check the null bit to determine NULL values
				isSplCase = secondarySeekForDelete(node,nRChild,secDeleteSeekRecord)
				use the values of secondary node,its lchild,its rchild, its parent,SeclastUnmarkedEdge from secDeleteSeekRecord 
			else
				if(node key is marked)
					set secDoneFlag to true
					break from while loop
				else
					set isSimpleDelete flag to true
					break from while loop
			if node key is unmarked
			{
				try CAS(rnode->lChild,<anyAddress,1,0,0>,<nodeAddr,1,0,1>)
				if CAS failed
				{
					if promoteFlag is set
						if address does not match with node's address
							assert(node->secFlag == DONE)
							return
					else
						if address != NULL // (check the null bit to determine NULL values) restart secondary seek
							continue from top of secondary seek's while loop
						else
							if(primary node's done flag not set)
							{
								
								if(primary node's key is not marked and rnode->lChild's deleteFlag is set)
									help operation at secondaryLastUnmarkedEdge
									if secondaryLastUnmarkedEdge does not exist, then help node->rChild //simplehelp(node,nrChild)
								else
									continue									
							}
							break from while loop
				}
				set promote flag on rnode->rChild using BTS
				locally store the value of rnode->rChild
				promote key using a simple write. Node's key changes from <0,kN> to <1,kRN>
			} //end if node key is unmarked
			if(promote flag set in lcrnode && addr(lcrnode) == addr(node)) //lcrnode denotes left child of rnode
			{
				if(!isSplCase)
				{
					if(rnodeRchild != NULL)
						try CAS(rpnodeLChild,<rnode,0,0,0>,<rnodeRChild,0,0,0>) //remove secondary node
					else
						try CAS(rpnodeLChild,<rnode,0,0,0>,<rnode,1,0,0>) //remove secondary node
					if CAS FAILED, help operation at secondaryLastUnmarkedEdge
						if secondaryLastUnmarkedEdge doesn't exist, override CASinvariant and help node->rChild  //simplehelp(node,nrChild)
						continue from top of secondary seek's while loop									
					if CAS SUCCEEDED, set node->secDoneFlag to true
				}
				else
				{
					if(rnodeRchild != NULL)
						try CAS(nodeRChild,<rnode,0,1,0>,<rnodeRChild,0,1,0> //no problem if CAS fails
					else
						try CAS(nodeRChild,<rnode,0,1,0>,<rnode,1,1,0> //no problem if CAS fails
					set node->secDoneFlag to true
				}
			} //end if promote flag is set in lcrnode
		} //end secondary seek while
		if(!isSimpleDelete) //install fresh copy
		{
			oldNodeAddr = address of node
			create a fresh copy of node
			newNodeKey as <0,kRN>
			newNodeLChild as <node's lChildAddr,0,0,0>
			newNodeRChild = <node's rChildAddr,x,0,0>	//copy null flag
			
			while(true)
			{					
				if(newNodeKey < pnode->key)
					try CAS(pnode->lChild,<node,0,0,0>,<newNode,0,0,0>)
				else
					try CAS(pnode->rChild,<node,0,0,0>,<newNode,0,0,0>)
				if CAS SUCCEEDED then DONE
				if CAS FAILED
					if oldNodeAddr != address returned by CAS
						then someone helped me install a fresh copy.so DONE
					else 
						CAS has failed coz the edge is marked.
						if lastUnmarkedEdge is not (pnode,node) then help						
						do primarySeekForDelete(newNodeKey) //restart primary seek with new key	
						if the new key is not found then someone has installed a fresh copy. So done						
						if key is found and newNodeAddr != oldNodeAddr then someone has installed a fresh copy. So done
			} //end while			
		} // end install fresh copy
	} //end if complex delete
	else //simple delete
		set isSimpleDelete to true
	if(isSimpleDelete)
	{
		if both l & r child of node are NULL
			try CAS(pnode->l/r Child,<node,0,0,0>,<node,1,0,0>)
		else
			try CAS(pnode->l/r Child,<node,0,0,0>,<nonNullChild,0,0,0>)
		if CAS SUCCEEDED, then DONE
		if CAS FAILED
			if lastUnmarkedEdge is NOT (pnode,node) help			
	}
} //end main while

//simplehelp(node,node's rChild)
simplehelp(pnode,node)
assert(pnode's secDoneFlag not set)
set delete flag on node->rChild using BTS
locally store the lChild and rChild values of node
if node->secDoneFlag is set
		create a fresh copy of node
		try CAS(pnode->rChild,<node,0,1,0>,<newNode,0,1,0>)
else //secDoneFlag is not set
	if(key not marked & one of the child pointers is not NULL)
		if both l & r child of node are NULL
			try CAS(pnode->rchild,<node,0,1,0>,<node,1,1,0>)
		else
			try CAS(pnode->rchild,<node,0,1,0>,<nonNullChild,0,1,0>)