#include"header.h"
#include"helper.h"
#include"subFunctions.h"

bool search(struct tArgs* t, unsigned long key)
{
	struct node* node;
	unsigned long nKey;
	
	node = seekForSearch(t,key);
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

bool insert0(struct tArgs* t, unsigned long key)
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
		seekForInsert(t,key,t->mySeekRecord);
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
		address = getAddress(node->child[which]);
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
		deepHelp(t,t->mySeekRecord->lastUNode,t->mySeekRecord->lastUParent);
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
		seekForInsert(t,key,t->mySeekRecord);
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
		//address = getAddress(node->child[which]);
		address = t->mySeekRecord->injectionPoint;
		result = CAS(node,which,setNull(address),newNode);
		if(result)
		{
			t->isNewNodeAvailable = false;
			t->successfulInserts++;
			return(true);
		}
		t->insertRetries++;
		struct node* temp = node->child[which];
		d = isDFlagSet(temp); p = isPFlagSet(temp); address = getAddress(temp);
		if(!(d || p))
		{
			continue;
		}
		deepHelp(t,t->mySeekRecord->lastUNode,t->mySeekRecord->lastUParent);
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
	//initialize my seek record
	mySeekRecord->node = NULL;
	mySeekRecord->parent = NULL;
	mySeekRecord->lastUParent = NULL;
	mySeekRecord->lastUNode = NULL;
	//initialize the state record
	struct stateRecord* myState = t->myState;
	myState->node = NULL;
	myState->parent = NULL;
	myState->type = SIMPLE;
	myState->mode = INJECTION;
	myState->key = key;
	while(true)
	{
		seekForDelete(t,myState->key,mySeekRecord);
		node = mySeekRecord->node;
		parent = mySeekRecord->parent;
		nKey = getKey(node->markAndKey);
		if(myState->key != nKey)
		{
			//the key does not exist in the tree
			if(myState->mode == INJECTION)
			{
				t->unsuccessfulDeletes++;
				return(false);
			}
			else
			{
				t->successfulDeletes++;
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
			result = inject(t,myState);
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
				#ifdef PRINT
					t->bIdx +=snprintf(t->buffer+t->bIdx,sizeof(t->buffer)-t->bIdx,"t%d H2 %ld ",t->tId,key);
				#endif
				t->successfulDeletes++;
				return(true);
			}
			//update the parent information using the most recent seek
			myState->parent = parent;
		}
		if(myState->mode == DISCOVERY)
		{
			findAndMarkSuccessor(t,myState);
		}
		if(myState->mode == DISCOVERY)
		{
			removeSuccessor(t,myState);
		}
		if(myState->mode == CLEANUP)
		{
			result = cleanup(t,myState,0);
			if(result)
			{
				t->successfulDeletes++;
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
			deepHelp(t,lastUNode,lastUParent);
		}
		t->deleteRetries++;
	}
}