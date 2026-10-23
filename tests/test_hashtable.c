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
  }
  /*
  tstcase("htInsert", ) {

  }

  tstcase("htContains", ) {

  }

  tstcase("htGet", ) {

  }

  tstcase("htRemove", ) {

  }

  tstcase("htSize", ) {

  }

  tstcase("htCapacity", ) {

  }
  */
}
