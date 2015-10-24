/*
 * file: hash_table.c
 * author: Jan Wrona, <xwrona00@stud.fit.vutbr.cz>
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

#if 0
/*
 * hash table first record key read
 * if first recodr exists, retrun its key
 * if no record found, return null
 */
/*@null@*/ void * HT_first_key (HT_t * const hash_table)
{
        const struct HT_item *node = NULL;

        if (hash_table == NULL)
        {
                fprintf (stderr, "%s: invalid argument value\n", __func__);
                return NULL;
        }

        if ((node = HT_first (hash_table)) == NULL)
                return NULL;
        else
                return node->key;
}


/*
 * hash table (re)init
 */
        static void
HT_init (HT_t *const hash_table)
{
        size_t i;

        if (hash_table == NULL)
        {
                fprintf (stderr, "%s: invalid argument hash_table == NULL\n", __func__);
                return;
        }

        for (i = 0; i < hash_table->_tsize; ++i)
                hash_table->table[i] = NULL;		[> set all hash array fields to NULL <]

}

/*
 * hash table first record read
 * if first recodr exists, retrun it
 * if no record found, return null
 */
static struct HT_item * HT_first(struct hash_table *ht)
{
        if (ht == NULL) {
                fprintf (stderr, "%s: invalid argument value\n", __func__);
                return NULL;
        }

        for (size_t i = 0; i < ht->_tsize; ++i) { //for every header
                if (ht->table[i] != NULL) { //if HT record exists
                        return ht->table[i];
                }
        }

        return NULL; //no record in header
}

/*
 * hash table record delete
 * if key found, delete record and return 1
 * if key not found, do nothing and return 0
 */
unsigned HT_delete(struct hash_table *ht, const char *key)
{
        uint32_t index = 0;
        struct HT_item *ht_item = NULL;
        struct HT_item *prev_ht_item = NULL;


        if (ht == NULL || key == NULL) {
                fprintf(stderr, "%s: invalid argument value\n", __func__);
                return 0;
        }

        index = (super_fast_hash(key, (unsigned)ht->_ksize) % ht->_tsize);
        ht_item = ht->table[index]; //hash table record

        if (ht_item == NULL) { //record doesn't exist
                return 0;
        }

        //first record in list
        if (memcmp(key, ht_item->key, ht->_ksize) == 0) {
                ht->table[index] = ht_item->next; //hash table refresh
                free(ht_item->key);
                /* memory pointed to by ht_item->data MUST be free'd by user */
                free(ht_item->data);
                free(ht_item);
                ht_item = NULL;
                return 1;
        }

        prev_ht_item = ht_item; //save this record pointer
        ht_item = ht_item->next; //switch to next record in list
        while (ht_item != NULL) { //while item exists
                if (memcmp(key, ht_item->key, ht->_ksize) == 0) {
                        /* keys are the same */
                        prev_ht_item->next = ht_item->next; //previous record refresh
                        free(ht_item->key);
                        /* memory pointed to by ht_item->data MUST be free'd by user */
                        free(ht_item->data);
                        free(ht_item);
                        ht_item = NULL;
                        return 1;
                }

                prev_ht_item = ht_item; //save this record pointer
                ht_item = ht_item->next; //switch to next record in list
        }

        return 0;
}
#endif
