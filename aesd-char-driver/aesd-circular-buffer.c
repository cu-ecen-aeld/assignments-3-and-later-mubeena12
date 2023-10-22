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
#include <stdio.h>

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
    /**
    * TODO: implement per description
    */
    size_t cumulative_length = 0;
    size_t entry_index = buffer->out_offs;
    size_t entries_checked = 0; // Track the number of entries checked

    while (entries_checked < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        if (buffer->entry[entry_index].buffptr) {
            cumulative_length += buffer->entry[entry_index].size;

            if (cumulative_length > char_offset) {
                // Calculate the byte offset within the entry
                size_t byte_offset = char_offset - (cumulative_length - buffer->entry[entry_index].size);

                // Store the byte offset if requested
                if (entry_offset_byte_rtn) {
                    *entry_offset_byte_rtn = byte_offset;
                }

                return &(buffer->entry[entry_index]);
            }
        }

        entry_index = (entry_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        entries_checked++;
    }

    return NULL;  // char_offset not found in the buffer
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

   // Check if the buffer is already full
    if (buffer->full) {
        // If so, overwrite the oldest entry and advance out_offs
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Copy the content of add_entry into the buffer at in_offs
    buffer->entry[buffer->in_offs] = *add_entry;

    // Update in_offs for the next write operation
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // Check if the buffer is now full
    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }
//    if (buffer->full) {
//         // If buffer is full, overwrite the oldest entry
//         buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
//     } else if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1) {
//         buffer->full = true;  // Buffer is full after this write
//     }

//     buffer->entry[buffer->in_offs] = *add_entry;
//     buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

/*
    // Print all entries in the buffer
    int idx;
    struct aesd_buffer_entry *tmp_entry;
    
    AESD_CIRCULAR_BUFFER_FOREACH(tmp_entry, buffer, idx)
    {
        printf("index = %d: in_offs = %d, out_offs = %d, full = %d, entry.size = %ld, entry.buff = %s\n", idx, buffer->in_offs, buffer->out_offs, buffer->full, tmp_entry->size, tmp_entry->buffptr);
    }
    printf("----------------\n");
*/
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
