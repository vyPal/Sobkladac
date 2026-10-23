/**
 * @file test_hashtable.c
 * Project: IFJ26 compiler
 * @brief hashtable unit tests
 * @author David Sobek <xsobekd00>
 *
 * Tests for the hashtable in hashtable.c
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "hashtable.h"
#include "tst.h"

tstsuite("HashTable", ) {
  tstcase("htCreate", ) {
    HashTable *ht = htCreate(0);
    tstcheck(ht != NULL, "hashtable should be created");
    tstcheck(htCapacity(ht) == 256, "default size is 256");
    ht = NULL;
    ht = htCreate(16);
    tstcheck(ht != NULL, "hashtable should be created");
    tstcheck(htCapacity(ht) == 16, "size should be 16");
  }

  tstcase("htInsert", ) {
    HashTable *ht = htCreate(0);
    int a = 10;
    int b = 20;
    int *pa = &a;
    int *pb = &b;
    tstcheck(htInsert(ht, "key1", pa) == true, "key1 should be added");
    ;
    tstcheck(htInsert(ht, "key2", pb) == true, "key2 should be added");
    ;
    tstcheck(htSize(ht) == 2, "should contain 2 keys");
    tstcheck(htContains(ht, "key1") == true, "key1 should exist");
    tstcheck(*(int *)htGet(ht, "key1") == 10, "key1 should be 10");
    tstcheck(htContains(ht, "key2") == true, "key2 should exist");
    tstcheck(*(int *)htGet(ht, "key2") == 20, "key2 should be 20");
    tstcheck(htContains(ht, "key3") == false, "key3 should not exist");
    tstcheck(htInsert(ht, "key1", pb) == true, "key1 should be rewritten");
    ;
    tstcheck(*(int *)htGet(ht, "key1") == 20, "key1 should be 20");
  }

  tstcase("htContains", ) {
    HashTable *ht = htCreate(0);
    int a = 10;
    int b = 20;
    int *pa = &a;
    int *pb = &b;
    htInsert(ht, "key1", pa);
    htInsert(ht, "key2", pb);
    tstcheck(htContains(ht, "key1") == true, "key1 should exist");
    tstcheck(htContains(ht, "key2") == true, "key2 should exist");
    tstcheck(htContains(ht, "key3") == false, "key3 should not exist");
    htRemove(ht, "key1");
    tstcheck(htContains(ht, "key1") == false, "key1 should no longer exist");
  }

  tstcase("htGet", ) {
    HashTable *ht = htCreate(0);
    int a = 10;
    int b = 20;
    int *pa = &a;
    int *pb = &b;
    htInsert(ht, "key1", pa);
    htInsert(ht, "key2", pb);
    tstcheck(*(int *)htGet(ht, "key1") == 10, "key1 should be 10");
    tstcheck(*(int *)htGet(ht, "key2") == 20, "key1 should be 20");
  }

  tstcase("htRemove", ) {
    HashTable *ht = htCreate(0);
    int a = 10;
    int b = 20;
    int *pa = &a;
    int *pb = &b;
    htInsert(ht, "key1", pa);
    htInsert(ht, "key2", pb);
    tstcheck(htSize(ht) == 2, "should contain 2 keys");
    tstcheck(htRemove(ht, "key1") == true, "key1 should be removed");
    tstcheck(htSize(ht) == 1, "should contain 1 key");
    tstcheck(htRemove(ht, "key3") == false, "key3 should not exist");
  }

  tstcase("htSize", ) {
    HashTable *ht = htCreate(64);
    int a = 10;
    int *pa = &a;
    tstcheck(htSize(ht) == 0, "should contain 0 keys");
    htInsert(ht, "key1", pa);
    htInsert(ht, "key2", pa);
    tstcheck(htSize(ht) == 2, "should contain 2 keys");
    htRemove(ht, "key1");
    htRemove(ht, "key2");
    tstcheck(htSize(ht) == 0, "should contain 0 keys");
  }

  tstcase("htCapacity", ) {
    HashTable *ht = htCreate(64);
    tstcheck(htCapacity(ht) == 64, "should contain 64 buckets");
  }

  tstcase("htResize", ) {
    HashTable *ht = htCreate(1);
    int a = 10;
    int *pa = &a;
    tstcheck(htCapacity(ht) == 1, "should contain 1 bucket");
    htInsert(ht, "key1", pa);
    tstcheck(htCapacity(ht) == 2, "should contain 2 buckets");
    htInsert(ht, "key2", pa);
    tstcheck(htCapacity(ht) == 4, "should contain 4 buckets");
  }
}
