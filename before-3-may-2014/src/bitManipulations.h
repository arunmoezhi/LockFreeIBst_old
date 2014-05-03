static inline bool isNull(struct node* p)
{
	return ((uintptr_t) p & NULL_BIT) != 0;
}

static inline bool isDeleteFlagSet(struct node* p)
{
	return ((uintptr_t) p & DELETE_BIT) != 0;
}

static inline bool isPromoteFlagSet(struct node* p)
{
	return ((uintptr_t) p & PROMOTE_BIT) != 0;
}

static inline bool isNodeMarked(struct node* p)
{
	return ((uintptr_t) p & DELETE_AND_PROMOTE_BIT) != 0;
}

static inline bool isKeyMarked(unsigned long key)
{
	return ((key & KEY_MASK) == KEY_MASK);
}

static inline struct node* setNull(struct node* p)
{
	return (struct node*) ((uintptr_t) p | NULL_BIT);
}

static inline struct node* setDeleteFlag(struct node* p)
{
	return (struct node*) ((uintptr_t) p | DELETE_BIT);
}

static inline struct node* setPromoteFlag(struct node* p)
{
	return (struct node*) ((uintptr_t) p | PROMOTE_BIT);
}

static inline unsigned long setReplaceFlagInKey(unsigned long key)
{
	return (key | KEY_MASK);
}

static inline unsigned long getKey(unsigned long key)
{
	return (key & MAX_KEY);
}

static inline struct node* getAddress(struct node* p)
{
	return (struct node*)((uintptr_t) p & ~((uintptr_t) ADDRESS_MASK));
}

static inline struct node* unmarkNode(struct node* p)
{
	return (struct node*)((uintptr_t) p & ~((uintptr_t) DELETE_AND_PROMOTE_BIT));
}