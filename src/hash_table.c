/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#include "hash_table.h"


#include <stdio.h>
#include <string.h>
#include <assert.h>


#define DEFAULT_BUCKETS_CNT 13


/* Hash Table item structure. */
struct ht_entry {
  char *key; //string key
  void *data; //pointer to user data
  struct ht_entry *next; //pointer to next synonym entry
};

/* Hash table structure. */
struct hash_table {
  struct ht_entry **buckets; //array of pointers to the entries
  size_t buckets_cnt;
};


/*
 * static functions
 */

/*
 * Specialized hash function for C strings.
 * Source: http://www.cse.yorku.ca/~oz/hash.html
 */
static size_t djb2_hash(const char *str)
{
        size_t hash = 5381;
        int c;

        while ((c = *str++)) {
                hash = ((hash << 5) + hash) + c; //hash * 33 + c
        }

        return hash;
}


/*
 * Hash table search by key.
 * If key found return pointer to item, else return NULL.
 */
static struct ht_entry * ht_search(const struct hash_table *ht, const char *key,
                size_t *bucket_index)
{
        assert(ht != NULL && key != NULL && bucket_index != NULL);

        *bucket_index = djb2_hash(key) % ht->buckets_cnt;

        /* Loop through all the synonyms. */
        for (struct ht_entry *entry = ht->buckets[*bucket_index]; entry != NULL;
                        entry = entry->next)
        {
                if (strcmp(key, entry->key) == 0) {
                        return entry; //keys are equal
                }
        }

        return NULL;
}


/*
 * public functions
 */

/*
 * Hash table creation and initialization.
 */
struct hash_table * ht_init(size_t buckets_cnt)
{
        struct hash_table *ht;


        /* Hash table struct memory allocation. */
        ht = malloc(sizeof (struct hash_table));
        if (ht == NULL) {
                fprintf(stderr, "%s: malloc error\n", __func__);
                return NULL;
        }

        /* Use provided or default buckets count. */
        ht->buckets_cnt = (buckets_cnt == 0)? DEFAULT_BUCKETS_CNT : buckets_cnt;

        /* Buckets memory allocation. */
        ht->buckets = calloc(ht->buckets_cnt, sizeof (struct ht_entry));
        if (ht->buckets == NULL) {
                fprintf(stderr, "%s: malloc error\n", __func__);
                free(ht);
                return NULL;
        }


        return ht;
}

/*
 * Hash table clear and free. Optional key and data freeing callbacks.
 */
void ht_free(struct hash_table *ht, void (*key_free_callback)(void *),
                void (*data_free_callback)(void *))
{
        assert(ht != NULL);

        for (size_t i = 0; i < ht->buckets_cnt; ++i) { //for each bucket
                struct ht_entry *entry = ht->buckets[i]; //get head entry

                while (entry != NULL) { //for each entry in bucket
                        struct ht_entry *tmp = entry->next; //save next entry

                        if (key_free_callback) {
                                key_free_callback(entry->key);
                        }
                        if (data_free_callback) {
                                data_free_callback(entry->data);
                        }
                        free(entry); //free entry
                        entry = tmp; //switch to next entry in list
                }
        }

        free(ht->buckets);
        free(ht);
}

/*
 * Hash table insert.
 * If key not found create new record, else refresh existing item with new data.
 * Return NULL if error, else pointer to inserted data.
 */
void * ht_insert(struct hash_table *ht, const char *key, const void *data)
{
        size_t bucket_index;
        struct ht_entry *entry;


        assert(ht != NULL && key != NULL);

        entry = ht_search(ht, key, &bucket_index);
        if (entry == NULL) { //entry (key) not found, create new onw
                entry = malloc(sizeof (struct ht_entry));
                if (entry == NULL) {
                        fprintf(stderr, "%s: malloc error\n", __func__);
                        return NULL;
                }

                entry->key = (char *)key;

                entry->next = ht->buckets[bucket_index]; //next entry pointer
                ht->buckets[bucket_index] = entry; //insert new record to bucket
        }
        entry->data = (void *)data;


        return entry->data; //return new record data pointer
}

/*
 * Hash table record read.
 * If key found, return pointer to record, else return NULL.
 */
void * ht_read(const struct hash_table *ht, const char *key)
{
        size_t bucket_index;
        struct ht_entry *entry;


        assert(ht != NULL && key != NULL);

        entry = ht_search(ht, key, &bucket_index);
        if (entry == NULL) { //not found
                return NULL;
        } else { //found
                return entry->data;
        }
}
