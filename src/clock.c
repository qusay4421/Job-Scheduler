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
static int clock_hand = 0;
static int *ref_bits = NULL;

/* The page to evict is chosen using the CLOCK algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
    while (1) {
        // skip unused frames (shouldnt happen but just incase)
        if (!coremap[clock_hand].in_use) {
            clock_hand = (clock_hand + 1) % memsize;
            continue;
        }
        
        if (ref_bits[clock_hand]) {
            // give it a second chance
            ref_bits[clock_hand] = 0;
            clock_hand = (clock_hand + 1) % memsize;
        } else {
            // this one loses its turn
            int victim = clock_hand;
            clock_hand = (clock_hand + 1) % memsize;
            return victim;
        }
    }
}

// Mark a page as recently used when it’s accessed
void clock_ref(pt_entry_t *pte) {
	int frame = pte->frame >> PAGE_SHIFT;
    ref_bits[frame] = 1;
}

// Initialize any data structures needed
void clock_init() {
	clock_hand = 0;
    ref_bits = calloc(memsize, sizeof(int));
}

// Free memory used by CLOCK after simulation
void clock_cleanup() {
	free(ref_bits);
    ref_bits = NULL;
}
