#ifndef _HOTNESS_H_
#define _HOTNESS_H_
extern void insert_uid_hotness_node(unsigned int new_hotcount, uid_t uid);
extern void delete_uid_hotness_node(uid_t uid);
extern void print_uid_hotness_list(struct seq_file *m);
#endif
