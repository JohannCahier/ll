/**
 * Thread-safe linked-list data-structure for C.
 *
 * See `../README.md` and `main()` in this file for usage.
 *
 * @file ll.c contains the implementatons of the functions outlined in `ll.h` as well as
 * all the functions necessary to manipulate and handle nodes (which are not exposed to
 * the user).
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "ll.h"

/* macros */

// for locking and unlocking rwlocks along with `locktype_t`
#define RWLOCK(item, locktype) ((locktype) == l_read)          \
                           ? pthread_rwlock_rdlock(&(item->m)) \
                           : pthread_rwlock_wrlock(&(item->m))
#define RWUNLOCK(item) pthread_rwlock_unlock(&(item->m));


// shorthand for locking a list mutex and checking list's validity
// locktype: is the locktype_t wanted by the function that checks the list
// list:     the list to be checked
// retval:   is the value to be returned by the function that uses the macro,
//           when the check fails
#define CHECK_VALID(list, locktype, retval) {                \
                           valid_flag_t flag;                \
                           RWLOCK(list, locktype);           \
                           flag = list->valid_flag;          \
                           if(flag != VALID) {RWUNLOCK(list);\
                                              return retval;}\
                   } while(0);

/* type definitions */

typedef enum locktype locktype_t;

// locktype enumerates the two typs of rw locks. This isused in the macros above for
// simplifying all the locking/unlocking that goes on.
enum locktype {
    l_read,
    l_write
};

// ll_node models a linked-list node
struct ll_node {
    // pointer to the value at the node
    void *val;

    // pointer to the next node
    ll_node_t *nxt;

    // rw mutex
    pthread_rwlock_t m;
};

/**
 * @function ll_new
 *
 * Allocates a new linked list and initalizes its values.
 *
 * @param val_teardown - the `val_teardown` attribute of the linked list will be set to this
 *
 * @returns a pointer to a new linked list
 */
ll_t *ll_new(gen_fun_t val_teardown) {
    ll_t *list = (ll_t *)malloc(sizeof(ll_t));
    list->hd = NULL;
    list->len = 0;
    list->val_teardown = val_teardown;
    list->valid_flag = VALID;
    pthread_rwlock_init(&list->m, NULL);

    return list;
}


/**
 * @function ll_clear
 *
 * Traverses the whole linked list and deletes/deallocates the nodes.
 * Invalidates the list (no further operation, other than ll_delete, won't succeed).
 *
 * @param list - the linked list
 */
void ll_clear(ll_t *list) {
    CHECK_VALID(list, l_write, );
    ll_node_t *node = list->hd;
    ll_node_t *next = node;

    while (next != NULL) {
        node = next;
        RWLOCK(node, l_write);
        list->val_teardown(node->val);
        next = node->nxt;
        RWUNLOCK(node);
        pthread_rwlock_destroy(&(node->m));
        free(node);
        (list->len)--;
    }
    assert(list->len == 0);
    list->hd = NULL;
    list->val_teardown = NULL;
    list->val_printer = NULL;
    list->valid_flag = INVALID;
    RWUNLOCK(list);
    pthread_rwlock_destroy(&(list->m));

}

/**
 * @function ll_delete
 *
 * Calls ll_clear() to deallocates the nodes then frees the linked list itself.
 *
 * @param list - the linked list
 */
void ll_delete(ll_t *list) {
    if(list->valid_flag != INVALID) {
        ll_clear(list);
    }

    free(list);
}


/**
 * @function ll_length
 *
 * get the number of items stored in the list
 *
 * @param list - the linked list
 *
 * @return list lengtth, or -1 if list is not valid.

 */
int ll_length(ll_t *list) {
    int len = -1;

    CHECK_VALID(list, l_read, -1);
    len = list->len;
    RWUNLOCK(list);
    return len;
}

/**
 * @function ll_new_node
 *
 * Makes a new node with the given value.
 *
 * @param val - a pointer to the value
 *
 * @returns a pointer to the new node
 */
ll_node_t *ll_new_node(void *val) {
    ll_node_t *node = (ll_node_t *)malloc(sizeof(ll_node_t));
    node->val = val;
    node->nxt = NULL;
    pthread_rwlock_init(&node->m, NULL);

    return node;
}

/**
 * @function ll_select_n_min_1
 *
 * Actually selects the n - 1th element. Inserting and deleting at the front of a
 * list do NOT really depend on this.
 *
 * @param list - the linked list
 * @param node - a pointer to set when the node is found
 * @param n - the index
 *
 * @returns 0 if successful, -1 otherwise
 */
int ll_select_n_min_1(ll_t *list, ll_node_t **node, int n, locktype_t lt) {
    if (n < 0) // don't check against list->len because threads can add length
        return -1;

    if (n == 0)
        return 0;

    // n > 0
    CHECK_VALID(list, lt, -1);
    *node = list->hd;
    if (*node == NULL) { // list is empty
        assert(list->len == 0);
        RWUNLOCK(list);
        return -1;
    }

    RWLOCK((*node), lt);
    ll_node_t *last;
    for (; n > 1; n--) {
        last = *node;
        *node = last->nxt;
        if (*node == NULL) { // happens when another thread deletes the end of a list
            RWUNLOCK(last);
            RWUNLOCK(list);
            return -1;
        }

        RWLOCK((*node), lt);
        RWUNLOCK(last);
    }
    // RWUNLOCK(list->m); keep the list locked

    return 0;
}

/**
 * @function ll_insert_n
 *
 * Inserts a value at the nth position of a linked list.
 *
 * @param list - the linked list
 * @param val - a pointer to the value
 * @param n - the index
 *
 * @returns 0 if successful, -1 otherwise
 */
int ll_insert_n(ll_t *list, void *val, int n) {
    ll_node_t *new_node = ll_new_node(val);

    if (n == 0) { // nth_node is list->hd
        CHECK_VALID(list, l_write, -1);
        new_node->nxt = list->hd;
        list->hd = new_node;
    } else {
        ll_node_t *nth_node;
        // ll_select_n_min_1 checks and locks the list for us (on success)
        if (ll_select_n_min_1(list, &nth_node, n, l_write)) {
            free(new_node);
            return -1;
        }
        new_node->nxt = nth_node->nxt;
        nth_node->nxt = new_node;
        RWUNLOCK(nth_node);
    }

    (list->len)++;
    RWUNLOCK(list);

    return list->len;
}

/**
 * @function ll_insert_first
 *
 * Just a wrapper for `ll_insert_n` called with 0.
 *
 * @param list - the linked list
 * @param val - a pointer to the value
 *
 * @returns the new length of thew linked list on success, -1 otherwise
 */
int ll_insert_first(ll_t *list, void *val) {
    return ll_insert_n(list, val, 0);
}

/**
 * @function ll_insert_last
 *
 * Just a wrapper for `ll_insert_n` called with the index being the length of the linked list.
 *
 * @param list - the linked list
 * @param val - a pointer to the value
 *
 * @returns the new length of thew linked list on success, -1 otherwise
 */
int ll_insert_last(ll_t *list, void *val) {
    return ll_insert_n(list, val, list->len);
}

/**
 * @function ll_remove_n
 *
 * Removes the nth element of the linked list.
 *
 * @param list - the linked list
 * @param n - the index
 *
 * @returns the new length of thew linked list on success, -1 otherwise
 */
int ll_remove_n(ll_t *list, int n) {
    ll_node_t *tmp;
    if (n == 0) {
        CHECK_VALID(list, l_write, -1);
        tmp = list->hd;
        list->hd = tmp->nxt;
    } else {
        ll_node_t *nth_node;
        // ll_select_n_min_1 checks and locks the list for us (on success)
        if (ll_select_n_min_1(list, &nth_node, n, l_write)) {
            // that node index doesn't exist
            return -1;
        }

        tmp = nth_node->nxt;
        nth_node->nxt = nth_node->nxt == NULL ? NULL : nth_node->nxt->nxt;
        RWUNLOCK(nth_node);
    }

    (list->len)--;
    list->val_teardown(tmp->val);

    RWUNLOCK(list);
    pthread_rwlock_destroy(&(tmp->m));
    free(tmp);

    return list->len;
}

/**
 * @function ll_remove_first
 *
 * Wrapper for `ll_remove_n`.
 *
 * @param list - the linked list
 *
 * @returns 0 if successful, -1 otherwise
 */
int ll_remove_first(ll_t *list) {
    return ll_remove_n(list, 0);
}

/**
 * @function ll_pop_first
 *
 * returns a pointer to data of the first node and removes it.
 * NOTE : the caller takes the owner ship of the pointer
 *        (and thus, needs to call the teardown function on it)
 *
 * @param list - the linked list
 *
 * @returns pointer to data or NULL
 */
void *ll_pop_first(ll_t *list) {
    void *data = NULL;

    CHECK_VALID(list, l_write, NULL);
    ll_node_t *node = list->hd;
    if (node != NULL) {
        data = node->val;
        list->len--;
        list->hd = node->nxt;
        pthread_rwlock_destroy(&(node->m));
        free(node);
    }
    RWUNLOCK(list);

    return data;
}


/**
 * @function ll_remove_search
 *
 * Removes the first item in the list whose value returns 1 if `cond` is called on it.
 *
 * @param list - the linked list
 * @param cond - a function that will be called on the values of each node. It should
 * return 1 of the element matches.
 *
 * @returns the new length of thew linked list on success, -1 otherwise
 */
int ll_remove_search(ll_t *list, int cond(void *)) {
    CHECK_VALID(list, l_write, -1);

    ll_node_t *last = NULL;
    ll_node_t *node = list->hd;
    while ((node != NULL) && !(cond(node->val))) {
        last = node;
        node = node->nxt;
    }

    if (node == NULL) {
        return -1;
    } else if (node == list->hd) {
        list->hd = node->nxt;
    } else {
        RWLOCK(last, l_write);
        last->nxt = node->nxt;
        RWUNLOCK(last);
    }

    list->val_teardown(node->val);
    pthread_rwlock_destroy(&(node->m));
    (list->len)--;
    RWUNLOCK(list);

    free(node); // let's chat on IRC !

    return list->len;
}

/**
 * @function ll_get_n
 *
 * Gets the value of the nth element of a linked list.
 *
 * @param list - the linked list
 * @param n - the index
 *
 * @returns the `val` attribute of the nth element of `list`.
 */
void *ll_get_n(ll_t *list, int n) {
    ll_node_t *node = NULL;
    void *val = NULL;
    // ll_select_n_min_1 chacks and locks the list on our behalf
    if (ll_select_n_min_1(list, &node, n + 1, l_read)) {
        return NULL;
    }
    val = node->val;
    RWUNLOCK(node);
    RWUNLOCK(list);
    return val;
}

/**
 * @function ll_get_first
 *
 * Wrapper for `ll_get_n`.
 *
 * @param list - the linked list
 *
 * @returns the `val` attribute of the first element of `list`.
 */
void *ll_get_first(ll_t *list) {
    return ll_get_n(list, 0);
}


static void _ll_map_internal(ll_t *list, gen_fun_t f) {
    ll_node_t *node = list->hd;

    while (node != NULL) {
        // f() may alterate values, so "lock write", just in case of...
        RWLOCK(node, l_write);
        ll_node_t *next = node->nxt;
        f(node->val);
        RWUNLOCK(node);

        node = next;
    }
}

/**
 * @function ll_map
 *
 * Calls a function on the value of every element of a linked list.
 *
 * @param list - the linked list
 * @param f - the function to call on the values.
 */
void ll_map(ll_t *list, gen_fun_t f) {
    CHECK_VALID(list, l_read, );

    _ll_map_internal(list, f);

    RWUNLOCK(list);
}

/**
 * @function ll_print
 *
 * If `val_printer` has been set on the linked list, that function is called on the values
 * of all the elements of the linked list.
 *
 * @param list - the linked list
 */
void ll_print(ll_t list) {
    int len = -1;

    CHECK_VALID((&list), l_read, );
    len = list.len;

    if (list.val_printer != NULL) {
        printf("(ll:");
        _ll_map_internal(&list, list.val_printer);
        printf("), length: %d\n", len);
    }
    RWUNLOCK((&list));
}

/**
 * @function ll_no_teardown
 *
 * A generic taredown function for values that don't need anything done.
 *
 * @param n - a pointer
 */
void ll_no_teardown(void *n) {
    n += 0; // compiler won't let me just return
}


/**
 * @function ll_find
 *
 * Generically searches for the first node that matches a reference value.
 *
 * @param list - the linked list
 * @param comparator - a function that will be called on the values of each node. (It should return 0 when the element matches the reference value)
 * @param ref_value - reference value passed to the comparator
 *
 * @returns pointer to value of the first matching node on success, NULL otherwise
 */
void* ll_find(ll_t *list, comp_fun_t comparator, const void *ref_value) {
//     int count = 0;

    CHECK_VALID(list, l_read, NULL);
    ll_node_t *node = list->hd;
    while ((node != NULL) && (comparator(node->val, ref_value) != 0)) {
        node = node->nxt;
//         count++;
    }
    RWUNLOCK(list);

    return (node == NULL)? NULL : node->val;
}


/**
 * @function ll_remove_find
 *
 * More generic replacement for ll_remove_search().
 *
 * @param list - the linked list
 * @param comparator - see ll_find() for details on comparator
 * @param ref_value - reference value passed to the comparator
 *
 * @returns the new length of thew linked list on success, -1 otherwise
 */
int ll_remove_find(ll_t *list, comp_fun_t comparator, const void *ref_value) {

    int new_len = -1;
    ll_node_t *last = NULL;
    CHECK_VALID(list, l_write, -1);
    ll_node_t *node = list->hd;

    while ((node != NULL) && (comparator(node->val, ref_value) != 0)) {
        last = node;
        node = node->nxt;
    }

    if (node == NULL) {
        return -1;
    } else if (node == list->hd) {
        list->hd = node->nxt;
    } else {
        RWLOCK(last, l_write);
        last->nxt = node->nxt;
        RWUNLOCK(last);
    }

    list->val_teardown(node->val);
    pthread_rwlock_destroy(&(node->m));
    free(node);
    (list->len)--;
    new_len = list->len;
    RWUNLOCK(list);

    return new_len;
}






#ifdef LL
/* this following code is just for testing this library */

void num_teardown(void *n) {
    *(int *)n *= -1; // just so we can visually inspect removals afterwards
}

void num_printer(void *n) {
    printf(" %d", *(int *)n);
}

int num_equals_3(void *n) {
    return *(int *)n == 3;
}

int num_equals(const void *n, const void *ref) {
    return *(const int *)n - *(const int *)ref;
}

int main() {
    int *_n; // for storing returned ones
    int test_count = 1;
    int fail_count = 0;
    int a = 0;
    int b = 1;
    int c = 2;
    int d = 3;
    int e = 4;
    int f = 5;
    int g = 6;
    int h = 3;
    int i = 3;

    ll_t *list = ll_new(num_teardown);
    list->val_printer = num_printer;

    ll_insert_first(list, &c); // 2 in front

    _n = (int *)ll_get_first(list);
    if (!(*_n == c)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, c, *_n);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    if (list->len != 1) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, 1, list->len);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    ll_insert_first(list, &b); // 1 in front
    ll_insert_first(list, &a); // 0 in front -> 0, 1, 2

    _n = (int *)ll_get_first(list);
    if (!(*_n == a)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, a, *_n);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    if (!(list->len == 3)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, 3, list->len);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    ll_insert_last(list, &d); // 3 in back
    ll_insert_last(list, &e); // 4 in back
    ll_insert_last(list, &f); // 5 in back

    _n = (int *)ll_get_n(list, 5);
    if (!(*_n == f)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, f, *_n);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    if (!(list->len == 6)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, 6, list->len);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    ll_insert_n(list, &g, 6); // 6 at index 6 -> 0, 1, 2, 3, 4, 5, 6

    int _i;
    for (_i = 0; _i < list->len; _i++) { // O(n^2) test lol
        _n = (int *)ll_get_n(list, _i);
        if (!(*_n == _i)) {
            fail_count++;
            fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", 1, _i, *_n);
        } else
            fprintf(stderr, "PASS Test %d!\n", test_count);
        test_count++;
    }

    // (ll: 0 1 2 3 4 5 6), length: 7

    ll_remove_first(list);                // (ll: 1 2 3 4 5 6), length: 6
    ll_remove_n(list, 1);                 // (ll: 1 3 4 5 6),   length: 5
    ll_remove_n(list, 2);                 // (ll: 1 3 5 6),     length: 4
    ll_remove_n(list, 5);                 // (ll: 1 3 5 6),     length: 4; does nothing
    ll_remove_search(list, num_equals_3); // (ll: 1 5 6),       length: 3
    ll_insert_first(list, &h);            // (ll: 3 1 5 6),     length: 5
    ll_insert_last(list, &i);             // (ll: 3 1 5 6 3),   length: 5
    ll_remove_search(list, num_equals_3); // (ll: 1 5 6 3),     length: 4
    ll_remove_search(list, num_equals_3); // (ll: 1 5 6),       length: 3

    int dummy_value = 42;
    fprintf(stderr, "Get position of value 5: %p\n", ll_find(list, num_equals, &f));
    fprintf(stderr, "Get position of value 42: %p\n", ll_find(list, num_equals, &dummy_value));

    ll_remove_find(list, num_equals, &f);  // (ll: 1 6),         length: 2

    ll_print(*list);

    fprintf(stderr, "Clear list\n");
    ll_clear(list);
    // following operation should have no effect (list is INVALID)
    ll_insert_last(list, &h);
    ll_insert_last(list, &i);
    ll_remove_first(list);
    ll_print(*list);

    ll_delete(list);

    if (fail_count) {
        fprintf(stderr, "FAILED %d tests of %d.\n", fail_count, test_count);
        return fail_count;
    }

    fprintf(stderr, "PASSED all %d tests!\n", test_count);
}
#endif
