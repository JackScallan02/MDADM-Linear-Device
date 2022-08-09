#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;


int cache_create(int num_entries) {
	if (num_entries >= 2 && num_entries <= 4096 && cache == NULL) {
		cache = (cache_entry_t*)calloc(num_entries, sizeof(cache_entry_t));
		cache_size = num_entries;
		return 1;
		
	}
	
  	return -1;
}

int cache_destroy(void) {
	if (cache != NULL) {
		free(cache);
		cache = NULL;
		cache_size = 0;
		return 1;
	}
	
  	return -1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
	//Looks up block in the cache, then copies the block data into buf.
	if (buf == NULL || cache == NULL || cache_size == 0) {
		return -1;
	} else if (disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255) {
		return -1;
	}
	
	num_queries += 1;
	
	for (int i = 0; i < cache_size; i++) {
		if (cache[i].valid == true && cache[i].block_num == block_num && cache[i].disk_num == disk_num) {
			num_hits += 1;
			clock += 1;
			cache[i].access_time = clock;
			memcpy(buf, cache[i].block, 256);
			return 1;
		}
	}

		
	return -1;
		
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
	//Update block content in cache with new data from buf.
	if (buf == NULL || cache == NULL) {
		return;
	} else if (disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255) {
		return;
	}

	for (int i = 0; i < cache_size; i++) {
		if (cache[i].valid == true && cache[i].block_num == block_num && cache[i].disk_num == disk_num) {
			clock += 1;
			cache[i].access_time = clock;
			memcpy(cache[i].block, buf, 256);

		}
	}
}

int getLru() {
	//Returns the index of the least recently used block
	
	int min = cache[0].access_time;
	int minBlockIndex = 0;
	
	for (int i = 1; i < cache_size; i++) {
		if (cache[i].access_time < min) {
			min = cache[i].access_time;
			minBlockIndex = i;
		}
	}

	return minBlockIndex;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
	//Inserts block into cache and copies buf to the cache entry.
	if (buf == NULL || cache == NULL) {
		return -1;
	} else if (disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255) {
		return -1;
	}
	

	for (int i = 0; i < cache_size; i++) {
		if (cache[i].valid == true && cache[i].block_num == block_num && cache[i].disk_num == disk_num) { //If block is already in cache, no need to insert
			
			return -1;
		}
		
		if (cache[i].valid == false) { //We insert the block at this index
			memcpy(cache[i].block, buf, 256);
			clock += 1;
  			cache[i].access_time = clock;
  			cache[i].block_num = block_num;
  			cache[i].disk_num = disk_num;
  			cache[i].valid = true;
  			return 1;
		}
	}
	
	int LRU = getLru();
	memcpy(cache[LRU].block, buf, 256);
	clock += 1;
  	cache[LRU].access_time = clock;
  	cache[LRU].block_num = block_num;
  	cache[LRU].disk_num = disk_num;
  	cache[LRU].valid = true;
  	return 1;
}

bool cache_enabled(void) {
	if (cache_size > 2) {
		return true;
	}
	
  	return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
