// CacheController.c
//
// Authors: Alex Olson, Phong Nguyen, Ali Boshehri, Adel Alkharraz, Jennifer Lara
// Class: ECE 585
// Term: Fall 2018
// Group: 11
//
// This file contains the source code for a cache controller of a  split 4-way set 
// associative instruction cache and an 8-way set associative data cache.
//
/********************************************************************************
 *  			     DECLARATIONS
 * *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>

#define TAG 12
#define SET 14
#define BYTE 6
#define SETMASK 0x000FFFFF
#define BYTEMASK 0x0000003F

/* TAG SET BYTE caclulations references from addr
 	tag = addr >> (BYTE + SET);
	set = (addr & SETMASK) >> BYTE;
	byte = addr & BYTEMASK;
 */

/* Function declarations */
int parser(char *filename);
void reset_cache_controller(void);
void LRU_update(unsigned int slot);
int read(unsigned int addr);
int write(unsigned int addr);
int snooping(unsigned int addr);
void print_cache(unsigned int addr);
int fetch(unsigned int addr);
int invalidate(unsigned int addr);
int matching_tag_data(unsigned int tag);
int matching_tag_inst(unsigned int tag);
int search_LRU_data(void);
int search_LRU_inst(void);
int check_for_invalid_MESI_data(void);
int check_for_invalid_MESI_inst(void);
void LRU_instruction_update(unsigned int slot);
void LRU_data_update(unsigned int slot);

/* Trace file operations */
typedef enum ops {
    READ  = 0,			// L1 cache read
    WRITE = 1,			// L1 cache write
    FETCH = 2,			// L1 instruction fetch
    INVAL = 3,			// Invalidate command from L2 cache
    SNOOP = 4,			// Data request to L2 cache (response to snoop)
    RESET = 8,			// Reset cache and clear the statistics
    PRINT = 9,			// Print the contents of the cache
}OPS;

/* Cache line set data */
typedef struct my_cache {
    unsigned int tag;		// Tag bits
    unsigned int LRU;		// LRU bits
    char MESI;			// MESI bits
    unsigned char data[64];	// 64 bytes of data
	unsigned int address; 	//address
}CACHE;

/* Keep track of the Cache hits and misses */
typedef struct my_stat {
    unsigned int cache_hit;	// Cache hit count
    unsigned int cache_miss;	// Cache miss count
    unsigned int ratio;		// Hit and miss ratio
}STAT;

/* Instruction and Data Caches */    // NOTE: list assumption of testing a single set in final report, 16K sets would take unneeded space
CACHE data_cache[8];
CACHE instruction_cache[4];

//For keeping track of Stats
STAT stats;


/********************************************************************************
 *			  CACHE CONTROLLER
 * *****************************************************************************/

int main(int argc, char **argv) {
  
    // Check for command line argument
    if (argc != 2) {
      	printf("\n\tERROR: No input file provided.\n\t\tUsage: ./a.out filename.txt\n");
      	exit(1);
    }
  
    // Initialize the caches once at the beginning
    reset_cache_controller();
  
    // Read in file name from command line
    char *input_file = argv[1];
    
    // Test parsing function
    if (parser(input_file))
	printf("\n\tERROR: parsing file\n");
    

    print_cache(0);

    printf("\n\n\n\tClosing Program...\n\n\n\n");

    return 0;
}


/********************************************************************************
 * 			    FUNCTIONS
 * *****************************************************************************/

/* Text file parser
 * Parses ascii data in the format of <n FFFFFFFF> where
 * n is the operation number and FFFFFFFF is the address.
 * Calls appropiate operation based on parsing result.
 *
 * Input: string of .txt trace file to parse
 * Output: pass=0, fail=nonzero
 */
int parser(char *filename) {

    unsigned int op;		// Parsed operation from input
    unsigned int addr;		// Parsed address from input
    FILE *fp;			// .txt file pointer
  
    // Open the file for reading
    if (!(fp = fopen(filename, "r")))
	printf("\n\tERROR: opening file\n");

    // Read the file and decode the operation and address
    while (fscanf(fp, "%d %x", &op, &addr) != EOF) {
	switch(op) {
	    case READ:
		if (read(addr))
		    printf("\n\tERROR: L1 data cache read"); 
            break;

            case WRITE:
		if (write(addr))
		    printf("\n\tERROR: L1 data cache write");
            break;
          
            case FETCH: 
		if (fetch(addr))
		    printf("\n\tERROR: L1 instruction cache fetch");
            break;

            case INVAL: 
		if (invalidate(addr))
		    printf("\n\tERROR: L2 cache invalidate");
            break;
          
	    case SNOOP: 
		if (snooping(addr))
		    printf("\n\tERROR: L2 data request from snoop");
            break;

            case RESET:	reset_cache_controller();
            break;
          
            case PRINT: print_cache(addr);
            break;

	    default: printf("\n\tERROR: invalid trace number\n");
		return -1;
	}
    }
    
    // Close the file
    fclose(fp);

    return 0;
}
 
        
/* Reset the cache controller
 * 
 * Input: Void
 * Output: Void
 */
void reset_cache_controller(void)
{
    int i;
  
    printf("\n\t Resetting Cache Controller...\n\n");
    
    // Clear the instruction cache
    for (i = 0; i < 4; ++i) {
    	instruction_cache[i].tag = 0;
	instruction_cache[i].LRU = 0;
  	instruction_cache[i].MESI = 'I';
    }

    //Clear the data cache
    for (i = 0; i < 8; ++i) {
    	data_cache[i].tag = 0;
	data_cache[i].LRU = 0;
  	data_cache[i].MESI = 'I';
       data_cache[i].address = 0;
    }
      
    return;
}

/* This function will update the instruction cache LRU
 * Anything less than current LRU will increment
 * Anything that greater than current LRU doesn't change
 * MRU: 000   LRU: 111
 * 
 * Input: Line set number to start LRU comparison at
 * Output: Void
 */
void LRU_instruction_update(unsigned int slot)
{
    int current_LRU = instruction_cache[slot].LRU;
    
    for (int i = 0; i < 4; ++i) {
	if (instruction_cache[i].LRU <= current_LRU)
	    instruction_cache[i].LRU++;
    }
    instruction_cache[slot].LRU = 0;

    return;
}

        
/* This function will update the data cache LRU
 * Anything less than current LRU will increment
 * Anything that greater than current LRU doesn't change
 * MRU: 000   LRU: 111
 * 
 * Input: Line set number to start LRU comparison at
 * Output: Void
 */
void LRU_data_update(unsigned int slot)
{
    int current_LRU = data_cache[slot].LRU;
    
    for (int i = 0; i < 8; ++i) {
	if (data_cache[i].LRU <= current_LRU)
	    data_cache[i].LRU++;
    }
    data_cache[slot].LRU = 0;

    return;
}


/* This function will attempt to read a line from the cache
 * On a cache miss, the LRU member is evicted if the cache is full
 * 
 * Input: Address to read from cache
 * Output: Void
 */
int read(unsigned int addr)
{
    unsigned int tag;	// Cache tag decoded from incoming address
    int slot = -1;	// Set in the cache line
    int i = 0;

    // Cast the tag here so we can search the tag hit
    tag = addr >> (BYTE + SET);

    // Check for an empty set in the cache line
    for (i = 0; i < 8; ++i) {
	// Check for an empty set
	if (data_cache[i].tag == 0)
	    slot = i;
    }

    // Place the empty position 
    if (slot >= 0) {
	data_cache[slot].tag = tag;
	data_cache[slot].MESI = 'E';
	LRU_data_update(slot);
    data_cache[slot].address = addr;
    }
    else {		    // no gap then search for hit/miss
	slot = matching_tag_data(tag);	// Search for a matching tag first
	if (slot < 0) {  // Miss
	    // Check for a line with an invalid state to evict
	    slot = check_for_invalid_MESI_data();
	    if (slot < 0) { 	// If no invalid states, evict LRU
		slot = search_LRU_data();
        	if (slot>=0)
            {	data_cache[slot].tag = tag;
				data_cache[slot].MESI = 'E';
				LRU_data_update(slot);
             	data_cache[slot].address = addr;
            }
          	else
            {
              printf("LRU data is invalid");
            }
	    }
	    else {		// Else, evict the invalid member
		data_cache[slot].tag = tag;
		data_cache[slot].MESI = 'E';
		LRU_data_update(slot);  
         data_cache[slot].address = addr;
	    }
	}
	else {	// Hit
printf("\nHit");
	    switch (data_cache[slot].MESI) {
		case 'M':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'M';
		    LRU_data_update(slot);
            data_cache[slot].address = addr;


		break;
	    
		case 'E':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'S'; // According to TA
		    LRU_data_update(slot);
            data_cache[slot].address = addr;
		break;
	      
		case 'S':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'S';
		    LRU_data_update(slot);
            data_cache[slot].address = addr;
		break;
	    
		case 'I':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'S';
		    LRU_data_update(slot);
            data_cache[slot].address = addr;
		break;
            }
        }
    }

    return 0;
}

/* This function will attempt to write an address from the cache.
 * On a cache miss, the LRU member is evicted if the cache is miss.
 * 
 * Input: Address to read from cache
 * Output: Void
 */
int write(unsigned int addr)
{
    unsigned int tag;		// Cache tag from incoming address
    unsigned int slot;		// Set from the cache line
    int i = 0;

    // Cast the tag here so we can search the tag hit
    tag = addr >> (BYTE + SET);

    // Check for an empty set in the cache line
    for (i = 0; i < 8; ++i) {
	// Check for an empty set
	if (data_cache[i].tag == 0)
	    slot = i;
    }

    // Place the empty position 
    if (slot >= 0) {
	data_cache[slot].tag = tag;
	data_cache[slot].MESI = 'M';
	LRU_data_update(slot);
    }
     else {		    // no gap then search for hit/miss
	slot = matching_tag_data(tag);	// Search for a matching tag first
	if (slot < 0) {  // Miss
	    // Check for a line with an invalid state to evict
	    slot = check_for_invalid_MESI_data();
	    if (slot < 0) { 	// If no invalid states, evict LRU
		slot = search_LRU_data();
        	if (slot>=0)
            {	data_cache[slot].tag = tag;
				data_cache[slot].MESI = 'M';
				LRU_data_update(slot);
            }
          	else
            {
              printf("LRU data is invalid");
            }
	    }
	    else {		// Else, evict the invalid member
		data_cache[slot].tag = tag;
		data_cache[slot].MESI = 'M';
		LRU_data_update(slot);        	
	    }
	}
	else {	// Hit
printf("\nHit");
	    switch (data_cache[slot].MESI) {
		case 'M':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'M';
		    LRU_data_update(slot);
		break;
	    
		case 'E':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'M'; // According to TA
		    LRU_data_update(slot);
		break;
	      
		case 'S':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'E';
		    LRU_data_update(slot);
		break;
	    
		case 'I':data_cache[slot].tag = tag;
		    data_cache[slot].MESI = 'E';
		    LRU_data_update(slot);
		break;
            }
        }
    }

    return 0;
}

/* Evict the invalid member
 * 
 * Input: void
 * Output: error = -1 or set index of matching tag
 */
int search_LRU_data(void)
{
    for (int i = 0; i < 8; ++i) {
    	if (data_cache[i].LRU == 0x7)
        	return i;
    }
  
    return -1;
}
                   
/* Compare two cache tags for equality
 * One function is used for data cahches and the other is for instruction caches
 * Input: Decoded address tag
 * Output: error=-1 or set index of matching tag
 */
int matching_tag_data(unsigned int tag)
{	
    int i = 0;
  //  while ((i < 8) && (data_cache[i].tag != tag)) {
   // 	++i;
   // }
  
    while (data_cache[i].tag!=tag)
	{
		i++;
    	if (i >7)
		{
			return -1;
		} 
	}
  	return i;
	
}                   
  
  
  
  
  /*	for (i = 0; i < 8; ++i) {
     	if (data_cache[i].tag == tag)
    		return i;
    }*/

   // return -1;
                     
int matching_tag_inst(unsigned int tag)  
{
    int i = 0;
  //  while ((i < 8) && (data_cache[i].tag != tag)) {
   // 	++i;
   // }
  
    while (instruction_cache[i].tag!=tag)
	{
		i++;
    	if (i >3)
		{
			return -1;
		} 
	}
  	return i;
}
                        
/* Search for Invalid MESI states within a set
 * 
 * Input: void
 * Output: Index of Invalid state, negative if no invalid states
 */                        
int check_for_invalid_MESI_data(void)
{
    for (int i = 0; i < 8; ++i){
	if (data_cache[i].MESI == 'I')
	    return i;
    }
    return -1;
}

/* Search for Invalid MESI states within a set
 * 
 * Input: void
 * Output: Index of Invalid state, negative if no invalid states
 */                        
int check_for_invalid_MESI_inst(void)
{
    for (int i = 0; i < 4; ++i) {
	if (instruction_cache[i].MESI == 'I')
	    return i;
    }

    return -1;
}
                        
/* Check the bus for other processors reading/writing addresses of interest
 * 
 * Input: Address of interest
 * Output: pass=0, fail=nonzero
 */     
int snooping(unsigned int addr)
{
    //Total of 4 states, Modified, Exclusive, Shared, Invalid
    //Assumption that L2 is telling L1 that this address needs to be invalidated. 
    //Therefore we just have to set MESI bit to 'I'
    unsigned int tag = addr >> (BYTE + SET);
    int i = 0;
    
    // Check the data cache
    for (i = 0; i < 8; ++i) {
	if(data_cache[i].tag == tag) { //Compares to find tag of address to invalidate
	    switch (data_cache[i].MESI) {
      		case 'M': data_cache[i].MESI = 'I'; //changes MESI bit set to invalidate
      		case 'E': data_cache[i].MESI = 'I'; //changes MESI bit set to invalidate
      		case 'S': data_cache[i].MESI = 'I'; //changes MESI bit set to invalidate
          	case 'I': return 0; 		    //do nothing, already invalid
            } 
	}
    }
    return 0;
}

        
/* Print the contents of the current cache lines
 * 
 * Input: cache address to print cache line metadata (Tag, LRU, MESI) for
 * Output: pass=0, fail=nonzero
 */ 
void print_cache(unsigned int addr)
{
    //Input argument later is a mode for what to print. For right now mode = 0
    int i = 0; //To go through cache
    
    //Mode 0 Displays: statistics and print contents and state of the cache
    //Mode 1 Displays: Everything from mode 0. But also display the communication messages to the L2 cache
    int mode = 0;
	printf("\nInput the Mode \n");
  	printf("Mode 0: Summary of usage statistics and print contents and state of cache\n");
    printf("Mode 1: Information from Mode 0 and messages to L2. Only input a number. \n");
    scanf("Mode: %d", &mode);
    for (i = 0; i < 8; ++i) {
	printf("\n------------------------------------------\n");
	printf("-------------Cache Information--------------\n");
	printf("Address: %x Tag : %u LRU: %u MESI State: %c \n", data_cache[i].address, data_cache[i].tag, data_cache[i].LRU, data_cache[i].MESI);	//Tag,LRU,MESI bits
    }


    printf("\n---------------------------------------\n");
    printf("--------Statistics Information---------\n");
    printf("Cache Hits: %u Cache Miss: %u Ratio: %u \n", stats.cache_hit, stats.cache_miss, stats.ratio );	// Cache hit count, Cache miss count	
    
    if(mode == 1) {
	    //Print L2 cache messsages
    }
    
    return;
}
  
        
/* Read an instruction in from the instruction cache
 * Note: Since the instruction cache only reads, MESI state M is not possible
 * 
 * Input: address to read
 * Output: pass=0, fail=nonzero
 */
int fetch(unsigned int addr)
{
    unsigned int tag;		// Cache tag decoded from incoming address
    int slot = -1;	// Set in the cache line
    int i = 0;

    // Cast the tag here so we can search the tag hit
    tag = addr >> (BYTE + SET);

    // Check for an empty set in the cache line
    for (i = 0; i < 4; ++i) {
	// Check for an empty set
	if (instruction_cache[i].tag == 0)
	    slot = i;
    }

    // Place the empty position 
    if (slot >= 0) {
	instruction_cache[slot].tag = tag;
	instruction_cache[slot].MESI = 'E';
	LRU_data_update(slot);
    }
    else { // no gap then search for hit/miss
	slot = matching_tag_inst(tag);
	if ((slot) < 0) {  	// Miss
	    // Check for a line with an invalid state to evict
	    slot = check_for_invalid_MESI_inst();
	    if (slot < 0) { 	// No invalid states, evict LRU
		slot = search_LRU_data();
		instruction_cache[slot].tag = tag;
		instruction_cache[slot].MESI = 'E';
		LRU_instruction_update(slot);
	    }
	    else {			// Evict the invalid member
		instruction_cache[slot].tag = tag;
		instruction_cache[slot].MESI = 'E';
		LRU_instruction_update(slot);        	
	    }
	}
	else {				// Else, there was a hit
	    switch (instruction_cache[slot].MESI) {
          case 'M':instruction_cache[slot].tag = tag;
              instruction_cache[slot].MESI = 'M';
              LRU_instruction_update(slot);
          break;

          case 'E':instruction_cache[slot].tag = tag;
              instruction_cache[slot].MESI = 'S';		// According to TA
              LRU_instruction_update(slot);
          break;

          case 'S':instruction_cache[slot].tag = tag;
              instruction_cache[slot].MESI = 'S';
              LRU_instruction_update(slot);
          break;

          case 'I':instruction_cache[slot].tag = tag;
              instruction_cache[slot].MESI = 'S';
              LRU_instruction_update(slot);
          break;
            }
        }
    }
    return 0;
}
       
/* Invalidate an L2 command
 * 
 * Input: address to invalidate
 * Output: pass=0, fail=nonzero
 *
 */
int invalidate(unsigned int addr)
{
    //Assumption that L2 is telling L1 that this address needs to be invalidated. 
    //Therefore we just have to set MESI bit to 'I'
    unsigned int tag = addr >> (BYTE + SET);
    int i = 0;
    for (i = 0; i < 8; ++i)  //goes through data cache 
    {
	if(data_cache[i].tag == tag) //Compares to find tag of address to invalidate
        {
	    switch (data_cache[i].MESI) 
	    {
      		case 'M': data_cache[i].MESI = 'I';	//changes MESI bit set to invalidate
      		break;
		
		case 'E': data_cache[i].MESI = 'I';	//changes MESI bit set to invalidate
      		break;

		case 'S': data_cache[i].MESI = 'I';	//changes MESI bit set to invalidate
		break;

		case 'I': return 0; 			//do nothing, already invalid
            } 
	}
    }
    return 0;
}                      
                        
