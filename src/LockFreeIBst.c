#include"header.h"
#include"helper.h"
#include"subFunctions.h"

bool search(struct tArgs* t, unsigned long key)
{
	struct node* node;
	unsigned long nKey;
	
	seek(key,t->mySeekRecord);
	node = t->mySeekRecord->node;
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
		seek(key,t->mySeekRecord);
		node = t->mySeekRecord->node;
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
		deepHelp(t->mySeekRecord->lastUNode,t->mySeekRecord->lastUParent);
	}
}

bool remove(struct tArgs* t, unsigned long key)
{
	struct node* node;
	struct node* parent;
	struct node* lastUNode;
	struct node* lastUParent;
	struct seekRecord* mySeekRecord = t->mySeekRecord;
	unsigned long nKey;
	bool needToHelp;
	bool result;
	//initialize the state record
	struct stateRecord* myState = t->myState;
	myState->mode = INJECTION;
	myState->key = key;
	while(true)
	{
		seek(myState->key,mySeekRecord);
		node = mySeekRecord->node;
		parent = mySeekRecord->parent;
		nKey = getKey(node->markAndKey);
		if(myState->key != nKey)
		{
			//the key does not exist in the tree
			if(myState->mode == INJECTION)
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
		if(myState->mode == INJECTION)
		{
			//store a reference to the node
			myState->node = node;
			//attempt to inject the operation at the node
			result = inject(myState);
			if(!result)
			{
				needToHelp = true;
			}
		}
		//mode would have changed if the operation was injected successfully
		if(myState->mode != INJECTION)
		{
			//if the node found by the seek function is different from the one stored in state record, then return
			if(myState->node != node)
			{
				return(true);
			}
			//update the parent information using the most recent seek
			myState->parent = parent;
		}
		if(myState->mode == DISCOVERY)
		{
			findAndMarkSuccessor(myState);
		}
		if(myState->mode == DISCOVERY)
		{
			removeSuccessor(myState);
		}
		if(myState->mode == CLEANUP)
		{
			result = cleanup(myState,0);
			if(result)
			{
				return(true);
			}
			else
			{
				nKey = getKey(node->markAndKey);
				myState->key = nKey;
			}
			//help only if the helpee node is different from the node of interest
			if(mySeekRecord->lastUNode != node)
			{
				needToHelp = true;
			}
		}
		if(needToHelp)
		{
			lastUNode = mySeekRecord->lastUNode;
			lastUParent = mySeekRecord->lastUParent;
			deepHelp(lastUNode,lastUParent);
		}
	}
}