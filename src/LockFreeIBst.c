#include"header.h"
#include"helper.h"
#include"subFunctions.h"

bool search(struct tArgs* t, unsigned long key)
{
	struct node* node;
	unsigned long nKey;
	
	seek(key,t->mainSeekRecord);
	node = t->mainSeekRecord->node;
	nKey = getKey(node->markAndKey);
	if(nKey == key)
	{
		t->successfulReads++;
		return(true);
	}
	else
	{
		t->unsuccessfulReads++;
		return(false);
	}	
}

bool insert(struct tArgs* t, unsigned long key)
{
	struct node* node;
	struct node* newNode;
	struct node* address;
	unsigned long nKey;
	bool result;
	bool d;
	bool p;
	int which;
	
	while(true)
	{
		seek(key,t->mainSeekRecord);
		node = t->mainSeekRecord->node;
		nKey = getKey(node->markAndKey);
		if(nKey == key)
		{
			t->unsuccessfulInserts++;
			return(false);
		}
		//create a new node and initialize its fields
		if(!t->isNewNodeAvailable)
		{
			t->newNode = newLeafNode(key);
			t->isNewNodeAvailable = true;
		}
		newNode = t->newNode;
		newNode->markAndKey = key;
		which = key<nKey ? LEFT:RIGHT;
		address = node->child[which];
		result = CAS(node,which,setNull(address),newNode);
		if(result)
		{
			t->isNewNodeAvailable = false;
			t->successfulInserts++;
			return(true);
		}
		struct node* temp = node->child[which];
		d = isDFlagSet(temp); p = isPFlagSet(temp); address = getAddress(temp);
		if(!(d || p))
		{
			continue;
		}
		deepHelp(t->mainSeekRecord->freeNode,t->mainSeekRecord->freeParent);
	}
}

bool remove(struct tArgs* t, unsigned long key)
{
	struct node* node;
	struct node* parent;
	struct node* freeNode;
	struct node* freeParent;
	struct seekRecord* mainSeekRecord = t->mainSeekRecord;
	unsigned long nKey;
	bool needToHelp;
	bool result;
	//obtain a state record and initialize it
	struct stateRecord* state = t->state;
	state->mode = INJECTION;
	state->key = key;
	while(true)
	{
		seek(state->key,mainSeekRecord);
		node = mainSeekRecord->node;
		parent = mainSeekRecord->parent;
		nKey = getKey(node->markAndKey);
		if(state->key != nKey)
		{
			//the key does not exist in the tree
			if(state->mode == INJECTION)
			{
				return(false);
			}
			else
			{
				return(true);
			}
		}
		needToHelp = false;
		//perform appropriate action depending on the mode
		if(state->mode == INJECTION)
		{
			//store a reference to the node
			state->node = node;
			//attempt to inject the operation at the node
			result = inject(state);
			if(!result)
			{
				needToHelp = true;
			}
		}
		//mode would have changed if the operation was injected successfully
		if(state->mode != INJECTION)
		{
			//if the node found by the seek function is different from the one stored in state record, then return
			if(state->node != node)
			{
				return(true);
			}
			//update the parent information using the most recent seek
			state->parent = parent;
		}
		if(state->mode == DISCOVERY)
		{
			findAndMarkSuccessor(state);
		}
		if(state->mode == DISCOVERY)
		{
			removeSuccessor(state);
		}
		if(state->mode == CLEANUP)
		{
			result = cleanup(state,0);
			if(result)
			{
				return(true);
			}
			else
			{
				nKey = getKey(node->markAndKey);
				state->key = nKey;
			}
			if(mainSeekRecord->freeNode != node)
			{
				needToHelp = true;
			}
		}
		if(needToHelp)
		{
			freeNode = mainSeekRecord->freeNode;
			freeParent = mainSeekRecord->freeParent;
			deepHelp(freeNode,freeParent);
		}
	}
}