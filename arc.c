/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Bogdan Simion, Andrew Peterson, Karen Reid, Alexey Khrabrov, Vladislav Sytchenko
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2025 Bogdan Simion, Karen Reid
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pagetable.h"

extern int memsize;
extern struct frame *coremap;

/*
 * Keeps track of both recent and frequent pages.
 * Uses four lists: T1/T2 (real pages) and B1/B2 (ghost lists for history).
 * p adjusts dynamically to balance between recency and frequency.
 */

typedef struct node {
    int frame;
    struct node *prev;
    struct node *next;
} node_t;

static node_t *T1_head = NULL;  // recent pages
static node_t *T1_tail = NULL;
static node_t *T2_head = NULL;  // frequent pages  
static node_t *T2_tail = NULL;
static node_t *B1_head = NULL;  // ghost list for T1
static node_t *B1_tail = NULL;
static node_t *B2_head = NULL;  // ghost list for T2
static node_t *B2_tail = NULL;

static int p = 0;  // target size for T1
static int c;      // cache size

//  ----------------------- Helpers doubly linked list -----------------------

static void list_remove(node_t **head, node_t **tail, node_t *node) {
    if (node == NULL) return;
    
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (*head == node) *head = node->next;
    if (*tail == node) *tail = node->prev;
    node->prev = node->next = NULL;
}

static void list_add_front(node_t **head, node_t **tail, node_t *node) {
    if (node == NULL) return;
    
    node->next = *head;
    node->prev = NULL;
    if (*head) (*head)->prev = node;
    *head = node;
    if (!*tail) *tail = node;
}

static node_t* list_remove_back(node_t **head, node_t **tail) {
    if (!*tail) return NULL;
    node_t *node = *tail;
    list_remove(head, tail, node);
    return node;
}

static int list_size(node_t *head) {
    int count = 0;
    node_t *current = head;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

static node_t* find_in_list(node_t *head, int frame) {
    node_t *current = head;
    while (current) {
        if (current->frame == frame) return current;
        current = current->next;
    }
    return NULL;
}

static void free_list(node_t **head, node_t **tail) {
    node_t *current = *head;
    while (current) {
        node_t *next = current->next;
        free(current);
        current = next;
    }
    *head = *tail = NULL;
}

// ----------------------- End of Helpers -----------------------


/* The page to evict is chosen using the ARC algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int arc_evict() {
    int victim_frame = -1;
    node_t *victim_node = NULL;
    
    int T1_size = list_size(T1_head);
    int T2_size = list_size(T2_head);
    
    // decide which list to evict from
    if (T1_size > 0 && (T1_size > p || (T1_size == p && T2_size > 0))) {
        victim_node = list_remove_back(&T1_head, &T1_tail);
    } else {
        victim_node = list_remove_back(&T2_head, &T2_tail);
    }
    
    if (victim_node) {
        victim_frame = victim_node->frame;
        
        // move victim to corresponding ghost list
        if (victim_frame != -1) {
            if (find_in_list(T1_head, victim_frame) == victim_node) {
                list_add_front(&B1_head, &B1_tail, victim_node);
            } else {
                list_add_front(&B2_head, &B2_tail, victim_node);
            }
        }
    }
    
    // fallback if no victim found just in case
    if (victim_frame == -1) {
        for (int i = 0; i < memsize; i++) {
            if (coremap[i].in_use) {
                victim_frame = i;
                break;
            }
        }
    }
    
    assert(victim_frame != -1);
    return victim_frame;
}

/* This function is called on each access to a page to update any information
 * needed by the ARC algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void arc_ref(pt_entry_t *pte) {
    int frame = pte->frame >> PAGE_SHIFT;
    
    // check if page is in T1
    node_t *node = find_in_list(T1_head, frame);
    if (node) {
        // move from T1 to T2 (promote to frequently used)
        list_remove(&T1_head, &T1_tail, node);
        list_add_front(&T2_head, &T2_tail, node);
        return;
    }
    
    // check if page is in T2
    node = find_in_list(T2_head, frame);
    if (node) {
        // move to front of T2 (MRU position)
        list_remove(&T2_head, &T2_tail, node);
        list_add_front(&T2_head, &T2_tail, node);
        return;
    }
    
    // check if page is in B1 (ghost of recently evicted)
    node = find_in_list(B1_head, frame);
    if (node) {
        // increase target size for T1
        int B1_size = list_size(B1_head);
        int B2_size = list_size(B2_head);
        int delta = (B1_size >= B2_size) ? 1 : (B2_size / (B1_size > 0 ? B1_size : 1));
        p = (p + delta < c) ? p + delta : c;
        
        // nove from B1 to T2
        list_remove(&B1_head, &B1_tail, node);
        list_add_front(&T2_head, &T2_tail, node);
        
        // need to make space since we added to cache
        if (list_size(T1_head) + list_size(T2_head) >= c) {
            arc_evict();
        }

        return;
    }
    
    // check if page is in B2 (ghost of frequently evicted)
    node = find_in_list(B2_head, frame);
    if (node) {
        // decrease target size for T1
        int B1_size = list_size(B1_head);
        int B2_size = list_size(B2_head);
        int delta = (B2_size >= B1_size) ? 1 : (B1_size / (B2_size > 0 ? B2_size : 1));
        p = (p - delta > 0) ? p - delta : 0;
        
        // move from B2 to T2
        list_remove(&B2_head, &B2_tail, node);
        list_add_front(&T2_head, &T2_tail, node);
        
        // needd to make space since we added to cache
        if (list_size(T1_head) + list_size(T2_head) >= c) {
            arc_evict();
        }
        
        return;
    }
    
    // page not in any list so create new node and add to T1
    node = malloc(sizeof(node_t));
    if (!node) {
        fprintf(stderr, "ARC: Failed to allocate node\n");
        return;
    }
    node->frame = frame;
    node->prev = node->next = NULL;
    list_add_front(&T1_head, &T1_tail, node);
    
    // make space if cache is full
    if (list_size(T1_head) + list_size(T2_head) > c) {
        arc_evict();
    }
    
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void arc_init() {
    c = memsize;
    
    // initialize lists
    T1_head = T1_tail = NULL;
    T2_head = T2_tail = NULL; 
    B1_head = B1_tail = NULL;
    B2_head = B2_tail = NULL;
    
    // start with balanced target size
    p = c / 2;
}

/* Cleanup any data structures created in arc_init(). */
void arc_cleanup() {
    // free all nodes in all lists
    free_list(&T1_head, &T1_tail);
    free_list(&T2_head, &T2_tail);
    free_list(&B1_head, &B1_tail);
    free_list(&B2_head, &B2_tail);
}