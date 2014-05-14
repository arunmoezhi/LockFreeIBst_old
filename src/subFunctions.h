void seek(struct tArgs*, unsigned long, struct seekRecord*);
void updateModeAndType(struct tArgs*, struct stateRecord*);
bool inject(struct tArgs*, struct stateRecord*);
void findSmallest(struct tArgs*, struct node*, struct node*, struct seekRecord*);
void findAndMarkSuccessor(struct tArgs*, struct stateRecord*);
void removeSuccessor(struct tArgs*, struct stateRecord*);
bool cleanup(struct tArgs*, struct stateRecord*, bool);
void shallowHelp(struct tArgs*, struct node*, struct node*);
void deepHelp(struct tArgs*, struct node*, struct node*);

void seek(struct tArgs* t,unsigned long key, struct seekRecord* s)
{
	struct node* prev;
	struct node* curr;
	struct node* lastUParent;
	struct node* lastUNode;
	struct node* lastRNode;
	struct node* address;
	unsigned long lastRKey;
	unsigned long cKey;
	int which;
	bool done;
	bool n;
	bool d;
	bool p;
	
	while(true)
	{	
		//initialize all variables use in traversal
		lastUParent = R; lastUNode = S;
		prev = R; curr = S;
		lastRNode = R; lastRKey = INF_R;
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
				//null flag set; reached a leaf node
				if(getKey(lastRNode->markAndKey) == lastRKey)
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
				lastRNode = curr;
				lastRKey = cKey;
			}
			//traverse the next edge
			prev = curr; curr = address;
			//determine if the most recent edge traversed is marked
			if(!(d || p))
			{
				//keep track of the two end points of the edge
				lastUParent = prev;
				lastUNode = curr;
			}
		}
		if(done)
		{
			//initialize the seek record and return
			s->node = curr;
			s->parent = prev;
			s->lastUParent = lastUParent;
			s->lastUNode = lastUNode;
			return;			
		}
	}
}

void updateModeAndType(struct tArgs* t,struct stateRecord* state)
{
	struct node* node;
	bool m;
	bool lN;
	bool rN;
	//retrieve the address from the state record
	node = state->node;
	#ifdef PRINT
		if(!isDFlagSet(node->child[LEFT]))
		{
			printf("%x %ld %x %x %d %d %d %ld\n",node,node->markAndKey,(struct node*)node->child[0],(struct node*)node->child[1],node->ownerId,state->mode,state->type,state->key);
			assert(false);
		}
	#endif
	//mark the right edge if unmarked
	if(!isDFlagSet(node->child[RIGHT]))
	{
		BTS((struct node*) &(node->child[RIGHT]), BTS_D_FLAG);
	}
	#ifdef PRINT
		assert(isDFlagSet(node->child[RIGHT]));	
	#endif
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

bool inject(struct tArgs* t,struct stateRecord* state)
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
			#ifdef PRINT
				if(!isDFlagSet(node->child[LEFT]))
				{
					printf("%x %x %x %x\n",node,addressWithNBit,addressWithDFlagSet,(struct node*)node->child[LEFT]);
					assert(false);
				}
				if(!isDFlagSet(state->node->child[LEFT]))
				{
					printf("%x %x %x %x\n",node,addressWithNBit,addressWithDFlagSet,(struct node*)node->child[LEFT]);
					assert(false);
				}
			#endif
			break;
		}
		//retry from the beginning of the while loop
	}
	//mark the right edge; update the operation mode and type
	updateModeAndType(t,state);
	#ifdef PRINT
		snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d I %ld ",t->tId,state->key);
		t->bIdx +=7;
	#endif
	return(true);	
}

void findSmallest(struct tArgs* t,struct node* node, struct node* right, struct seekRecord* s)
{
	struct node* prev;
	struct node* curr;
	struct node* lastUParent;
	struct node* lastUNode;
	struct node* left;
	struct node* temp;
	bool n;
	bool d;
	bool p;
	//find the node with the smallest key in the subtree rooted at the right child
	//initialize the variables used in the traversal
	lastUParent = node; lastUNode = right;
	prev = node; curr = right;
	while(true)
	{
		temp = curr->child[LEFT];
		n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); left = getAddress(temp);
		if(n)
		{
			break;
		}
		//traverse the next edge
		prev = curr; curr = left;
		//determine if the most recently traversed edge is marked
		if(!(d || p))
		{
			//keep track of the two endpoints of the last unmarked edge
			lastUParent = prev;
			lastUNode = curr;
		}		
	}
	//initialize seek record and return	
	s->parent = prev;
	s->node = curr;
	s->lastUParent = lastUParent;
	s->lastUNode = lastUNode;
	return;
}

void findAndMarkSuccessor(struct tArgs* t, struct stateRecord* state)
{
	struct node* node;
	struct seekRecord* s;
	struct node* succNode;
	struct node* right;
	struct node* left;
	struct node* temp;
	struct node* lastUParent;
	struct node* lastUNode;
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
		findSmallest(t,node,right,s);
		if(getAddress(node->child[RIGHT]) != right)
		{
			//the root node of the right subtree has changed
			continue;
		}
		//retrieve the address from the seek record
		succNode = s->node;
		//obtain the information stored in the left edge
		temp = succNode->child[LEFT];
		n = isNull(temp); d = isDFlagSet(temp); p = isPFlagSet(temp); left = getAddress(temp);
		//read the mark flag of the key under deletion
		newM = isKeyMarked(node->markAndKey);
		if(newM)
		{
			//successor node has already been selected
			if(node->readyToReplace)
			{
				//the successor node has already been removed
				break;
			}
			if(p)
			{
				if(left == node)
				{
					//a successor node has already been selected
					break;
				}
				else
				{
					//the node found is a successor node for another delete operation
					node->readyToReplace = true;
					break;
				}
			}
			if(oldM)
			{
				//the successor node has already been removed
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
				//a successor node has already been selected
				break;
			}
			else
			{
				//the node found is a successor node for another delete operation
				node->readyToReplace = true;
				break;
			}
		}
		if(!n)
		{
			//the node found has since gained a left child
			continue;
		}
		if(d)
		{
			//the node found is undergoing deletion; need to help
			lastUParent = s->lastUParent;
			lastUNode = s->lastUNode;
			if(lastUParent == node)
			{
				//all edges from the node to its possible successor are marked
				shallowHelp(t,lastUNode,lastUParent);
			}
			else
			{
				deepHelp(t,lastUNode,lastUParent);
			}
		}
	}
	//update the operation mode and type
	updateModeAndType(t,state);
	return;
}

void removeSuccessor(struct tArgs* t, struct stateRecord* state)
{
	struct node* node;
	struct seekRecord* s;
	struct node* succNode;
	struct node* succParent;
	struct node* lastUParent;
	struct node* lastUNode;
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
	p = isPFlagSet(succNode->child[RIGHT]);
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
		n = isNull(temp); right = getAddress(temp);
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
				result = CAS(succParent,which,setDFlag(succNode),setDFlag(right));
			}
			else
			{
				result = CAS(succParent,which,succNode,right);
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
		lastUParent = s->lastUParent;
		lastUNode = s->lastUNode;
		if(lastUParent == node)
		{
			//all edges in the path from the node to its successor are marked
			shallowHelp(t,lastUNode,lastUParent);
		}
		else if(lastUNode != succNode)
		{
			//last unmarked edge is not incident on the successor
			deepHelp(t,lastUNode,lastUParent);
		}		
		temp = node->child[RIGHT];
		n = isNull(temp); right = getAddress(temp);
		if(n)
		{
			break;
		}
		findSmallest(t,node,right,s);
		if(s->node != succNode)
		{
			//the successor node has already been deleted
			break;
		}
	}
	node->readyToReplace = true;
	if(!isNull(state->parent))
	{
		updateModeAndType(t,state);
	}
}

bool cleanup(struct tArgs* t, struct stateRecord* state, bool dFlag)
{
	struct node* node;
	struct node* parent;
	struct node* newNode;
	struct node* left;
	struct node* right;
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
	//retrieve the addresses from the state record
	node = state->node;
	parent = state->parent;
	//determine which edge of the parent needs to be switched
	pKey = getKey(parent->markAndKey);
	nKey = getKey(node->markAndKey);
	pWhich = nKey<pKey ? LEFT:RIGHT;
	if(state->type == COMPLEX)
	{
		//replace the node with a new copy in which all the fields are unmarked
		newNode = (struct node*) malloc(sizeof(struct node));
		assert((uintptr_t) newNode%8==0);
		newNode->markAndKey = nKey;
		newNode->readyToReplace = false;
		left = getAddress(node->child[LEFT]);
		newNode->child[LEFT] = left;
		temp = node->child[RIGHT];
		n = isNull(temp); right = getAddress(temp);
		if(n)
		{
			newNode->child[RIGHT] = setNull(NULL);
		}
		else
		{
			newNode->child[RIGHT] = right;
		}
		newNode->ownerId = -1;
		//try to switch the edge at the parent
		if(dFlag)
		{
			result = CAS(parent,pWhich,setDFlag(node),setDFlag(newNode));
			if(result)
			{
				#ifdef PRINT
					snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C0 %ld %x %d %x %x\n",t->tId,state->key,parent,pWhich,setDFlag(node),setDFlag(newNode));
					t->bIdx +=37;
				#endif
			}
		}
		else
		{
			result = CAS(parent,pWhich,node,newNode);
			if(result)
			{
				#ifdef PRINT
					snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C1 %ld %x %d %x %x\n",t->tId,state->key,parent,pWhich,node,newNode);
					t->bIdx +=37;
				#endif
			}
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
				if(result)
				{
					#ifdef PRINT
						snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C2 %ld %x %d %x %x\n",t->tId,state->key,parent,pWhich,setDFlag(node),setNull(setDFlag(node)));
						t->bIdx +=37;
					#endif
				}
			}
			else
			{
				result = CAS(parent,pWhich,node,setNull(node));
				if(result)
				{
					#ifdef PRINT
						snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C3 %ld %x %d %x %x\n",t->tId,state->key,parent,pWhich,node,setNull(node));
						t->bIdx +=37;
					#endif
				}
			}			
		}
		else
		{
			if(dFlag)
			{
				result = CAS(parent,pWhich,setDFlag(node),setDFlag(address));
				if(result)
				{
					#ifdef PRINT
						snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C4 %ld %x %d %x %x\n",t->tId,state->key,parent,pWhich,setDFlag(node),setDFlag(address));
						t->bIdx +=37;
					#endif
				}
			}
			else
			{
				result = CAS(parent,pWhich,node,address);
				if(result)
				{
					#ifdef PRINT
						snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C5 %ld %x %d %x %x\n",t->tId,state->key,parent,pWhich,node,address);
						t->bIdx +=37;
					#endif
				}
			}
		}
	}
	if(result)
	{
		return(true);
	}
	//check if some other process has already switched the edge at the parent
	temp = parent->child[pWhich];
	n = isNull(temp); address = getAddress(temp);
	if(n || (address != node))
	{
		#ifdef PRINT
			snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d C6 %ld\n",t->tId,state->key);
			t->bIdx +=8;
		#endif
		//some other process has already switched the edge
		return(true);
	}
	else
	{
		return(false);
	}
}

void shallowHelp(struct tArgs* t, struct node* node, struct node* parent)
{
	struct stateRecord* state;
	//obtain new state record and initialize it
	state = (struct stateRecord*) malloc(sizeof(struct stateRecord));
	state->node = node;
	state->parent = parent;
	//mark the right edge if unmarked; also update the operation mode and type
	updateModeAndType(t,state);
	//if the operation mode is cleanup
	if(state->mode == CLEANUP)
	{
		cleanup(t,state,1);
	}
	else
	{
		deepHelp(t,node,parent);
	}
	return;
}

void deepHelp(struct tArgs* t,struct node* node, struct node* parent)
{
	return;
}