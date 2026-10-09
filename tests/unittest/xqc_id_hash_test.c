/**
 * @copyright Copyright (c) 2022, Alibaba Group Holding Limited
 */

#include "xqc_id_hash_test.h"
#include "src/common/xqc_id_hash.h"
#include "src/common/xqc_memory_pool.h"
#include "src/common/xqc_hash.h"
#include <CUnit/CUnit.h>


void
xqc_test_id_hash()
{
    xqc_id_hash_table_t hash_tab;
    xqc_id_hash_init(&hash_tab, xqc_default_allocator, 100);

    xqc_id_hash_element_t e1 = {1, "hello"};
    xqc_id_hash_add(&hash_tab, e1);

    xqc_id_hash_element_t e2 = {3, "world"};
    xqc_id_hash_add(&hash_tab, e2);

    xqc_id_hash_element_t e3 = {5, "!"};
    xqc_id_hash_add(&hash_tab, e3);

    char *p1 = xqc_id_hash_find(&hash_tab, 3);
    CU_ASSERT(p1 != NULL);

    void *p2 = xqc_id_hash_find(&hash_tab, 4);
    CU_ASSERT(p2 == NULL);

    int ret = xqc_id_hash_delete(&hash_tab, 3);
    CU_ASSERT(ret == XQC_OK);

    void *p3 = xqc_id_hash_find(&hash_tab, 3);
    CU_ASSERT(p3 == NULL);

    ret = xqc_id_hash_delete(&hash_tab, 4);
    CU_ASSERT(ret != XQC_OK);


    xqc_id_hash_release(&hash_tab);
}


void
xqc_test_pool_large_alignment()
{
#ifdef XQC_PROTECT_POOL_MEM
    xqc_memory_pool_t *pool = xqc_create_pool(8192, XQC_FALSE);
#else
    xqc_memory_pool_t *pool = xqc_create_pool(8192);
#endif
    CU_ASSERT_FATAL(pool != NULL);
    for (size_t size = 4096; size < 8192; size += 37) {
        void *data = xqc_pcalloc(pool, size);
        CU_ASSERT_FATAL(data != NULL);
        CU_ASSERT((uintptr_t)data % XQC_ALIGNMENT == 0);
        ((unsigned char *)data)[size - 1] = 0xa5;
    }
    CU_ASSERT(xqc_palloc_large(pool, SIZE_MAX) == NULL);
    xqc_destroy_pool(pool);
}

void
xqc_test_murmur_high_bit_word()
{
    unsigned char data[] = {0x01, 0x23, 0x45, 0xe6};
    uint32_t k = UINT32_C(0xe6452301), h = 4;
    k *= UINT32_C(0x5bd1e995); k ^= k >> 24; k *= UINT32_C(0x5bd1e995);
    h *= UINT32_C(0x5bd1e995); h ^= k;
    h ^= h >> 13; h *= UINT32_C(0x5bd1e995); h ^= h >> 15;
    CU_ASSERT(xqc_murmur_hash2(data, sizeof(data)) == h);
}
