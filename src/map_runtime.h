#ifndef CCO_MAP_RUNTIME_H
#define CCO_MAP_RUNTIME_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef enum {
    __CCO_BUCKET_EMPTY = 0,
    __CCO_BUCKET_OCCUPIED = 1,
    __CCO_BUCKET_TOMBSTONE = 2
} __cco_bucket_state;

typedef struct {
    __cco_bucket_state state;
    void *key;
    void *value;
} __cco_map_bucket;

typedef struct {
    size_t capacity;
    size_t occupied;
    size_t tombstones;
    int key_type; // 0 = int, 1 = string
    __cco_map_bucket *buckets;
} __cco_map;

typedef void (*__cco_val_free_fn)(void *);

static inline uint32_t __cco_hash_str(const char *str) {
    uint32_t hash = 2166136261u;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619u;
    }
    return hash;
}

static inline uint32_t __cco_hash_int(int key) {
    uint32_t x = (uint32_t)key;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

static inline bool __cco_key_eq(int key_type, void *k1, void *k2) {
    if (key_type == 1) {
        if (!k1 || !k2) return k1 == k2;
        return strcmp((const char *)k1, (const char *)k2) == 0;
    } else {
        return (intptr_t)k1 == (intptr_t)k2;
    }
}

static inline uint32_t __cco_hash_key(int key_type, void *key) {
    if (key_type == 1) {
        return __cco_hash_str((const char *)key);
    } else {
        return __cco_hash_int((int)(intptr_t)key);
    }
}

static inline __cco_map *__cco_map_new(int key_type) {
    __cco_map *m = (__cco_map *)malloc(sizeof(__cco_map));
    if (!m) {
        fprintf(stderr, "Memory allocation failed in map_new()\n");
        exit(1);
    }
    m->capacity = 8;
    m->occupied = 0;
    m->tombstones = 0;
    m->key_type = key_type;
    m->buckets = (__cco_map_bucket *)calloc(m->capacity, sizeof(__cco_map_bucket));
    if (!m->buckets) {
        fprintf(stderr, "Memory allocation failed in map_new()\n");
        exit(1);
    }
    return m;
}

static inline void __cco_map_rehash(__cco_map *m) {
    size_t old_cap = m->capacity;
    size_t new_cap = old_cap * 2;
    __cco_map_bucket *old_buckets = m->buckets;

    __cco_map_bucket *new_buckets = (__cco_map_bucket *)calloc(new_cap, sizeof(__cco_map_bucket));
    if (!new_buckets) {
        fprintf(stderr, "Memory allocation failed during map rehash\n");
        exit(1);
    }

    m->buckets = new_buckets;
    m->capacity = new_cap;
    m->occupied = 0;
    m->tombstones = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_buckets[i].state == __CCO_BUCKET_OCCUPIED) {
            void *key = old_buckets[i].key;
            void *val = old_buckets[i].value;
            uint32_t h = __cco_hash_key(m->key_type, key);
            size_t idx = h & (new_cap - 1);
            while (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {
                idx = (idx + 1) & (new_cap - 1);
            }
            m->buckets[idx].state = __CCO_BUCKET_OCCUPIED;
            m->buckets[idx].key = key;
            m->buckets[idx].value = val;
            m->occupied++;
        }
    }

    free(old_buckets);
}

static inline __cco_map *__cco_map_put(__cco_map *m, void *key, void *val, __cco_val_free_fn free_fn) {
    if (!m) return NULL;

    if ((m->occupied + m->tombstones + 1) * 10 >= m->capacity * 7) {
        __cco_map_rehash(m);
    }

    uint32_t h = __cco_hash_key(m->key_type, key);
    size_t cap = m->capacity;
    size_t idx = h & (cap - 1);
    ssize_t first_tombstone = -1;

    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {
        if (m->buckets[idx].state == __CCO_BUCKET_TOMBSTONE) {
            if (first_tombstone == -1) {
                first_tombstone = (ssize_t)idx;
            }
        } else if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {
            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {
                if (free_fn && m->buckets[idx].value) {
                    free_fn(m->buckets[idx].value);
                }
                m->buckets[idx].value = val;
                return m;
            }
        }
        idx = (idx + 1) & (cap - 1);
    }

    size_t insert_idx = (first_tombstone != -1) ? (size_t)first_tombstone : idx;

    void *key_to_store;
    if (m->key_type == 1) {
        key_to_store = strdup((const char *)key);
        if (!key_to_store) {
            fprintf(stderr, "Memory allocation failed in strdup key\n");
            exit(1);
        }
    } else {
        key_to_store = key;
    }

    if (m->buckets[insert_idx].state == __CCO_BUCKET_TOMBSTONE) {
        m->tombstones--;
    }
    m->buckets[insert_idx].state = __CCO_BUCKET_OCCUPIED;
    m->buckets[insert_idx].key = key_to_store;
    m->buckets[insert_idx].value = val;
    m->occupied++;

    return m;
}

static inline void *__cco_map_get(__cco_map *m, void *key) {
    if (!m) {
        fprintf(stderr, "Runtime Error: map is null in get()\n");
        exit(1);
    }
    uint32_t h = __cco_hash_key(m->key_type, key);
    size_t cap = m->capacity;
    size_t idx = h & (cap - 1);

    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {
        if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {
            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {
                return m->buckets[idx].value;
            }
        }
        idx = (idx + 1) & (cap - 1);
    }

    if (m->key_type == 1) {
        fprintf(stderr, "Runtime Error: key '%s' not found in map\n", (const char *)key);
    } else {
        fprintf(stderr, "Runtime Error: key '%d' not found in map\n", (int)(intptr_t)key);
    }
    exit(1);
}

static inline bool __cco_map_has(__cco_map *m, void *key) {
    if (!m) return false;
    uint32_t h = __cco_hash_key(m->key_type, key);
    size_t cap = m->capacity;
    size_t idx = h & (cap - 1);

    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {
        if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {
            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {
                return true;
            }
        }
        idx = (idx + 1) & (cap - 1);
    }
    return false;
}

static inline void *__cco_map_remove(__cco_map *m, void *key) {
    if (!m) {
        fprintf(stderr, "Runtime Error: map is null in remove()\n");
        exit(1);
    }
    uint32_t h = __cco_hash_key(m->key_type, key);
    size_t cap = m->capacity;
    size_t idx = h & (cap - 1);

    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {
        if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {
            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {
                void *val = m->buckets[idx].value;
                if (m->key_type == 1) {
                    free(m->buckets[idx].key);
                }
                m->buckets[idx].key = NULL;
                m->buckets[idx].value = NULL;
                m->buckets[idx].state = __CCO_BUCKET_TOMBSTONE;
                m->occupied--;
                m->tombstones++;
                return val;
            }
        }
        idx = (idx + 1) & (cap - 1);
    }

    if (m->key_type == 1) {
        fprintf(stderr, "Runtime Error: key '%s' not found in map\n", (const char *)key);
    } else {
        fprintf(stderr, "Runtime Error: key '%d' not found in map\n", (int)(intptr_t)key);
    }
    exit(1);
}

static inline void __cco_map_free(__cco_map *m, __cco_val_free_fn val_free_fn) {
    if (!m) return;
    if (m->buckets) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (m->buckets[i].state == __CCO_BUCKET_OCCUPIED) {
                if (m->key_type == 1 && m->buckets[i].key) {
                    free(m->buckets[i].key);
                }
                if (val_free_fn && m->buckets[i].value) {
                    val_free_fn(m->buckets[i].value);
                }
            }
        }
        free(m->buckets);
    }
    free(m);
}

#endif // CCO_MAP_RUNTIME_H
