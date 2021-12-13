#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <assert.h>



/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)

#define MAX_Alloc_size 4088
#define MAX_Block_size 4096
#define MIN_Block_size 32
#define HEADER(addr) ((list_node*)((void*)addr))->header
#define NEXT(addr) ((list_node*)((void*)addr))->next
#define DATA(addr) (void*)NEXT(addr)
#define FLIST(index) ((list_node**)start)[index]

typedef struct list_node list_node2;

struct list_node {
    uint64_t head;
    list_node2* next;
};

/*
static void* start = NULL;*/
/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}
static void* heap = NULL;

static void init(){
    heap = sbrk(13 * 8);
    memset(heap, 0, 13 * 8);
}

static void* new_chunk(size_t bsize){
    void* chunk = sbrk(CHUNK_SIZE);
    void* point;
    void* epoint;
    point = chunk;
    epoint = chunk+CHUNK_SIZE-bsize;
    for(;point<epoint;point += bsize){
        ((list_node2*)((void*)point))->head = bsize << 1;
        ((list_node2*)((void*)point))->next = point + bsize;
    }
    ((list_node2*)((void*)point))->next = NULL;
    return chunk;
}

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
    size_t index;
    size_t the_size;
    void* memory;
    if(size == 0){
        return NULL;
    }

    size_t num = size + 8;
    if(size > MAX_Alloc_size){
        memory = bulk_alloc(num);
        if(memory){
            ((list_node2*)((void*)memory))->head = (size <<1)|1;
            memory = memory + 8;
        }
        return memory;
    }
    if(heap == NULL){
        init();
    }
    index = block_index(size);
    the_size = 1<<index;
    if(((list_node2**)heap)[index] == NULL){
        ((list_node2**)heap)[index] = new_chunk(the_size);
    }
    ((list_node2**)heap)[index]->head |= 1;
    memory = ((void*)&(((list_node2**)heap)[index]->head))+8;
    ((list_node2**)heap)[index] = ((list_node2**)heap)[index]->next;
    return memory;
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
    void* memory;
    if(nmemb>4088){
        void *ptr = bulk_alloc(nmemb * size);
        memset(ptr, 0, nmemb * size);
        return ptr;
    }
    else{
        memory = malloc(nmemb * size);
        if(memory != NULL){
            memset(memory,0,nmemb * size);
        }
        return memory;
    }
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
    size_t bsize;
    void* memory;
    if(ptr == NULL){
        return malloc(size);
    }
    else if(size == 0){
        free(ptr);
        return NULL;
    }
    bsize = ((list_node2*)((void*)ptr-8))->head >>1;

    if(bsize - 8 >= size){
        return ptr;
    }

    memory = malloc(size);
    if(memory != NULL){
        memcpy(memory, ptr, bsize-8);
        free(ptr);
    }
    return memory;
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
    size_t bsize;
    if(ptr == NULL){
        return;
    }
    ptr = ptr - 8;
    bsize = ((list_node2*)((void*)ptr))->head >> 1;
    if(bsize>MAX_Alloc_size){
        bulk_free(ptr,bsize);
    }
}
