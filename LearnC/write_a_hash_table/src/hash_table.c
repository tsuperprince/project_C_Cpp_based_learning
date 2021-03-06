#include "hash_table.h"
static ht_item HT_DELETED_ITEM = {NULL, NULL};
/*************************define functions************************************** */
/* @des: a function ht_new_item allocates a chunk of memory the size of an ht_item
 * and saves a copy of the strings k and v in the new chunk of memory
 * */
static ht_item *ht_new_item(const char *k, const char *v) {
    ht_item* i = malloc(sizeof(ht_item));
    i->key = strdup(k);
    i->value = strdup(v);
    /*strdup(const char *s) This function returns a pointer to a null-terminated byte 
    string, which is a duplicate of the string pointed to by s. The memory obtained is 
    done dynamically using malloc and hence it can be freed using free().*/
    return i;
}
/*  
 * @des: a function resize hash table
 */
static void ht_resize(ht_hash_table* ht, const int base_size) {
    if (base_size < HT_INITIAL_BASE_SIZE) {
        return;
    }
    ht_hash_table* new_ht = ht_new_sized(base_size);
    for (int i = 0; i < ht->size; i++) {
        ht_item* item = ht->items[i];
        if (item != NULL && item != &HT_DELETED_ITEM) {
            ht_insert(new_ht, item->key, item->value);
        }
    }

    ht->base_size = new_ht->base_size;
    ht->count = new_ht->count;

    // To delete new_ht, we give it ht's size and items 
    const int tmp_size = ht->size;
    ht->size = new_ht->size;
    new_ht->size = tmp_size;

    ht_item** tmp_items = ht->items;
    ht->items = new_ht->items;
    new_ht->items = tmp_items;

    ht_del_hash_table(new_ht);
}
static void ht_resize_up(ht_hash_table* ht) {
    const int new_size = ht->base_size * 2;
    ht_resize(ht, new_size);
}


static void ht_resize_down(ht_hash_table* ht) {
    const int new_size = ht->base_size / 2;
    ht_resize(ht, new_size);
}
/*  
 * @des: a function ht_new initialises a new hash table.
 */
static ht_hash_table* ht_new_sized(const int base_size) {
    ht_hash_table* ht = xmalloc(sizeof(ht_hash_table));
    ht->base_size = base_size;

    ht->size = next_prime(ht->base_size);

    ht->count = 0;
    ht->items = xcalloc((size_t)ht->size, sizeof(ht_item*));
    return ht;
}


ht_hash_table* ht_new() {
    return ht_new_sized(HT_INITIAL_BASE_SIZE);
}

/* 
 * @des: function ht_items and ht_hash_tables,which free the memory we've allocated,
 * so we don't cause memory leaks.
 * */
static void ht_del_item(ht_item* i) {
    free(i->key);
    free(i->value);
    free(i);
}

/* 
 * @des: This function delete a hash table
 * */
void ht_del_hash_table(ht_hash_table* ht) {
    ht_item *item;
    //loop in all pointer items and call ht_del_item()
    for(int i = 0; i < ht->size; i++) {
        item = ht->items[i];
        if(item != NULL) {
            ht_del_item(item);
        }
    }
    free(ht->items);
    free(ht);
}

/* 
 * @des: hash function
 * */
static int ht_hash(const char* s, const int prime_num, const int hash_size) {
    long hash = 0;
    const int len_s = strlen(s);
    for(int i = 0; i < len_s; i++) {
        hash += (long)pow(prime_num,len_s - (i+1)) * s[i];
        hash = hash%hash_size;
    }
    return (int)hash;
}

/* 
 * @des: double hashing function
 * */
static int ht_get_hash(
    const char *s, const int num_buckets, const int attempt
) {
    const int hash_a = ht_hash(s, HT_PRIME_1, num_buckets);
    const int hash_b = ht_hash(s, HT_PRIME_2, num_buckets);
    return (hash_a + (attempt*(hash_b + 1)))%num_buckets;
}

/* 
 * @des: This function insert a new key-value pair
 * */
void ht_insert(ht_hash_table* ht, const char* key, const char* value) {
    const int load = ht->count * 100 / ht->size;
    if (load > 70) {
        ht_resize_up(ht);
    }
    ht_item* item = ht_new_item(key, value);
    int index = ht_get_hash(item->key, ht->size, 0);
    ht_item* cur_item = ht->items[index];
    int i = 1;
    while (cur_item != NULL && cur_item != HT_DELETED_ITEM) {
        if(strcmp(cur_item->key, key) == 0) {
            ht_del_item(cur_item);
            ht->items[index] = item;
            return;
        }
        index = ht_get_hash(item->key, ht->size, i);
        cur_item = ht->items[index];
        i++;
    } 
    ht->items[index] = item;
    ht->count++;
}

/* 
 * @des: This function search a key value in hash tables
 * */
char *ht_search(ht_hash_table* ht, const char* key){
    int index = ht_get_hash(key, ht->size,0);
    ht_item* item = ht->items[index];
    int i = 1;
    while(item != NULL){
        if(strcmp(item->key, key) == 0){
            return item->value;
        }
        index = ht_get_hash(key, ht->size, i);
        item = ht->items[index];
        i++;
    }
    return NULL;
}


/* 
 * @des: This function delete a key value in hash tables
 * */
void ht_delete(ht_hash_table* ht, const char* key) {
    const int load = ht->count * 100 / ht->size;
    if (load < 10) {
        ht_resize_down(ht);
    }
    int index = ht_get_hash(key, ht->size, 0);
    ht_item* item = ht->items[index];
    int i = 1;
    while (item != NULL) {
        if (item != &HT_DELETED_ITEM) {
            if (strcmp(item->key, key) == 0) {
                ht_del_item(item);
                ht->items[index] = &HT_DELETED_ITEM;
            }
        }
        index = ht_get_hash(key, ht->size, i);
        item = ht->items[index];
        i++;
    } 
    ht->count--;
}