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
#define NULL_BIT 4
#define DELETE_BIT 2
#define PROMOTE_BIT 1
#define DELETE_AND_PROMOTE_BIT 3
#define ADDRESS_MASK 7
#define KEY_MASK 0x80000000
#define K 2
#define LEFT 0
#define RIGHT 1
#define SIMPLE false
#define COMPLEX true
#define INJECTION 0
#define DISCOVERY 1
#define CLEANUP 2
#define PRIMARY false
#define SECONDARY true
#define STATS_ON
#define DEBUG_ON

struct node
{
  unsigned long key;									          //format <replaceFlag, address>
  tbb::atomic<struct node*> childrenArray[K];		//format <address, deleteFlag, promoteFlag>
	bool secDoneFlag;
};

struct primarySeekRecord
{
	struct node* pnode;
  struct node* node;
	struct node* lastUnmarkedPnode;
	struct node* lastUnmarkedNode;
};

struct secSeekRecord
{
	struct node* rpnode;
  struct node* rnode;
	struct node* rnodeChildrenArray[K];
	struct node* secLastUnmarkedPnode;
	struct node* secLastUnmarkedNode;
};

struct opRecord
{
	unsigned long key;
	struct secSeekRecord* ssr;
	struct node* storedNode;
	struct node* lastUnmarkedPnode; 	//this is populated from primarySeekRecord or secSeekRecord based on nodeType
	struct node* lastUnmarkedNode; 		//this is populated from primarySeekRecord or secSeekRecord based on nodeType
	bool opType; 											//SIMPLE or COMPLEX
	int mode; 												//INJECTION or DISCOVERY or CLEANUP (for help it can be either DISCOVERY or CLEANUP) - default value is INJECTION
	bool nodeType; 										//PRIMARY or SECONDARY
	struct node* freshNode;					 //used to install a fresh copy during CLEANUP
	bool isFreshNodeAvailable;
};

struct threadArgs
{
  int threadId;
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
	struct primarySeekRecord* psr;
	struct opRecord* opr;
};

void createHeadNodes();
unsigned long lookup(struct threadArgs*, unsigned long);
bool insert(struct threadArgs*, unsigned long);
bool remove(struct threadArgs*, unsigned long);
unsigned long size();
void printKeys();
bool isValidTree();
void help();
void simpleHelp(struct node*, struct node*, struct threadArgs*);