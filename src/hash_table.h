/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#ifndef HASH_TABLE_H
#define HASH_TABLE_H


#include <stdlib.h>


struct hash_table * ht_init(size_t buckets_cnt);
void ht_free(struct hash_table *ht, void (*key_free_callback)(void *),
                void (*data_free_callback)(void *));
void * ht_insert(struct hash_table *ht, const char *key, const void *data);
void * ht_read(const struct hash_table *ht, const char *key);


#endif /* HASH_TABLE_H */
