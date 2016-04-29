#define TASK_LAZY_FREE 0
#define TASK_NTYPES    1

#define SUBTYPE_FREE_OBJ 0

void rr_bgt_init(void);
void rr_bgt_add_task(int type, int sub_type, void *ud);
void rr_bgt_terminate(void);
