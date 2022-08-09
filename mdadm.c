//CMPSC 311 SP22
//LAB 2

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "mdadm.h"
#include "jbod.h"
#include "cache.h"
#include "net.h"

struct {
  int mountStatus;
} jbod;

uint32_t make_operation(jbod_cmd_t cmd, int diskID, uint16_t reserved, int blockID) { //Formats the jbod operation correctly
	return cmd << 26 | diskID << 22 | reserved << 8 | blockID;
}

int mdadm_mount(void) {
	if (jbod.mountStatus == JBOD_ALREADY_MOUNTED) { //If jbod is already mounted
    	return -1;
    
	} else {
	
  		uint32_t op = make_operation(JBOD_MOUNT, 0, 0, 0);
    	uint32_t make_operation = jbod_client_operation(op, NULL);
    	
    	if (make_operation == -1) {
    		return -1;
    	}
    	
    	jbod.mountStatus = JBOD_MOUNT;
    	
    	return 1;
  }
  return 1;
}

int mdadm_unmount(void) {
	if (jbod.mountStatus != JBOD_ALREADY_UNMOUNTED) { //If it is mounted
  		uint32_t op = make_operation(JBOD_UNMOUNT, 0, 0, 0);
    	uint32_t make_operation = jbod_client_operation(op, NULL);
    	
    	if (make_operation == -1) {
    		return -1;
    	}
    	
    	jbod.mountStatus = JBOD_UNMOUNT;
    	
    	cache_destroy();
    	
    	return 1;
	}
  
  return -1;
}

	
int getDiskNum(uint32_t addr) {
	int diskNum = (int)(addr / (256 * 256));
	return diskNum;
}

int getBlockNum(uint32_t addr) {
	int blockNum = (int)((addr % (256 * 256)) / 256);
	return blockNum;
}

uint8_t getOffset(uint32_t addr) { //How many bytes from 0 we are
	uint8_t offset = (int)((addr % (256 * 256)) % 256);
	return offset;
}

int seek(int blockNum, int diskNum) { //Seeks to block and disk
	uint16_t reserved = 0;
  	uint32_t diskOp = make_operation(JBOD_SEEK_TO_DISK, diskNum, reserved, blockNum);
  	uint32_t blockOp = make_operation(JBOD_SEEK_TO_BLOCK, diskNum, reserved, blockNum);
  	
  	if (diskOp == -1 || blockOp == -1) {
  		return -1;
  	}
  	
  	jbod_client_operation(diskOp, NULL);
  	jbod_client_operation(blockOp, NULL);
  	return 1;
}


void readToBuffer(int diskNum, int blockNum, uint32_t len, uint8_t *buf, int bufferIndex, uint8_t offset) {
	//bufferIndex: Total number of bytes already read to buf
	//offset: how many bytes we are from 0
	
	uint16_t reserved = 0;
	uint8_t localBuffer[256];
	
	if (cache_enabled() == true) {
		
		if (cache_lookup(diskNum, blockNum, buf) == -1) { // Block is not in cache
			seek(blockNum, diskNum);
			uint32_t readOp = make_operation(JBOD_READ_BLOCK, diskNum, reserved, blockNum);
			jbod_client_operation(readOp, localBuffer);
			
			cache_insert(diskNum, blockNum, localBuffer);
			memcpy(buf + bufferIndex, localBuffer + offset, len);
		}

	} else { //Cache is disabled
		uint32_t readOp = make_operation(JBOD_READ_BLOCK, diskNum, reserved, blockNum);
		jbod_client_operation(readOp, localBuffer);
  		memcpy(buf + bufferIndex, localBuffer + offset, len);

	}

}




void writeToJbod(int diskNum, int blockNum, uint32_t len, const uint8_t *buf, int bufferIndex, uint8_t offset) {

	uint8_t tempBuf[256];
	uint16_t reserved = 0;
	
	if (cache_enabled() == true) {
		if (cache_lookup(diskNum, blockNum, tempBuf) == -1) { // Block is not in cache
		
			uint32_t readOp = make_operation(4, 0, 0, 0);
			jbod_client_operation(readOp, tempBuf);
			seek(blockNum, diskNum);
			
			memcpy(tempBuf + offset, buf + bufferIndex, len);
			cache_insert(diskNum, blockNum, tempBuf);
			
			uint32_t writeOp = make_operation(JBOD_WRITE_BLOCK, diskNum, reserved, blockNum);
			jbod_client_operation(writeOp, tempBuf);
			
			
		} else { //If block is found in cache

			memcpy(tempBuf + offset, buf + bufferIndex, len);
			cache_update(diskNum, blockNum, tempBuf);

			uint32_t writeOp = make_operation(JBOD_WRITE_BLOCK, diskNum, reserved, blockNum);
			jbod_client_operation(writeOp, tempBuf);

		}


	} else { //Cache is disabled
		uint32_t readOp = make_operation(4, 0, 0, 0);
		jbod_client_operation(readOp, tempBuf);
		seek(blockNum, diskNum);
	
		memcpy(tempBuf + offset, buf + bufferIndex, len);
		
		uint32_t writeOp = make_operation(JBOD_WRITE_BLOCK, diskNum, reserved, blockNum);
		jbod_client_operation(writeOp, tempBuf);
	}
	
}



int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
 	if (len + addr > 1048576) {
 		return -1;
 	} else if (jbod.mountStatus != JBOD_MOUNT) {
 		return -1;
 	} else if (!buf && len != 0) {
 		return -1;
 	} else if (len > 1024) {
 		return -1;
 	}
 
 
  	int diskNum = getDiskNum(addr);
  	int blockNum = getBlockNum(addr);
  	uint8_t offset = getOffset(addr); //How many bytes from 256 we are
  	
  	
  	
  	//Seek to Block & Disk
	seek(blockNum, diskNum);
	

	//If we are reading from a single block, and thats it
	if ((int)((offset + len) / 256) < 1) {
  		readToBuffer(diskNum, blockNum, len, buf, 0, offset);
  		return len;
  	}
  	

	readToBuffer(diskNum, blockNum, 256 - offset, buf, 0, offset);
  	blockNum++;
  	if (blockNum >= 256) { //We have reached a new disk
  		blockNum = 0;
		diskNum++;
		seek(blockNum, diskNum);
  	}
  	
  	
  	uint32_t tempLen = len - (256 - offset);
  	int bufferIndex = 256 - offset; //How many bytes we have added to the buffer so far
  	
  	
	while (tempLen > 256) { //Keep reading from blocks until we only need to copy part of a block, i.e., templen is less than 256
		readToBuffer(diskNum, blockNum, 256, buf, bufferIndex, 0);
  		
  		tempLen -= 256;
  		bufferIndex += 256;
  		blockNum++;
  		
  		if (blockNum >= 256) {
  			diskNum++;
  			blockNum = 0;
  			seek(blockNum, diskNum);
  		}
  	}
  	
  	
  	if (tempLen > 0) {
  		readToBuffer(diskNum, blockNum, tempLen, buf, bufferIndex, 0);
  	}
  	

	return len;
	
}


int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
	if (len + addr > 1048576) { //Out of bounds
		return -1;
	} else if (jbod.mountStatus != JBOD_MOUNT) { //Unmounted
    	return -1;
	} else if (!buf && len != 0) { //Buffer is NULL and len is non-zero
		return -1;
	} else if (len > 1024) { //Len can be of max 1024
		return -1;
	}
	
	int diskNum = getDiskNum(addr);
  	int blockNum = getBlockNum(addr);
  	uint8_t offset = getOffset(addr); //How many bytes from 0 we are
	
	
	
	seek(blockNum, diskNum);

	
	
	//If we are writing to a single block
	if ((int)((offset + len) / 256) < 1) {
		
		writeToJbod(diskNum, blockNum, len, buf, 0, offset);
		blockNum++;
  		return len;
	}
	
	
	writeToJbod(diskNum, blockNum, 256 - offset, buf, 0, offset);
  	blockNum++;

  	
  	
  	if (blockNum >= 256) { //We have reached a new disk
		diskNum++;
		blockNum = 0;
		seek(blockNum, diskNum);
  	}
  	
  	
  	uint32_t tempLen = len - (256 - offset);
  	int bufferIndex = 256 - offset; //How many bytes we have added to the buffer so far
  	
  	
	while (tempLen > 256) { //Keep writing to blocks until we only need to write to part of a block, i.e., templen is less than 256
		writeToJbod(diskNum, blockNum, 256, buf, bufferIndex, 0);
  		
  		tempLen -= 256;
  		bufferIndex += 256;
  		blockNum++;
  		
  		if (blockNum >= 256) {
  			blockNum = 0;
  			diskNum++;
  			seek(blockNum, diskNum);
  		}
  	}
  	
  	if (tempLen > 0) {
  		writeToJbod(diskNum, blockNum, tempLen, buf, bufferIndex, 0);
  	}
  	
	
	return len;
}


