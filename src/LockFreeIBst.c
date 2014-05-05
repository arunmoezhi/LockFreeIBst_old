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
		return true;
	}
	else
	{
		t->unsuccessfulReads++;
		return false;
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
			return false;
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
	return true;
}