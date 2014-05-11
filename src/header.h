#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdbool.h>
#include<math.h>
#include<time.h>
#include<stdint.h>
#include<tbb/atomic.h>
#include<gsl/gsl_rng.h>
#include<gsl/gsl_randist.h>
#include<assert.h>
#define MAX_KEY 0x7FFFFFFF
#define INF_R 0x0
#define INF_S 0x7FFFFFFE
#define NULL_BIT 4
#define DELETE_BIT 2
#define PROMOTE_BIT 1
#define DELETE_AND_PROMOTE_BIT 3
#define BTS_P_FLAG 0
#define BTS_D_FLAG 1
#define ADDRESS_MASK 7
#define ADDRESS_WITH_DFLAG 5
#define KEY_MASK 0x80000000
#define K 2
#define LEFT 0
#define RIGHT 1
#define DEBUG_ON
#define PRINT

typedef enum {INJECTION, DISCOVERY, CLEANUP} Mode;
typedef enum {SIMPLE, COMPLEX} Type;

struct node
{
	unsigned long markAndKey;							//format <markFlag,address>
	tbb::atomic<struct node*> child[K];		//format <address,NullBit,DeleteFlag,PromoteFlag>
	bool readyToReplace;
	int ownerId;
};

struct seekRecord
{
	struct node* node;
	struct node* parent;
	struct node* lastUParent;
	struct node* lastUNode;
};

struct stateRecord
{
	struct node* node;
	struct node* parent;
	unsigned long key;
	Mode mode;
	Type type;
	struct seekRecord* seekRecord;
};

struct tArgs
{
	int tId;
	unsigned long lseed;
	unsigned long readCount;
	unsigned long successfulReads;
	unsigned long unsuccessfulReads;
	unsigned long readRetries;
	unsigned long insertCount;
	unsigned long successfulInserts;
	unsigned long unsuccessfulInserts;
	unsigned long insertRetries;
	unsigned long deleteCount;
	unsigned long successfulDeletes;
	unsigned long unsuccessfulDeletes;
	unsigned long deleteRetries;
	unsigned long simpleDeleteCount;
	unsigned long complexDeleteCount;
	struct node* newNode;
	bool isNewNodeAvailable;
	struct seekRecord* mySeekRecord;
	struct stateRecord* myState;
	#ifdef PRINT
	char buffer[1048576];
	int bIdx;
	#endif
};

void createHeadNodes();
bool search(struct tArgs*, unsigned long);
bool insert(struct tArgs*, unsigned long);
bool remove(struct tArgs*, unsigned long);
unsigned long size();
void printKeys();
bool isValidTree();