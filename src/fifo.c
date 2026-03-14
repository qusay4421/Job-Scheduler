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
static int next_to_evict = 0;

/* Initialize any data structures needed for this
 * replacement algorithm
 */
void fifo_init() {
    next_to_evict = 0;
}

/* Evict the oldest page*/
int fifo_evict() {
    int victim = next_to_evict;
    next_to_evict = (next_to_evict + 1) % memsize; // wrap around
    return victim;
}

// FIFO does not need to track references
void fifo_ref(pt_entry_t *p) {
    (void)p; // for unused variable warning
}

// No dynamic memory or structures to clean up
void fifo_cleanup() {}
