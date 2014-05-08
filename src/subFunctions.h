void seek(unsigned long, struct seekRecord*);
void updateModeAndType(struct stateRecord*);
bool inject(struct stateRecord*);
void findSmallest(struct node*, struct node*, struct seekRecord*);
void findAndMarkSuccessor(struct stateRecord*);
void removeSuccessor(struct stateRecord*);
bool cleanup(struct stateRecord*, bool);
void shallowHelp(struct node*, struct node*);
void deepHelp(struct node*, struct node*);

void seek(unsigned long key, struct seekRecord* s)
{
	struct node* prev;
	struct node* curr;
	struct node* freeParent;
	struct node* freeNode;
	struct node* rightNode;
	struct node* address;
	unsigned long rightKey;
	unsigned long cKey;
	int which;
	bool done;
	bool n;
	bool d;
	bool p;
	
	while(true)
	{	
		//initialize all variables use in traversal
		freeParent = R; freeNode = S;
		prev = R; curr = S;
		rightNode = R; rightKey = INF_R;
		done = false;
		
		while(true)
		{
			//read the key stored in the current node
			cKey = getKey(curr->markAndKey);
			if(key == cKey)
			{
				//key found; stop the traversal
				done = true;
				break;
			}
			which = key<cKey ? LEFT:RIGHT;
			struct node* temp = curr->child[which];
			n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); address = getAddress(temp);
			if(n)
			{
				//reached a leaf node
				if(getKey(rightNode->markAndKey) == rightKey)
				{
					//key stored in the node at which the last right edge was traversed has not changed
					done = true;
				}
				//stop the traversal and restart if not done
				break;
			}
			if(which == RIGHT)
			{
				//the next edge that will be traversed is the right edge; keep track of the current node and its key
				rightNode = curr;
				rightKey = cKey;
			}
			//traverse the next edge
			prev = curr;
			curr = address;
			//determine if the most recent edge traversed is marked
			if(!(d || p))
			{
				//keep trace of the two end points of the edge
				freeParent = prev;
				freeNode = curr;
			}
		}
		if(done)
		{
			//initialize the seek record and return
			s->node = curr;
			s->parent = prev;
			s->freeParent = freeParent;
			s->freeNode = freeNode;
			return;			
		}
	}
}

void updateModeAndType(struct stateRecord* state)
{
	struct node* node;
	bool m;
	bool lN;
	bool rN;
	//retrieve the address from the state record
	node = state->node;
	//mark the right edge if unmarked
	if(!isDFlagSet(node->child[RIGHT]))
	{
		BTS((struct node*) &(node->child[RIGHT]), BTS_D_FLAG);
	}
	//update the operation mode and type
	m = isKeyMarked(node->markAndKey);
	lN = isNull(node->child[LEFT]);
	rN = isNull(node->child[RIGHT]);
	if(lN || rN)
	{
		if(m)
		{
			node->readyToReplace = true;
			state->type = COMPLEX;
		}
		else
		{
			state->type = SIMPLE;
		}
		state->mode = CLEANUP;
	}
	else
	{
		state->type = COMPLEX;
		if(node->readyToReplace)
		{
			state->mode = CLEANUP;
		}
		else
		{
			state->mode = DISCOVERY;
		}
	}
	return;
}

bool inject(struct stateRecord* state)
{
	struct node* node;
	struct node* addressWithNBit;
	bool result;
	bool d;
	bool p;
	
	node = state->node;
	//try to set the delete flag on the left edge
	while(true)
	{
		//read the information stored in the left edge
		struct node* temp = node->child[LEFT];
		d = isDFlagSet(temp); p = isPFlagSet(temp); addressWithNBit = addressWithNullBit(temp);
		struct node* addressWithDFlagSet = setDFlag(addressWithNBit);
		if(d || p)
		{
			//the edge is already marked
			return(false);
		}
		result = CAS(node,LEFT,addressWithNBit,addressWithDFlagSet);
		if(result)
		{
			break;
		}
		//retry from the beginning of the while loop
	}
	//mark the right edge; update the operation mode and type
	updateModeAndType(state);
	return(true);	
}

void findSmallest(struct node* node, struct node* right, struct seekRecord* s)
{
	struct node* prev;
	struct node* curr;
	struct node* freeParent;
	struct node* freeNode;
	struct node* address;
	struct node* temp;
	bool n;
	bool d;
	bool p;
	//find the node with the smallest key in the subtree rooted at the right child
	//initialize the variables used in the traversal
	freeParent = node; freeNode = right;
	prev = node; curr = right;
	while(true)
	{
		temp = curr->child[LEFT];
		n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); address = getAddress(temp);
		if(n)
		{
			break;
		}
		//traverse the next edge
		prev = curr;
		curr = address;
		//determine if the most recently traversed edge is marked
		if(!(d || p))
		{
			//keep track of the two endpoints of the last unmarked edge
			freeParent = prev;
			freeNode = curr;
		}		
	}
	//initialize seek record and return	
	s->parent = prev;
	s->node = curr;
	s->freeParent = freeParent;
	s->freeNode = freeNode;
	return;
}

void findAndMarkSuccessor(struct stateRecord* state)
{
	struct node* node;
	struct seekRecord* s;
	struct node* succNode;
	struct node* right;
	struct node* left;
	struct node* temp;
	bool n;
	bool d;
	bool p;
	bool oldM;
	bool newM;
	bool result;
	//retrieve the addresses from the state record
	node = state->node;
	s = state->seekRecord;
	while(true)
	{
		//obtain the address of the right child
		temp = node->child[RIGHT];
		n = isNull(temp); right = getAddress(temp);
		if(n)
		{
			break;
		}
		oldM = isKeyMarked(node->markAndKey);
		//find the node with the smallest key in the right subtree
		findSmallest(node,right,s);
		if(getAddress(node->child[RIGHT]) != right)
		{
			//the root node of the right subtree has changed
			continue;
		}
		//retrieve the address from the seek record
		succNode = s->node;
		newM = isKeyMarked(node->markAndKey);
		temp = succNode->child[LEFT];
		n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); left = getAddress(temp);
		if(newM)
		{
			if(node->readyToReplace)
			{
				break;
			}
			if(p)
			{
				if(left == node)
				{
					break;
				}
				else
				{
					node->readyToReplace = true;
					break;
				}
			}
			if(oldM)
			{
				node->readyToReplace = true;
				break;
			}
			else
			{
				continue;
			}
		}
		//try to set the promote flag on the left edge
		result = CAS(succNode,LEFT,setNull(left),setPFlag(setNull(node)));
		if(result)
		{
			break;
		}
		//attempt to mark the edge failed; recover from the failure and retry if needed
		struct node* temp = succNode->child[LEFT];
		n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); left = getAddress(temp);
		if(p)
		{
			if(left == node)
			{
				break;
			}
			else
			{
				node->readyToReplace = true;
				break;
			}
		}
		if(!n)
		{
			continue;
		}
		if(d)
		{
			if(s->freeParent == node)
			{
				shallowHelp(s->freeNode,s->freeParent);
			}
			else
			{
				deepHelp(s->freeNode,s->freeParent);
			}
		}
	}
	//update the operation mode and type
	updateModeAndType(state);
	return;
}

void removeSuccessor(struct stateRecord* state)
{
	struct node* node;
	struct seekRecord* s;
	struct node* succNode;
	struct node* succParent;
	struct node* freeParent;
	struct node* freeNode;
	struct node* address;
	struct node* temp;
	struct node* right;
	bool n;
	bool d;
	bool p;
	bool result;
	bool dFlag;
	bool which;
	//retrieve addresses from the state record
	node = state->node;
	s = state->seekRecord;
	//extract information about the successor node
	succNode = s->node;
	//mark the right edge if unmarked
	temp = succNode->child[RIGHT];
	p = isPFlagSet(temp); address = getAddress(temp);
	if(!p)
	{
		//set the promote flag on the right edge
		BTS((struct node*) &(succNode->child[RIGHT]), BTS_P_FLAG);		
	}
	//promote the key
	node->markAndKey = setReplaceFlagInKey(succNode->markAndKey);
	while(true)
	{
		//retrieve parent of the successor node from the seek record
		succParent = s->parent;
		//check if the successor is the right child of the node itself
		if(succParent == node)
		{
			dFlag = true; which = RIGHT;
		}
		else
		{
			dFlag = false; which = LEFT;
		}
		struct node* temp = succNode->child[RIGHT];
		n = isNull(temp); address = getAddress(temp);
		if(n)
		{
			//only set the null flag; do not change the address
			if(dFlag)
			{
				result = CAS(succParent,which,setDFlag(succNode),setNull(setDFlag(succNode)));
			}
			else
			{
				result = CAS(succParent,which,succNode,setNull(succNode));
			}
		}
		else
		{
			if(dFlag)
			{
				result = CAS(succParent,which,setDFlag(succNode),setDFlag(address));
			}
			else
			{
				result = CAS(succParent,which,succNode,address);
			}
		}
		if(result || dFlag)
		{
			break;
		}
		temp = succParent->child[which];
		n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); address = getAddress(temp);
		if(n || (address != succNode))
		{
			break;
		}
		if(succParent == node)
		{
			shallowHelp(succNode,succParent);
		}
		else
		{
			//locate the edge among which to help
			freeParent = s->freeParent;
			freeNode = s->freeNode;
			if(freeNode != succNode)
			{
				deepHelp(freeNode,freeParent);
			}
		}
		temp = node->child[RIGHT];
		n = isNull(temp); right = getAddress(temp);
		if(n)
		{
			break;
		}
		findSmallest(node,right,s);
		if(s->node != succNode)
		{
			//the successor node has already been deleted
			break;
		}
	}
	if(!isNull(state->parent))
	{
		updateModeAndType(state);
	}
}

bool cleanup(struct stateRecord* state, bool dFlag)
{
	struct node* node;
	struct node* parent;
	struct node* newNode;
	struct node* address;
	struct node* temp;
	unsigned long pKey;
	unsigned long nKey;
	int pWhich;
	int nWhich;
	bool n;
	bool d;
	bool p;
	bool result;
	//retrieve the address from the state record
	node = state->node;
	parent = state->parent;
	//determine which edge of the parent needs to be switched
	pKey = parent->markAndKey;
	nKey = node->markAndKey;
	pWhich = nKey<pKey ? LEFT:RIGHT;
	if(state->type == COMPLEX)
	{
		//replace the node with a new copy in which all the fields are unmarked
		newNode = (struct node*) malloc(sizeof(struct node));
		newNode->markAndKey = nKey;
		temp = node->child[LEFT];
		n = isNull(temp); address = getAddress(temp);
		if(n)
		{
			newNode->child[LEFT] = setNull(NULL);
		}
		else
		{
			newNode->child[LEFT] = address;
		}
		temp = node->child[RIGHT];
		n = isNull(temp); address = getAddress(temp);
		if(n)
		{
			newNode->child[RIGHT] = setNull(NULL);
		}
		else
		{
			newNode->child[RIGHT] = address;
		}
		//try to switch the edge at the parent
		if(dFlag)
		{
			result = CAS(parent,pWhich,setDFlag(node),setDFlag(newNode));
		}
		else
		{
			result = CAS(parent,pWhich,node,newNode);
		}	
	}
	else
	{
		//remove the node
		//determine to which grand child will the edge at the parent be switched
		if(isNull(node->child[LEFT]))
		{
			nWhich = RIGHT;
		}
		else
		{
			nWhich = LEFT;
		}
		temp = node->child[nWhich];
		n = isNull(temp); address = getAddress(temp);
		if(n)
		{
			//set the null flag only; do not change the address
			if(dFlag)
			{
				result = CAS(parent,pWhich,setDFlag(node),setNull(setDFlag(node)));
			}
			else
			{
				result = CAS(parent,pWhich,node,setNull(node));
			}			
		}
		else
		{
			if(dFlag)
			{
				result = CAS(parent,pWhich,setDFlag(node),setDFlag(address));
			}
			else
			{
				result = CAS(parent,pWhich,node,address);
			}
		}
	}
	if(result)
	{
		return(true);
	}
	//check if some other process has already switched the edge at the parent
	temp = parent->child[pWhich];
	n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); address = getAddress(temp);
	if(n || (address != node))
	{
		//some other process has already switched the edge
		return(true);
	}
	else
	{
		return(false);
	}
}

void shallowHelp(struct node* node, struct node* parent)
{
	struct stateRecord* state;
	//obtain new state record and initialize it
	state = (struct stateRecord*) malloc(sizeof(struct stateRecord));
	state->node = node;
	state->parent = parent;
	//mark the right edge if unmarked; also update the operation mode and type
	updateModeAndType(state);
	//if the operation mode is cleanup
	if(state->mode == CLEANUP)
	{
		cleanup(state,1);
	}
	else
	{
		deepHelp(node,parent);
	}
	return;
}

void deepHelp(struct node* node, struct node* parent)
{
	return;
}