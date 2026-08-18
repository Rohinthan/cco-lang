#include "../../src/map_runtime.h"
#include <assert.h>
#include <stdio.h>

static void test_int_map_basic(void) {
    __cco_map *m = __cco_map_new(0); // 0 = int key
    assert(m != NULL);
    assert(m->occupied == 0);

    // put entries
    m = __cco_map_put(m, (void *)(intptr_t)10, (void *)(intptr_t)100, NULL);
    m = __cco_map_put(m, (void *)(intptr_t)20, (void *)(intptr_t)200, NULL);
    m = __cco_map_put(m, (void *)(intptr_t)30, (void *)(intptr_t)300, NULL);

    assert(m->occupied == 3);
    assert(__cco_map_has(m, (void *)(intptr_t)10));
    assert(__cco_map_has(m, (void *)(intptr_t)20));
    assert(!__cco_map_has(m, (void *)(intptr_t)40));

    assert((intptr_t)__cco_map_get(m, (void *)(intptr_t)10) == 100);
    assert((intptr_t)__cco_map_get(m, (void *)(intptr_t)20) == 200);

    // remove entry
    void *rem = __cco_map_remove(m, (void *)(intptr_t)20);
    assert((intptr_t)rem == 200);
    assert(m->occupied == 2);
    assert(!__cco_map_has(m, (void *)(intptr_t)20));

    // get removed should still be present in probe sequence if tombstone
    m = __cco_map_put(m, (void *)(intptr_t)20, (void *)(intptr_t)250, NULL);
    assert(m->occupied == 3);
    assert((intptr_t)__cco_map_get(m, (void *)(intptr_t)20) == 250);

    __cco_map_free(m, NULL);
}

typedef struct {
    int x;
    int y;
} DummyObj;

static void dummy_free(void *ptr) {
    free(ptr);
}

static void test_str_map_class_vals(void) {
    __cco_map *m = __cco_map_new(1); // 1 = string key
    assert(m != NULL);

    DummyObj *d1 = (DummyObj *)malloc(sizeof(DummyObj));
    d1->x = 1; d1->y = 2;
    DummyObj *d2 = (DummyObj *)malloc(sizeof(DummyObj));
    d2->x = 3; d2->y = 4;

    m = __cco_map_put(m, (void *)"first", (void *)d1, dummy_free);
    m = __cco_map_put(m, (void *)"second", (void *)d2, dummy_free);

    assert(__cco_map_has(m, (void *)"first"));
    assert(__cco_map_has(m, (void *)"second"));

    DummyObj *got = (DummyObj *)__cco_map_get(m, (void *)"first");
    assert(got->x == 1 && got->y == 2);

    // overwrite existing key -> old dummy object should be freed by dummy_free!
    DummyObj *d1_new = (DummyObj *)malloc(sizeof(DummyObj));
    d1_new->x = 10; d1_new->y = 20;
    m = __cco_map_put(m, (void *)"first", (void *)d1_new, dummy_free);
    got = (DummyObj *)__cco_map_get(m, (void *)"first");
    assert(got->x == 10 && got->y == 20);

    // remove object -> caller takes ownership
    DummyObj *removed = (DummyObj *)__cco_map_remove(m, (void *)"second");
    assert(removed->x == 3 && removed->y == 4);
    free(removed);

    __cco_map_free(m, dummy_free);
}

static void test_rehash_stress(void) {
    __cco_map *m = __cco_map_new(1);
    char buf[64];
    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        m = __cco_map_put(m, (void *)buf, (void *)(intptr_t)(i * 10), NULL);
    }
    assert(m->occupied == 100);

    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        assert(__cco_map_has(m, (void *)buf));
        assert((intptr_t)__cco_map_get(m, (void *)buf) == i * 10);
    }

    __cco_map_free(m, NULL);
}

int main(void) {
    printf("Running Map Runtime Unit Tests...\n");
    test_int_map_basic();
    printf("[PASS] test_int_map_basic\n");
    test_str_map_class_vals();
    printf("[PASS] test_str_map_class_vals\n");
    test_rehash_stress();
    printf("[PASS] test_rehash_stress\n");
    printf("All Map Runtime tests passed!\n");
    return 0;
}
