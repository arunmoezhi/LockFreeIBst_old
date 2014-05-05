void seek(unsigned long, struct seekRecord*);
void updateModeAndType(struct stateRecord*);
bool inject(struct stateRecord*);
bool findSmallest(struct node*, struct node*, struct seekRecord*);
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

bool findSmallest(struct node* node, struct node* right, struct seekRecord* s)
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
	
}

void removeSuccessor(struct stateRecord* state)
{
	
}

bool cleanup(struct stateRecord* state, bool dFlag)
{
	
}

void shallowHelp(struct node* node, struct node* parent)
{
	return;
}

void deepHelp(struct node* node, struct node* parent)
{
	return;
}