#ifndef VERSE_HASH_H
#define VERSE_HASH_H

#include <stdlib.h>
#include <string.h>

// adapted from https://github.com/petewarden/c_hashmap
// inspired by https://github.com/rxi/map

typedef struct hashmap_element {
	int   in_use;
	char *key;
	void *value;
} hashmap_element;

typedef struct hashmap {
	int capacity;
	int size;
	hashmap_element *data;
} hashmap;

typedef struct iter_t {
    int index;
    char *key;
    void *ref;
} iter_t;

void  _hashmap_init(hashmap *m);
void *_hashmap_get(hashmap *m, char* key);
int   _hashmap_put(hashmap *m, char* key, void *value, size_t vsize);
int   _hashmap_remove(hashmap *m, char* key);
void  _hashmap_free(hashmap *m);
void  _hashmap_next(hashmap *m, iter_t *iter);

#define hashmap_t(T) struct{ T tmp; T *ref; hashmap map; }

#define hashmap_init(m) \
    (_hashmap_init(&(m)->map))

#define hashmap_get(m, key) \
    ((m)->ref = _hashmap_get(&(m)->map, key))

#define hashmap_put(m, key, value) \
    ((m)->tmp = (value), _hashmap_put(&(m)->map, (key), &(m)->tmp, sizeof((m)->tmp)))

#define hashmap_remove(m, key) \
    (_hashmap_remove(&(m)->map, (key)))

#define hashmap_free(m) \
    (_hashmap_free(&(m)->map))

#define hashmap_iter() \
    ((iter_t){.index=-1,.ref=NULL})

#define hashmap_next(m, iter) \
     (_hashmap_next(&(m)->map, &(iter)), (m)->ref = (iter).ref)

#endif // VERSE_HASH_H
