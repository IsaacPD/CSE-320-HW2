#include <limits.h>
#include <unistd.h>
#include "helper.h"
#define WSIZE 8
#define DSIZE 16

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc, id) ((size) | (alloc) | (id << 1))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & -0x8)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_ID(p) ((GET(p) & 0x6) >> 1)

void coalesce(void* buffer){
	int id, first = 1, size;
	do{
		id = GET_ID(buffer);
		size = GET_SIZE(buffer);
		int alloc = GET_ALLOC(buffer);

		if (alloc == 0 &&  size != 16){
			int prevA = first ? first : GET_ALLOC(buffer - WSIZE);
			int nextA = GET_ALLOC(buffer + size);

			prevA = (prevA || id != GET_ID(buffer - WSIZE)) ? 1 : 0;
			nextA = (nextA || id != GET_ID(buffer + size) || GET_SIZE(buffer + size) == 16) ? 1 : 0;

			if(!prevA && nextA){
				size += GET_SIZE(buffer - WSIZE);
				buffer = buffer - GET_SIZE(buffer - WSIZE);
				PUT(buffer, PACK(size, 0, id));
				PUT(buffer + size - WSIZE, PACK(size, 0, id));
			} 
			else if (prevA && !nextA){
				size += GET_SIZE(buffer + size);
				PUT(buffer, PACK(size, 0, id));
				PUT(buffer + size - WSIZE, PACK(size, 0, id));
			}
			else if (!prevA && !nextA){
				size += GET_SIZE(buffer + size);
				size += GET_SIZE(buffer - WSIZE);
				buffer = buffer - GET_SIZE(buffer - WSIZE);
				PUT(buffer, PACK(size, 0, id));
				PUT(buffer + size - WSIZE, PACK(size, 0, id));
			}
		}
		buffer += size;
		first = 0;
	} while (id != 0 || size != 16);
}

void copyPackets(void* source, void* dest){
	int id;
	do{
		id = GET_ID(source);
		int size = GET_SIZE(source);

		memcpy(dest, source, size);

		dest += size;
		source += size;
	} while (id != 0);
}

void printPackets(void* buffer){
	int i, id, size;
	do{
		size = GET_SIZE(buffer);
		id = GET_ID(buffer);
		int alloc = GET_ALLOC(buffer);
		printf("Memory Location: %p\nSize: %d\nID: %d\nAllocation Flag: %d\nHeader: %d\n",
			buffer, size, id, alloc, GET(buffer));
		buffer += WSIZE;
		printf("Payload: %s\n", ((char*)buffer));	
		buffer += size - DSIZE;
		printf("Tail: %d\n\n", GET(buffer));
		buffer += WSIZE;	//Next Packet
	} while (id != 0 || size != 16);
}

void superfree(void* buffer){
	int id, totalSize = 0;
	do {
		id = GET_ID(buffer);
		int size = GET_SIZE(buffer);
		int flag = GET_ALLOC(buffer);
		totalSize += size;

		if (flag == 0) {
			PUT(buffer, PACK(size, 0, 0));
			PUT(buffer + size - WSIZE, PACK(size, 0, 0));
		}
		buffer += size;
	} while (id != 0);
	
	buffer -= totalSize;
	coalesce(buffer);
}

int main(int argc, char** argv) {
    if (*(argv + 1) == NULL) {
        printf("You should provide name of the test file.\n");
        return 1;
    }
	
	if (access(*(argv + 1), F_OK) != 0) return 0;

    void* ram = cse320_init(*(argv + 1));
    void* tmp_buf = cse320_tmp_buffer_init();
    int ret = 0;
    /*
     * You code goes below. Do not modify provided parts
     * of the code.
     */
	void* cursor = ram;
	void* bCursor = cse320_sbrk(0);
	int i = 0, id;

	while(i < 1024){
		if (GET_SIZE(cursor) != 0){
			int size = GET_SIZE(cursor);
			int alloc = GET_ALLOC(cursor);
			int id = GET_ID(cursor);

			if(cse320_sbrk(size))
				memcpy(bCursor, cursor, size);
			else {
				printf("SBRK_ERROR");
				exit(errno);
			}
			cursor += size;
			bCursor += size;
			i += size;
		} else {
			i+=WSIZE;
			cursor+=WSIZE;
		}
	}

	if (cse320_sbrk(DSIZE)){
		PUT(bCursor, PACK(16, 0, 0));
		PUT(bCursor + WSIZE, PACK(16, 0, 0));
	}
	
	bCursor = ram;
	cursor = tmp_buf + 128;
	
	int f, prevMin = 0, min = INT_MAX;
	void* currentMin = NULL;
	for (i = 1; i < 4; i++){
		for (f = 1; f >= 0; f--){
			while (prevMin != -1){	
				do{
					int size = GET_SIZE(cursor);
					id = GET_ID(cursor);
					int alloc = GET_ALLOC(cursor);
					if (id == i && alloc == f && size < min && size > prevMin){
						min = size;
						currentMin = cursor;
					}
					cursor += size;	//Next Packet
				} while (id != 0);
				
				if (prevMin != min && currentMin != NULL){
					if (cse320_sbrk(min)){
						memcpy(bCursor, currentMin, min);
						bCursor += min;
						prevMin = min;
					} else {
						printf("SBRK_ERROR");
						exit(errno);
					}
				}
				else prevMin = -1;
				cursor = tmp_buf + 128;
				min = INT_MAX;
				currentMin = NULL;
			}
			prevMin = 0;
		}
	}
	
	PUT(bCursor, PACK(16, 0, 0));
	PUT(bCursor + WSIZE, PACK(16, 0, 0));
	coalesce(ram);

	//superfree(ram);
	//printf("\nAfter Coalesce\n----------\n");
	//printPackets(ram);
	
    /*
     * Do not modify code below.
     */
    cse320_check();
    cse320_free();
    return ret;
}
