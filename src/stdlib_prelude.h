#ifndef STDLIB_PRELUDE_H
#define STDLIB_PRELUDE_H

#include <stddef.h>

typedef struct {
    const char *name;
    const char *c_source;
    const char **depends_on;
} PreludeChunk;

static const char *deps_alloc_arr[] = { "arr_header", NULL };
static const char *deps_list_new[] = { "arr_header", NULL };
static const char *deps_arr_len_raw[] = { "arr_header", NULL };
static const char *deps_arr_len[] = { "arr_len_raw", "arr_header", NULL };
static const char *deps_free_arr[] = { "arr_header", NULL };
static const char *deps_arr_incr_len[] = { "arr_header", NULL };
static const char *deps_arr_decr_len[] = { "arr_header", NULL };
static const char *deps_arr_maybe_grow[] = { "arr_header", NULL };
static const char *deps_bounds_check[] = { "arr_len", "arr_header", NULL };

static const char *deps_map_hash[] = { "map_bucket", NULL };
static const char *deps_map_new[] = { "map_bucket", NULL };
static const char *deps_map_rehash[] = { "map_hash", "map_bucket", NULL };
static const char *deps_map_put[] = { "map_rehash", "map_hash", "map_bucket", NULL };
static const char *deps_map_get[] = { "map_hash", "map_bucket", NULL };
static const char *deps_map_has[] = { "map_hash", "map_bucket", NULL };
static const char *deps_map_remove[] = { "map_hash", "map_bucket", NULL };
static const char *deps_map_keys[] = { "list_new", "arr_maybe_grow", "arr_len_raw", "arr_incr_len", "map_bucket", NULL };
static const char *deps_map_free[] = { "map_bucket", NULL };
static const char *deps_get_args[] = { "list_new", "arr_maybe_grow", "arr_len_raw", "arr_incr_len", NULL };
static const char *deps_random_int[] = { "random_seed", NULL };
static const char *deps_is_int[] = { "try_parse_int", NULL };
static const char *deps_to_int[] = { "try_parse_int", NULL };
static const char *deps_is_float[] = { "try_parse_float", NULL };
static const char *deps_to_float[] = { "try_parse_float", NULL };

static const PreludeChunk PRELUDE_CHUNKS[] = {
    {
        "concat",
        "static inline char *__cco_concat(const char *a, const char *b) {\n"
        "    if (!a) a = \"\";\n"
        "    if (!b) b = \"\";\n"
        "    size_t len_a = strlen(a);\n"
        "    size_t len_b = strlen(b);\n"
        "    char *res = (char *)malloc(len_a + len_b + 1);\n"
        "    if (!res) {\n"
        "        fprintf(stderr, \"Memory allocation failed in concat()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    strcpy(res, a);\n"
        "    strcat(res, b);\n"
        "    return res;\n"
        "}\n\n",
        NULL
    },
    {
        "char_at",
        "static inline char __cco_char_at(const char *s, int i) {\n"
        "    if (!s) {\n"
        "        fprintf(stderr, \"Error: char_at called on null string\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    int len = (int)strlen(s);\n"
        "    if (i < 0 || i >= len) {\n"
        "        fprintf(stderr, \"Runtime Error: char_at index %d out of bounds (length %d)\\n\", i, len);\n"
        "        exit(1);\n"
        "    }\n"
        "    return s[i];\n"
        "}\n\n",
        NULL
    },
    {
        "substring",
        "static inline char *__cco_substring(const char *s, int start, int end) {\n"
        "    if (!s) s = \"\";\n"
        "    int len = (int)strlen(s);\n"
        "    if (start < 0) start = 0;\n"
        "    if (end > len) end = len;\n"
        "    if (start > end) start = end;\n"
        "    int sub_len = end - start;\n"
        "    char *res = (char *)malloc(sub_len + 1);\n"
        "    if (!res) {\n"
        "        fprintf(stderr, \"Memory allocation failed in substring()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    memcpy(res, s + start, sub_len);\n"
        "    res[sub_len] = '\\0';\n"
        "    return res;\n"
        "}\n\n",
        NULL
    },
    {
        "abs_int",
        "static inline int __cco_abs_int(int x) {\n"
        "    return x < 0 ? -x : x;\n"
        "}\n\n",
        NULL
    },
    {
        "min_int",
        "static inline int __cco_min_int(int a, int b) {\n"
        "    return a < b ? a : b;\n"
        "}\n\n",
        NULL
    },
    {
        "max_int",
        "static inline int __cco_max_int(int a, int b) {\n"
        "    return a > b ? a : b;\n"
        "}\n\n",
        NULL
    },
    {
        "min_float",
        "static inline double __cco_min_float(double a, double b) {\n"
        "    return a < b ? a : b;\n"
        "}\n\n",
        NULL
    },
    {
        "max_float",
        "static inline double __cco_max_float(double a, double b) {\n"
        "    return a > b ? a : b;\n"
        "}\n\n",
        NULL
    },
    {
        "read_file",
        "static inline char *__cco_read_file(const char *path) {\n"
        "    FILE *f = fopen(path, \"rb\");\n"
        "    if (!f) {\n"
        "        fprintf(stderr, \"Error: Could not open file '%s' for reading\\n\", path ? path : \"\");\n"
        "        exit(1);\n"
        "    }\n"
        "    fseek(f, 0L, SEEK_END);\n"
        "    long sz = ftell(f);\n"
        "    rewind(f);\n"
        "    char *buf = (char *)malloc(sz + 1);\n"
        "    if (!buf) {\n"
        "        fclose(f);\n"
        "        fprintf(stderr, \"Memory allocation failed in read_file()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    size_t read_bytes = fread(buf, 1, sz, f);\n"
        "    buf[read_bytes] = '\\0';\n"
        "    fclose(f);\n"
        "    return buf;\n"
        "}\n\n",
        NULL
    },
    {
        "write_file",
        "static inline bool __cco_write_file(const char *path, const char *content) {\n"
        "    if (!path) return false;\n"
        "    FILE *f = fopen(path, \"w\");\n"
        "    if (!f) return false;\n"
        "    if (!content) content = \"\";\n"
        "    size_t len = strlen(content);\n"
        "    size_t written = fwrite(content, 1, len, f);\n"
        "    fclose(f);\n"
        "    return written == len;\n"
        "}\n\n",
        NULL
    },
    {
        "arr_header",
        "typedef struct {\n"
        "    size_t capacity;\n"
        "    size_t length;\n"
        "} __cco_arr_header;\n\n",
        NULL
    },
    {
        "alloc_arr",
        "static inline void *__cco_alloc_arr(size_t elem_size, int count) {\n"
        "    if (count < 0) count = 0;\n"
        "    __cco_arr_header *hdr = (__cco_arr_header *)calloc(1, sizeof(__cco_arr_header) + (size_t)count * elem_size);\n"
        "    if (!hdr) {\n"
        "        fprintf(stderr, \"Memory allocation failed in alloc()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    hdr->capacity = (size_t)count;\n"
        "    hdr->length = (size_t)count;\n"
        "    return (void *)(hdr + 1);\n"
        "}\n\n",
        deps_alloc_arr
    },
    {
        "list_new",
        "static inline void *__cco_list_new(size_t elem_size) {\n"
        "    size_t init_cap = 4;\n"
        "    __cco_arr_header *hdr = (__cco_arr_header *)calloc(1, sizeof(__cco_arr_header) + init_cap * elem_size);\n"
        "    if (!hdr) {\n"
        "        fprintf(stderr, \"Memory allocation failed in list_new()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    hdr->capacity = init_cap;\n"
        "    hdr->length = 0;\n"
        "    return (void *)(hdr + 1);\n"
        "}\n\n",
        deps_list_new
    },
    {
        "arr_len_raw",
        "static inline int __cco_arr_len_raw(void *ptr) {\n"
        "    if (!ptr) return 0;\n"
        "    return (int)(((__cco_arr_header *)ptr)[-1].length);\n"
        "}\n\n",
        deps_arr_len_raw
    },
    {
        "arr_len",
        "static inline int __cco_arr_len(void *ptr) {\n"
        "    return __cco_arr_len_raw(ptr);\n"
        "}\n\n",
        deps_arr_len
    },
    {
        "free_arr",
        "static inline void __cco_free_arr(void *ptr) {\n"
        "    if (ptr) {\n"
        "        free(((__cco_arr_header *)ptr) - 1);\n"
        "    }\n"
        "}\n\n",
        deps_free_arr
    },
    {
        "arr_incr_len",
        "static inline void __cco_arr_incr_len(void *ptr) {\n"
        "    if (ptr) {\n"
        "        ((__cco_arr_header *)ptr)[-1].length++;\n"
        "    }\n"
        "}\n\n",
        deps_arr_incr_len
    },
    {
        "arr_decr_len",
        "static inline void __cco_arr_decr_len(void *ptr) {\n"
        "    if (ptr) {\n"
        "        if (((__cco_arr_header *)ptr)[-1].length == 0) {\n"
        "            fprintf(stderr, \"Runtime Error: pop() called on empty array\\n\");\n"
        "            exit(1);\n"
        "        }\n"
        "        ((__cco_arr_header *)ptr)[-1].length--;\n"
        "    }\n"
        "}\n\n",
        deps_arr_decr_len
    },
    {
        "arr_maybe_grow",
        "static inline void *__cco_arr_maybe_grow(void *ptr, size_t elem_size) {\n"
        "    if (!ptr) return NULL;\n"
        "    __cco_arr_header *hdr = ((__cco_arr_header *)ptr) - 1;\n"
        "    if (hdr->length >= hdr->capacity) {\n"
        "        size_t new_cap = hdr->capacity == 0 ? 4 : hdr->capacity * 2;\n"
        "        __cco_arr_header *new_hdr = (__cco_arr_header *)realloc(hdr, sizeof(__cco_arr_header) + new_cap * elem_size);\n"
        "        if (!new_hdr) {\n"
        "            fprintf(stderr, \"Memory allocation failed in push()\\n\");\n"
        "            exit(1);\n"
        "        }\n"
        "        new_hdr->capacity = new_cap;\n"
        "        return (void *)(new_hdr + 1);\n"
        "    }\n"
        "    return ptr;\n"
        "}\n\n",
        deps_arr_maybe_grow
    },
    {
        "bounds_check",
        "static inline void __cco_bounds_check(void *ptr, int idx) {\n"
        "    if (!ptr) {\n"
        "        fprintf(stderr, \"Runtime Error: array indexing null pointer\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    int len = __cco_arr_len_raw(ptr);\n"
        "    if (idx < 0 || idx >= len) {\n"
        "        fprintf(stderr, \"Runtime Error: array index %d out of bounds (length %d)\\n\", idx, len);\n"
        "        exit(1);\n"
        "    }\n"
        "}\n\n",
        deps_bounds_check
    },
    {
        "map_bucket",
        "#ifndef CCO_MAP_BUCKET_DEF\n"
        "#define CCO_MAP_BUCKET_DEF\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#include <string.h>\n"
        "typedef enum {\n"
        "    __CCO_BUCKET_EMPTY = 0,\n"
        "    __CCO_BUCKET_OCCUPIED = 1,\n"
        "    __CCO_BUCKET_TOMBSTONE = 2\n"
        "} __cco_bucket_state;\n\n"
        "typedef struct {\n"
        "    __cco_bucket_state state;\n"
        "    void *key;\n"
        "    void *value;\n"
        "} __cco_map_bucket;\n\n"
        "typedef struct {\n"
        "    size_t capacity;\n"
        "    size_t occupied;\n"
        "    size_t tombstones;\n"
        "    int key_type;\n"
        "    __cco_map_bucket *buckets;\n"
        "} __cco_map;\n\n"
        "typedef void (*__cco_val_free_fn)(void *);\n"
        "#endif\n\n",
        NULL
    },
    {
        "map_hash",
        "static inline uint32_t __cco_hash_str(const char *str) {\n"
        "    uint32_t hash = 2166136261u;\n"
        "    for (const char *p = str; *p; p++) {\n"
        "        hash ^= (uint8_t)*p;\n"
        "        hash *= 16777619u;\n"
        "    }\n"
        "    return hash;\n"
        "}\n\n"
        "static inline uint32_t __cco_hash_int(int key) {\n"
        "    uint32_t x = (uint32_t)key;\n"
        "    x = ((x >> 16) ^ x) * 0x45d9f3b;\n"
        "    x = ((x >> 16) ^ x) * 0x45d9f3b;\n"
        "    x = (x >> 16) ^ x;\n"
        "    return x;\n"
        "}\n\n"
        "static inline bool __cco_key_eq(int key_type, void *k1, void *k2) {\n"
        "    if (key_type == 1) {\n"
        "        if (!k1 || !k2) return k1 == k2;\n"
        "        return strcmp((const char *)k1, (const char *)k2) == 0;\n"
        "    } else {\n"
        "        return (intptr_t)k1 == (intptr_t)k2;\n"
        "    }\n"
        "}\n\n"
        "static inline uint32_t __cco_hash_key(int key_type, void *key) {\n"
        "    if (key_type == 1) return __cco_hash_str((const char *)key);\n"
        "    else return __cco_hash_int((int)(intptr_t)key);\n"
        "}\n\n",
        deps_map_hash
    },
    {
        "map_new",
        "static inline __cco_map *__cco_map_new(int key_type) {\n"
        "    __cco_map *m = (__cco_map *)malloc(sizeof(__cco_map));\n"
        "    if (!m) {\n"
        "        fprintf(stderr, \"Memory allocation failed in map_new()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    m->capacity = 8;\n"
        "    m->occupied = 0;\n"
        "    m->tombstones = 0;\n"
        "    m->key_type = key_type;\n"
        "    m->buckets = (__cco_map_bucket *)calloc(m->capacity, sizeof(__cco_map_bucket));\n"
        "    if (!m->buckets) {\n"
        "        fprintf(stderr, \"Memory allocation failed in map_new()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    return m;\n"
        "}\n\n",
        deps_map_new
    },
    {
        "map_rehash",
        "static inline void __cco_map_rehash(__cco_map *m) {\n"
        "    size_t old_cap = m->capacity;\n"
        "    size_t new_cap = old_cap * 2;\n"
        "    __cco_map_bucket *old_buckets = m->buckets;\n"
        "    __cco_map_bucket *new_buckets = (__cco_map_bucket *)calloc(new_cap, sizeof(__cco_map_bucket));\n"
        "    if (!new_buckets) {\n"
        "        fprintf(stderr, \"Memory allocation failed during map rehash\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    m->buckets = new_buckets;\n"
        "    m->capacity = new_cap;\n"
        "    m->occupied = 0;\n"
        "    m->tombstones = 0;\n"
        "    for (size_t i = 0; i < old_cap; i++) {\n"
        "        if (old_buckets[i].state == __CCO_BUCKET_OCCUPIED) {\n"
        "            void *key = old_buckets[i].key;\n"
        "            void *val = old_buckets[i].value;\n"
        "            uint32_t h = __cco_hash_key(m->key_type, key);\n"
        "            size_t idx = h & (new_cap - 1);\n"
        "            while (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {\n"
        "                idx = (idx + 1) & (new_cap - 1);\n"
        "            }\n"
        "            m->buckets[idx].state = __CCO_BUCKET_OCCUPIED;\n"
        "            m->buckets[idx].key = key;\n"
        "            m->buckets[idx].value = val;\n"
        "            m->occupied++;\n"
        "        }\n"
        "    }\n"
        "    free(old_buckets);\n"
        "}\n\n",
        deps_map_rehash
    },
    {
        "map_put",
        "static inline __cco_map *__cco_map_put(__cco_map *m, void *key, void *val, __cco_val_free_fn free_fn) {\n"
        "    if (!m) return NULL;\n"
        "    if ((m->occupied + m->tombstones + 1) * 10 >= m->capacity * 7) {\n"
        "        __cco_map_rehash(m);\n"
        "    }\n"
        "    uint32_t h = __cco_hash_key(m->key_type, key);\n"
        "    size_t cap = m->capacity;\n"
        "    size_t idx = h & (cap - 1);\n"
        "    long first_tombstone = -1;\n"
        "    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {\n"
        "        if (m->buckets[idx].state == __CCO_BUCKET_TOMBSTONE) {\n"
        "            if (first_tombstone == -1) first_tombstone = (long)idx;\n"
        "        } else if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {\n"
        "            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {\n"
        "                if (free_fn && m->buckets[idx].value) {\n"
        "                    free_fn(m->buckets[idx].value);\n"
        "                }\n"
        "                m->buckets[idx].value = val;\n"
        "                return m;\n"
        "            }\n"
        "        }\n"
        "        idx = (idx + 1) & (cap - 1);\n"
        "    }\n"
        "    size_t insert_idx = (first_tombstone != -1) ? (size_t)first_tombstone : idx;\n"
        "    void *key_to_store;\n"
        "    if (m->key_type == 1) {\n"
        "        key_to_store = strdup((const char *)key);\n"
        "        if (!key_to_store) {\n"
        "            fprintf(stderr, \"Memory allocation failed in strdup key\\n\");\n"
        "            exit(1);\n"
        "        }\n"
        "    } else {\n"
        "        key_to_store = key;\n"
        "    }\n"
        "    if (m->buckets[insert_idx].state == __CCO_BUCKET_TOMBSTONE) {\n"
        "        m->tombstones--;\n"
        "    }\n"
        "    m->buckets[insert_idx].state = __CCO_BUCKET_OCCUPIED;\n"
        "    m->buckets[insert_idx].key = key_to_store;\n"
        "    m->buckets[insert_idx].value = val;\n"
        "    m->occupied++;\n"
        "    return m;\n"
        "}\n\n",
        deps_map_put
    },
    {
        "map_get",
        "static inline void *__cco_map_get(__cco_map *m, void *key) {\n"
        "    if (!m) {\n"
        "        fprintf(stderr, \"Runtime Error: map is null in get()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    uint32_t h = __cco_hash_key(m->key_type, key);\n"
        "    size_t cap = m->capacity;\n"
        "    size_t idx = h & (cap - 1);\n"
        "    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {\n"
        "        if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {\n"
        "            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {\n"
        "                return m->buckets[idx].value;\n"
        "            }\n"
        "        }\n"
        "        idx = (idx + 1) & (cap - 1);\n"
        "    }\n"
        "    if (m->key_type == 1) {\n"
        "        fprintf(stderr, \"Runtime Error: key '%s' not found in map\\n\", (const char *)key);\n"
        "    } else {\n"
        "        fprintf(stderr, \"Runtime Error: key '%d' not found in map\\n\", (int)(intptr_t)key);\n"
        "    }\n"
        "    exit(1);\n"
        "}\n\n",
        deps_map_get
    },
    {
        "map_has",
        "static inline bool __cco_map_has(__cco_map *m, void *key) {\n"
        "    if (!m) return false;\n"
        "    uint32_t h = __cco_hash_key(m->key_type, key);\n"
        "    size_t cap = m->capacity;\n"
        "    size_t idx = h & (cap - 1);\n"
        "    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {\n"
        "        if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {\n"
        "            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {\n"
        "                return true;\n"
        "            }\n"
        "        }\n"
        "        idx = (idx + 1) & (cap - 1);\n"
        "    }\n"
        "    return false;\n"
        "}\n\n",
        deps_map_has
    },
    {
        "map_remove",
        "static inline void *__cco_map_remove(__cco_map *m, void *key) {\n"
        "    if (!m) {\n"
        "        fprintf(stderr, \"Runtime Error: map is null in remove()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    uint32_t h = __cco_hash_key(m->key_type, key);\n"
        "    size_t cap = m->capacity;\n"
        "    size_t idx = h & (cap - 1);\n"
        "    while (m->buckets[idx].state != __CCO_BUCKET_EMPTY) {\n"
        "        if (m->buckets[idx].state == __CCO_BUCKET_OCCUPIED) {\n"
        "            if (__cco_key_eq(m->key_type, m->buckets[idx].key, key)) {\n"
        "                void *val = m->buckets[idx].value;\n"
        "                if (m->key_type == 1) free(m->buckets[idx].key);\n"
        "                m->buckets[idx].key = NULL;\n"
        "                m->buckets[idx].value = NULL;\n"
        "                m->buckets[idx].state = __CCO_BUCKET_TOMBSTONE;\n"
        "                m->occupied--;\n"
        "                m->tombstones++;\n"
        "                return val;\n"
        "            }\n"
        "        }\n"
        "        idx = (idx + 1) & (cap - 1);\n"
        "    }\n"
        "    if (m->key_type == 1) {\n"
        "        fprintf(stderr, \"Runtime Error: key '%s' not found in map\\n\", (const char *)key);\n"
        "    } else {\n"
        "        fprintf(stderr, \"Runtime Error: key '%d' not found in map\\n\", (int)(intptr_t)key);\n"
        "    }\n"
        "    exit(1);\n"
        "}\n\n",
        deps_map_remove
    },
    {
        "map_keys",
        "static inline void *__cco_map_keys(__cco_map *m) {\n"
        "    if (!m) return NULL;\n"
        "    size_t elem_size = (m->key_type == 1) ? sizeof(char *) : sizeof(int);\n"
        "    void *arr = __cco_list_new(elem_size);\n"
        "    if (m->occupied == 0) return arr;\n"
        "    for (size_t i = 0; i < m->capacity; i++) {\n"
        "        if (m->buckets[i].state == __CCO_BUCKET_OCCUPIED) {\n"
        "            arr = __cco_arr_maybe_grow(arr, elem_size);\n"
        "            int len = __cco_arr_len_raw(arr);\n"
        "            if (m->key_type == 1) {\n"
        "                ((char **)arr)[len] = strdup((char *)m->buckets[i].key);\n"
        "            } else {\n"
        "                ((int *)arr)[len] = (int)(intptr_t)m->buckets[i].key;\n"
        "            }\n"
        "            __cco_arr_incr_len(arr);\n"
        "        }\n"
        "    }\n"
        "    return arr;\n"
        "}\n\n",
        deps_map_keys
    },
    {
        "map_free",
        "static inline void __cco_map_free(__cco_map *m, __cco_val_free_fn val_free_fn) {\n"
        "    if (!m) return;\n"
        "    if (m->buckets) {\n"
        "        for (size_t i = 0; i < m->capacity; i++) {\n"
        "            if (m->buckets[i].state == __CCO_BUCKET_OCCUPIED) {\n"
        "                if (m->key_type == 1 && m->buckets[i].key) {\n"
        "                    free(m->buckets[i].key);\n"
        "                }\n"
        "                if (val_free_fn && m->buckets[i].value) {\n"
        "                    val_free_fn(m->buckets[i].value);\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "        free(m->buckets);\n"
        "    }\n"
        "    free(m);\n"
        "}\n\n",
        deps_map_free
    },
    {
        "get_args",
        "static inline char **__cco_get_args(void) {\n"
        "    char **arr = (char **)__cco_list_new(sizeof(char *));\n"
        "    if (__cco_argc <= 1 || !__cco_argv) return arr;\n"
        "    for (int i = 1; i < __cco_argc; i++) {\n"
        "        arr = (char **)__cco_arr_maybe_grow(arr, sizeof(char *));\n"
        "        int len = __cco_arr_len_raw(arr);\n"
        "        arr[len] = strdup(__cco_argv[i]);\n"
        "        __cco_arr_incr_len(arr);\n"
        "    }\n"
        "    return arr;\n"
        "}\n\n",
        deps_get_args
    },
    {
        "get_arg_count",
        "static inline int __cco_get_arg_count(void) {\n"
        "    return (__cco_argc > 1) ? (__cco_argc - 1) : 0;\n"
        "}\n\n",
        NULL
    },
    {
        "get_program_name",
        "static inline char *__cco_get_program_name(void) {\n"
        "    if (__cco_argc > 0 && __cco_argv && __cco_argv[0]) {\n"
        "        return strdup(__cco_argv[0]);\n"
        "    }\n"
        "    return strdup(\"\");\n"
        "}\n\n",
        NULL
    },
    {
        "random_seed",
        "static int __cco_rand_seeded = 0;\n"
        "static inline void __cco_random_seed(long seed) {\n"
        "    srand((unsigned int)seed);\n"
        "    __cco_rand_seeded = 1;\n"
        "}\n\n",
        NULL
    },
    {
        "random_int",
        "static inline long __cco_random_int(long min, long max) {\n"
        "    if (!__cco_rand_seeded) {\n"
        "        srand((unsigned int)time(NULL));\n"
        "        __cco_rand_seeded = 1;\n"
        "    }\n"
        "    if (min == max) return min;\n"
        "    if (min > max) { long tmp = min; min = max; max = tmp; }\n"
        "    unsigned long range = (unsigned long)(max - min + 1);\n"
        "    unsigned long limit = (unsigned long)RAND_MAX - ((unsigned long)RAND_MAX % range);\n"
        "    unsigned long r;\n"
        "    do {\n"
        "        r = (unsigned long)rand();\n"
        "    } while (r >= limit);\n"
        "    return min + (long)(r % range);\n"
        "}\n\n",
        deps_random_int
    },
    {
        "try_parse_int",
        "static inline bool __cco_try_parse_int(const char *s, long *out_val) {\n"
        "    if (!s) return false;\n"
        "    while (*s == ' ' || *s == '\\t' || *s == '\\n' || *s == '\\r') s++;\n"
        "    if (!*s) return false;\n"
        "    char *endptr = NULL;\n"
        "    errno = 0;\n"
        "    long val = strtol(s, &endptr, 10);\n"
        "    if (endptr == s || errno != 0) return false;\n"
        "    while (*endptr == ' ' || *endptr == '\\t' || *endptr == '\\n' || *endptr == '\\r') endptr++;\n"
        "    if (*endptr != '\\0') return false;\n"
        "    if (out_val) *out_val = val;\n"
        "    return true;\n"
        "}\n\n",
        NULL
    },
    {
        "is_int",
        "static inline bool __cco_is_int(const char *s) {\n"
        "    return __cco_try_parse_int(s, NULL);\n"
        "}\n\n",
        deps_is_int
    },
    {
        "to_int",
        "static inline long __cco_to_int(const char *s) {\n"
        "    long val = 0;\n"
        "    if (!__cco_try_parse_int(s, &val)) {\n"
        "        fprintf(stderr, \"Runtime Error: '%s' is not a valid integer\\n\", s ? s : \"null\");\n"
        "        exit(1);\n"
        "    }\n"
        "    return val;\n"
        "}\n\n",
        deps_to_int
    },
    {
        "try_parse_float",
        "static inline bool __cco_try_parse_float(const char *s, double *out_val) {\n"
        "    if (!s) return false;\n"
        "    while (*s == ' ' || *s == '\\t' || *s == '\\n' || *s == '\\r') s++;\n"
        "    if (!*s) return false;\n"
        "    char *endptr = NULL;\n"
        "    errno = 0;\n"
        "    double val = strtod(s, &endptr);\n"
        "    if (endptr == s || errno != 0) return false;\n"
        "    while (*endptr == ' ' || *endptr == '\\t' || *endptr == '\\n' || *endptr == '\\r') endptr++;\n"
        "    if (*endptr != '\\0') return false;\n"
        "    if (out_val) *out_val = val;\n"
        "    return true;\n"
        "}\n\n",
        NULL
    },
    {
        "is_float",
        "static inline bool __cco_is_float(const char *s) {\n"
        "    return __cco_try_parse_float(s, NULL);\n"
        "}\n\n",
        deps_is_float
    },
    {
        "to_float",
        "static inline double __cco_to_float(const char *s) {\n"
        "    double val = 0.0;\n"
        "    if (!__cco_try_parse_float(s, &val)) {\n"
        "        fprintf(stderr, \"Runtime Error: '%s' is not a valid float\\n\", s ? s : \"null\");\n"
        "        exit(1);\n"
        "    }\n"
        "    return val;\n"
        "}\n\n",
        deps_to_float
    },
    {
        "read_line",
        "static inline char *__cco_read_line(void) {\n"
        "    size_t cap = 128;\n"
        "    size_t len = 0;\n"
        "    char *buf = (char *)malloc(cap);\n"
        "    if (!buf) {\n"
        "        fprintf(stderr, \"Memory allocation failed in read_line()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    int c;\n"
        "    while ((c = fgetc(stdin)) != EOF) {\n"
        "        if (c == '\\n') break;\n"
        "        if (c == '\\r') {\n"
        "            int next_c = fgetc(stdin);\n"
        "            if (next_c != '\\n' && next_c != EOF) {\n"
        "                ungetc(next_c, stdin);\n"
        "            }\n"
        "            break;\n"
        "        }\n"
        "        if (len + 1 >= cap) {\n"
        "            cap *= 2;\n"
        "            char *new_buf = (char *)realloc(buf, cap);\n"
        "            if (!new_buf) {\n"
        "                free(buf);\n"
        "                fprintf(stderr, \"Memory allocation failed in read_line()\\n\");\n"
        "                exit(1);\n"
        "            }\n"
        "            buf = new_buf;\n"
        "        }\n"
        "        buf[len++] = (char)c;\n"
        "    }\n"
        "    buf[len] = '\\0';\n"
        "    return buf;\n"
        "}\n\n",
        NULL
    },
    {
        "concat_free",
        "static inline char *__cco_concat_free(const char *a, const char *b, bool free_a, bool free_b) {\n"
        "    if (!a) a = \"\";\n"
        "    if (!b) b = \"\";\n"
        "    size_t len_a = strlen(a);\n"
        "    size_t len_b = strlen(b);\n"
        "    char *res = (char *)malloc(len_a + len_b + 1);\n"
        "    if (!res) {\n"
        "        fprintf(stderr, \"Memory allocation failed in concat_free()\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    memcpy(res, a, len_a);\n"
        "    memcpy(res + len_a, b, len_b);\n"
        "    res[len_a + len_b] = '\\0';\n"
        "    if (free_a && a && *a) free((void *)a);\n"
        "    if (free_b && b && *b) free((void *)b);\n"
        "    return res;\n"
        "}\n\n",
        NULL
    },
    {
        "int_to_str",
        "static inline char *__cco_int_to_str(long n) {\n"
        "    char buf[32];\n"
        "    snprintf(buf, sizeof(buf), \"%ld\", n);\n"
        "    return strdup(buf);\n"
        "}\n\n",
        NULL
    },
    {
        "float_to_str",
        "static inline char *__cco_float_to_str(double f) {\n"
        "    char buf[64];\n"
        "    snprintf(buf, sizeof(buf), \"%g\", f);\n"
        "    return strdup(buf);\n"
        "}\n\n",
        NULL
    },
    {
        "bool_to_str",
        "static inline char *__cco_bool_to_str(bool b) {\n"
        "    return strdup(b ? \"true\" : \"false\");\n"
        "}\n\n",
        NULL
    },
    {
        "char_to_str",
        "static inline char *__cco_char_to_str(char c) {\n"
        "    char buf[2];\n"
        "    buf[0] = c;\n"
        "    buf[1] = '\\0';\n"
        "    return strdup(buf);\n"
        "}\n\n",
        NULL
    },
    {
        "net_listen",
        "static inline int __cco_net_listen(int port) {\n"
        "    int server_fd = socket(AF_INET, SOCK_STREAM, 0);\n"
        "    if (server_fd < 0) return -1;\n"
        "    int opt = 1;\n"
        "    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));\n"
        "    struct sockaddr_in address;\n"
        "    memset(&address, 0, sizeof(address));\n"
        "    address.sin_family = AF_INET;\n"
        "    address.sin_addr.s_addr = INADDR_ANY;\n"
        "    address.sin_port = htons((uint16_t)port);\n"
        "    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {\n"
        "        close(server_fd);\n"
        "        return -1;\n"
        "    }\n"
        "    if (listen(server_fd, 128) < 0) {\n"
        "        close(server_fd);\n"
        "        return -1;\n"
        "    }\n"
        "    return server_fd;\n"
        "}\n\n",
        NULL
    },
    {
        "net_accept",
        "static inline int __cco_net_accept(int server_fd) {\n"
        "    struct sockaddr_in client_addr;\n"
        "    socklen_t client_len = sizeof(client_addr);\n"
        "    return accept(server_fd, (struct sockaddr *)&client_addr, &client_len);\n"
        "}\n\n",
        NULL
    },
    {
        "net_recv",
        "static inline char *__cco_net_recv(int client_fd, int initial_capacity) {\n"
        "    if (client_fd < 0) return strdup(\"\");\n"
        "    size_t cap = (initial_capacity > 0) ? (size_t)initial_capacity : 4096;\n"
        "    size_t len = 0;\n"
        "    char *buf = (char *)malloc(cap + 1);\n"
        "    if (!buf) return strdup(\"\");\n"
        "    while (1) {\n"
        "        if (len >= cap) {\n"
        "            if (cap >= 1048576) break;\n"
        "            size_t new_cap = cap * 2;\n"
        "            if (new_cap > 1048576) new_cap = 1048576;\n"
        "            char *new_buf = (char *)realloc(buf, new_cap + 1);\n"
        "            if (!new_buf) break;\n"
        "            buf = new_buf;\n"
        "            cap = new_cap;\n"
        "        }\n"
        "        ssize_t n = recv(client_fd, buf + len, cap - len, 0);\n"
        "        if (n > 0) {\n"
        "            len += (size_t)n;\n"
        "            buf[len] = '\\0';\n"
        "            char *hdr_end = strstr(buf, \"\\r\\n\\r\\n\");\n"
        "            if (hdr_end != NULL) {\n"
        "                const char *cl_needle = \"Content-Length:\";\n"
        "                size_t nlen = strlen(cl_needle);\n"
        "                char *cl_ptr = NULL;\n"
        "                for (char *p = buf; p + nlen <= hdr_end; p++) {\n"
        "                    if (strncasecmp(p, cl_needle, nlen) == 0) { cl_ptr = p + nlen; break; }\n"
        "                }\n"
        "                if (cl_ptr) {\n"
        "                    while (*cl_ptr == ' ' || *cl_ptr == '\\t') cl_ptr++;\n"
        "                    long cl = strtol(cl_ptr, NULL, 10);\n"
        "                    if (cl > 0) {\n"
        "                        size_t body_start = (size_t)(hdr_end - buf) + 4;\n"
        "                        size_t body_recvd = (len > body_start) ? (len - body_start) : 0;\n"
        "                        if (body_recvd < (size_t)cl) {\n"
        "                            size_t req_cap = body_start + (size_t)cl;\n"
        "                            if (req_cap > cap && req_cap <= 1048576) {\n"
        "                                char *new_buf = (char *)realloc(buf, req_cap + 1);\n"
        "                                if (new_buf) { buf = new_buf; cap = req_cap; }\n"
        "                            }\n"
        "                            continue;\n"
        "                        }\n"
        "                    }\n"
        "                }\n"
        "                break;\n"
        "            }\n"
        "        } else if (n == 0) {\n"
        "            break;\n"
        "        } else {\n"
        "            if (errno == EINTR) continue;\n"
        "            break;\n"
        "        }\n"
        "    }\n"
        "    if (len == 0) {\n"
        "        free(buf);\n"
        "        return strdup(\"\");\n"
        "    }\n"
        "    buf[len] = '\\0';\n"
        "    return buf;\n"
        "}\n\n",
        NULL
    },
    {
        "net_send",
        "static inline int __cco_net_send(int client_fd, const char *data) {\n"
        "    if (client_fd < 0 || !data) return 0;\n"
        "    size_t len = strlen(data);\n"
        "    ssize_t total = 0;\n"
        "    while (total < (ssize_t)len) {\n"
        "        #ifdef MSG_NOSIGNAL\n"
        "        int flags = MSG_NOSIGNAL;\n"
        "        #else\n"
        "        int flags = 0;\n"
        "        #endif\n"
        "        ssize_t sent = send(client_fd, data + total, len - (size_t)total, flags);\n"
        "        if (sent < 0) {\n"
        "            if (errno == EINTR) continue;\n"
        "            break;\n"
        "        }\n"
        "        if (sent == 0) break;\n"
        "        total += sent;\n"
        "    }\n"
        "    return (int)total;\n"
        "}\n\n",
        NULL
    },
    {
        "net_close",
        "static inline void __cco_net_close(int fd) {\n"
        "    if (fd >= 0) close(fd);\n"
        "}\n\n",
        NULL
    },
    {
        "sleep_ms",
        "static inline void __cco_sleep_ms(int ms) {\n"
        "    if (ms > 0) {\n"
        "        struct timespec ts;\n"
        "        ts.tv_sec = ms / 1000;\n"
        "        ts.tv_nsec = (ms % 1000) * 1000000L;\n"
        "        nanosleep(&ts, NULL);\n"
        "    }\n"
        "}\n\n",
        NULL
    }
};

static const size_t PRELUDE_CHUNK_COUNT = sizeof(PRELUDE_CHUNKS) / sizeof(PRELUDE_CHUNKS[0]);

#endif // STDLIB_PRELUDE_H
