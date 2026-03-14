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
#include <unistd.h>
#include "pagetable.h"

extern int memsize;
extern struct frame *coremap;

/*
 * LRU (Least Recently Used) page replacement.
 * Keeps pages in a doubly linked list so we can move them quickly when used.
 * The head is the most recently used page, and the tail is the one to evict.
 */

typedef struct Node {
    int frame;
    struct Node* prev;
    struct Node* next;
} Node;

static Node *head = NULL;  // most recently used
static Node *tail = NULL;  // least recently used
static Node **frame_map;   // frame -> list nod
static int initialized = 0;

// ---------------------- Helpers -----------------------

// Add a frame to the front (MRU position)
void insert_head(Node* node) {
    node->prev = NULL;
    node->next = head;
    if (head)
        head->prev = node;
    head = node;
    if (!tail)
        tail = node;
}

// Remove a node from wherever it is in the list.
void remove_node(Node* node) {
    if (node->prev)
        node->prev->next = node->next;
    else
        head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        tail = node->prev;
    node->prev = node->next = NULL;
}

// ---------------------- Helpers End -----------------------

/* The page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int lru_evict() {
    if (!tail) {
        fprintf(stderr, "LRU error: tried to evict from an empty list\n");
        exit(1);
    }

    int victim = tail->frame;
    Node* victim_node = frame_map[victim];

    remove_node(victim_node);
    free(victim_node);
    frame_map[victim] = NULL;

    return victim;
}

/* This function is called on each access to a page to update any information
 * needed by the LRU algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pt_entry_t* p) {
    int frame = p->frame >> PAGE_SHIFT; // get frame index
    Node* node = frame_map[frame];

    if (!node) {
        // create a new node on first reference
        node = malloc(sizeof(Node));
        node->frame = frame;
        node->prev = node->next = NULL;
        frame_map[frame] = node;
        insert_head(node);
    } else {
        // move to head since already in list
        remove_node(node);
        insert_head(node);
    }
}


// Initialize any data structures needed
void lru_init() {
    if (initialized)
        return;
    initialized = 1;

    frame_map = calloc(memsize, sizeof(Node*));
    if (!frame_map) {
        fprintf(stderr, "LRU: memory allocation failed\n");
        exit(1);
    }

    head = tail = NULL;
}

// Free all memory used by LRU before exit
void lru_cleanup() {
    Node* curr = head;
    while (curr) {
        Node* next = curr->next;
        free(curr);
        curr = next;
    }

    free(frame_map);
    head = tail = NULL;
    frame_map = NULL;
    initialized = 0;
}
