/**
 * Thread-safe linked-list data-structure for C.
 *
 * See `../README.md` and `main()` in `ll.c` for usage.
 *
 * @file ll.h contains the API for using this data-structure.
 *
 * @author r-medina
 *
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 r-medina
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LL_H
#define LL_H

#include <pthread.h>

/* type definitions */

// useful for casting
typedef void (*gen_fun_t)(void *);

// comparator : implementation should return "true" (0) if both values are considered as equal.
typedef int (*comp_fun_t)(const void *, const void *);

// linked list
typedef struct ll ll_t;

// linked list node
typedef struct ll_node ll_node_t;

typedef enum {
    INVALID = 0,
    VALID = 1,
} valid_flag_t;

// linked list
struct ll {
    // running length
    int len;

    // pointer to the first node
    ll_node_t *hd;

    // mutex for thread safety
    pthread_rwlock_t m;

    // a function that is called every time a value is deleted
    // with a pointer to that value
    gen_fun_t val_teardown;

    // a function that can print the values in a linked list
    gen_fun_t val_printer;

    // a flag that says if the list is valid
    valid_flag_t valid_flag;
};

/* function prototypes */

// returns a pointer to an allocated linked list.
// needs a taredown function that is called with
// a pointer to the value when it is being deleted.
ll_t *ll_new(gen_fun_t val_teardown);

// traverses the linked list, deallocating everything (including `list`)
void ll_delete(ll_t *list);

// traverses the linked list, deallocating everything (excluding `list`).
// list in marked as 'invalid' (no futher operation on the list will succeed)
// Once all thread are canceled/joint
void ll_clear(ll_t *list);


// return list len (with a lock read),
// or -1 if list is invalid.
int ll_length(ll_t *list);

// inserts a value into the linked list at position `n`. acceptable values for n are `0`
// (puts it in first) to `list->len` (puts it in last).
// returns the new length of the linked list if successful, -1 otherwise
int ll_insert_n(ll_t *list, void *val, int n);

// puts a value at the front of the linked list.
// returns the new length of the linked list if successful, -1 otherwise
int ll_insert_first(ll_t *list, void *val);

// puts a value at the end of the linked list.
// returns the new length of the linked list if successful, -1 otherwise
int ll_insert_last(ll_t *list, void *val);

// removes the value at position n of the linked list.
// returns the new length of the linked list if successful, -1 otherwise
int ll_remove_n(ll_t *list, int n);

// removes the value at the front of the linked list.
// returns the new length of the linked list if successful, -1 otherwise
int ll_remove_first(ll_t *list);

// given a function that tests the values in the linked list, the first element that
// satisfies that function is removed.
// returns the new length of the linked list if successful, -1 otherwise
int ll_remove_search(ll_t *list, int cond(void *));

// returns a pointer to the `n`th value in the linked list.
// returns `NULL` if unsuccessful
void *ll_get_n(ll_t *list, int n);

// returns a pointer to the first value in a linked list.
// `NULL` if empty
void *ll_get_first(ll_t *list);

// returns a pointer to the first value in a linked list, and remove it.
// `NULL` if empty
void *ll_pop_first(ll_t *list);


// runs f on all values of list
void ll_map(ll_t *list, gen_fun_t f);

// goes through all the values of a linked list and calls `list->val_printer` on them
void ll_print(ll_t list);

// a generic taredown function for values that don't need anything done
void ll_no_teardown(void *n);


// Generically searches for the first node that matches a reference value.
// comparator callback is used to check the node's value matches or not the given ref_value.
// NOTE : comparator should return 0 on matches (any other value on mismatch).
// returns a pointer to the value of first node that "matches" the given value, NULL otherwise
void* ll_find(ll_t *list, comp_fun_t comparator, const void *ref_value);

// More generic replacement for ll_remove_search().
// Use comparator callback to check matches just like ll_find()
// Returns the new length of the linked list if successful, -1 otherwise
int ll_remove_find(ll_t *list, comp_fun_t comparator, const void *ref_value);

// LL_H
#endif
