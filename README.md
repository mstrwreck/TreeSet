TreeSet .h and .c implement a RedBlack Tree to store a wide and potentially sparse bitmap, limited only by range of an unsigned int (assumed to be 32 bits at least) when
 using just the CheckBit and SetBit interfaces (so a bitmap with a virtual size of  2^32 bits).  Interfaces are also offered to allow the upper and lower
  part of the key index to be used independently, allowing for potentially 32 bits times the maximum bitmap size per node. (ie: the key is the 'upper' 32 bits of
  the overall key and the sub_bit_offset is up to the max bitmap size per node). Given the default MAX_BITMAP_PER_NODE of 64, this could be up to 2^(32+64) or 2^96 bits (~79 octillion).

 DataFilter.c is an application using the TreeSet to find and filter out timestamp collisions per a subset of ISO 8601 timestamp format (allowing for UTC or time offset formatting) over a 10,000 year range from an input file and write unique timestamps only
 to an output file.
 
 Note that two timestamps that refer to the same moment in time after time offset is applied will be counted as duplicates and only the first ocurrance (expressed however it was given in the input file) will be output to the output file.
 
 test.txt is a sample input file.
