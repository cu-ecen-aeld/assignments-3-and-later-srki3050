/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

size_t calculate_total_characters(struct aesd_circular_buffer *buffer){
	size_t characters = 0;
	int loc = 0;
	while(loc < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
		characters += buffer->entry[loc++].size;
	}
	return characters;
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
 //Considering a string example
 //"hello","this","is","AESD","embedded","circular","buffer","for","Assignment7","implementation"
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    //locate an entry correspoding to a position
    //This is just going to map an overall offset of the total number of bytes that are received across all entries to a specific pointer where that string is located and then the offset within that string that's being referenced by char offset.
        size_t current_string_length = buffer->entry[buffer->in_offs].size;
        size_t total_no_of_characters;
    	// Check if your list actually exists
	if(!buffer)
		return NULL;
	// check if you are trying to encode a value in an unexisting location
	if(!entry_offset_byte_rtn)
		return NULL;
	
	//the above example will give a total no of characters = 5+4+2+3+8+8+6+3+11+14 = 64
	total_no_of_characters = calculate_total_characters(buffer);
	if(char_offset >= total_no_of_characters){
		return NULL;
	}
	//consider in_offs entry is embedded, current string length = 8, then you can return that entry back as offset would point to 'e' in "embedded"
	if(char_offset < current_string_length){
		*entry_offset_byte_rtn = char_offset;
		return &(buffer->entry[buffer->in_offs]);
	}
	//if offset > 8, then you will be taking the next string to account
	else{
		int position = buffer->in_offs + 1;
		while((current_string_length + buffer->entry[position].size) <= char_offset){
			current_string_length += buffer->entry[position].size;
			position = (position + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		}
		*entry_offset_byte_rtn = char_offset - current_string_length;
		return &(buffer->entry[position]);
	}
	//if given offset is greater than total size, then corresponding offset cannot be calculated NULL is returned in that case as well.
	
    return NULL;
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
    /**
    * TODO: implement per description
    */
    if(!buffer)
	return;
    // check if you are trying to non-existent value in memory
    if(!add_entry)
	return;
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if(buffer->string_count == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
    	buffer->full = true;
    }
    if(buffer->full){
    	buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else{
    	buffer->string_count++;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
