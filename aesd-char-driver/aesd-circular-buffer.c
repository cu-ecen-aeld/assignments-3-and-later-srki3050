/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes (Edits made by Sricharan Kidambi
 * @date 2020-03-01   (Edits made by Sricharan on 2022-10-24)
 * @copyright Copyright (c) 2020
 * @references:	Revisited the circular buffer program done in PES 				course ECEN 5813 - Principles of Embedded Software.
 			Obtained Assistance from Swapnil Ghonge on how to write the same program with void return type
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"
// Writing this for coding convenience
#define MAX_WRITE AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
/*
* Function	: get_populated_nodes()
* Purpose	: obtain how much locations has data written there.
* @param	: an instance to the circular buffer
* returns	: The current location based on the buffer data	
*/
int get_populated_nodes(struct aesd_circular_buffer *buffer){
int remaining = buffer->out_offs - buffer->in_offs;
if(buffer->full)
	return MAX_WRITE;
else if(buffer->in_offs < buffer->out_offs)
	return remaining;
else if(buffer->out_offs < buffer->in_offs)
	return MAX_WRITE - (remaining) + 1;
else
	return 0;
}
/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
// Check if your list actually exists
if(!buffer)
	return NULL;
// check if you are trying to encode a value in an unexisting location
if(!entry_offset_byte_rtn)
	return NULL;
// The function has to return the position described by char_offset
struct aesd_buffer_entry *position = NULL;
// obtain the populated nodes and current pointing index
int current_location = get_populated_nodes(buffer);
uint8_t index = buffer->out_offs;
// start looping through those nodes to see if offset matches
for(int i = current_location;i > 0;i--){
	if(buffer->entry[index].size >= char_offset + 1){
		position = &buffer->entry[index];
		*entry_offset_byte_rtn = char_offset;
		break;
	}
	else{
		char_offset -= buffer->entry[index].size;
	}
	index++;
	index %= MAX_WRITE;
}
// if data is not written or position is not available this will return NULL
return position;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
// Check if your list actually exists
if(!buffer)
	return;
// check if you are trying to encode an unexisting value
if(!add_entry)
	return;
// Perform circular buffer write operation
buffer->entry[buffer->in_offs].size = add_entry->size;
buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
// Increment the writing pointer and wrap around the circular buffer
buffer->in_offs++;
buffer->in_offs %= MAX_WRITE;
//Check full case conditions
if(buffer->in_offs == buffer->out_offs)
	buffer->full = true;
//if buffer is full - expectation is to overwrite the most oldest elements, meaning, where the buffer->out_offs pointer is currently residing.
//Increment the read pointer and wrap around the circular buffer.
else if(buffer->full)
{
	buffer->out_offs++;
	buffer->out_offs %= MAX_WRITE;
}
return;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}