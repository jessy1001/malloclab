#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include "mm.h"
#include "memlib.h"

/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "teama",
    /* First member's full name */
    "Hyunjoo",
    /* First member's email address */
    "hyunjoo",
    /* Second member's full name (leave blank if none) */
    "",
    /* Third member's full name (leave blank if none) */
    ""};

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(HDRP(bp) - WSIZE))
#define MIN_BLK_SIZE (2*DSIZE)
static char *heap_listp;
static char* seg_listp;

/* explicit 추가 매크로*/
#define PRED_FREEP(bp) (*(void **)(bp))         // *(GET(PRED_FREEP(bp))) == 다음 가용리스트의 bp // Predecessor
#define SUCC_FREEP(bp) (*(void **)(bp + WSIZE)) // *(GET(SUCC_FREEP(bp))) == 다음 가용리스트의 bp // successor


/* Forward Declaration */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void removeBlock(void *bp);  //
static void putFreeBlock(void *bp); //
static void seg_init(void);
static void* seg_find(int size);


void seg_init(void){
	if ((seg_listp = mem_sbrk(32*WSIZE)) == (void*)-1) return -1; //seg_list는 뒤로 늘릴 일이 없다는 전제 하에 만들기 때문에 heap_list보다 먼저 initiate.
                                                                  //maximum free block size = 2^32일 거라는 가정 하에 그런 건데, ec2의 storage volume이 8GB인 데서 내린 결론.
	for (int i = 0; i < 32; i++){
		PUT(seg_listp + (i * WSIZE), NULL);
	}
}

static void* seg_find(int size){
	static char* seg;

	int i = 0;
	for (i = 32; i > 0; i--){
		if((size & (1<<i)) > 0){                //if size == 2^20 + blahblah, seg = 20
			break;
		}
	}
	seg = seg_listp + (i * WSIZE);
	return seg;
}


int mm_init(void)
{
	seg_init();

    if ((heap_listp = mem_sbrk(4 * WSIZE))== (void *)-1) return -1;
    PUT(heap_listp, 0);                              //padding
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));     // header of prologue
    //PUT(heap_listp + 2 * WSIZE, NULL);               // point at prev free block
    //PUT(heap_listp + 3 * WSIZE, NULL);               // point at next free block
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1)); // footer of prologue
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));         // header of epilogue

    heap_listp += DSIZE;
    //free_listp = heap_listp; // 지울 것

    if (extend_heap(CHUNKSIZE / DSIZE) == NULL)             //extending the heap by CHUNKSIZE is a convention derived from numerous practices...
    {
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words)
{
    size_t size;
    char *bp;

    size = words * DSIZE; 
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extend_size;
    void *bp;

    if (size <= 0)
        return NULL;

    if (size <= DSIZE){
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    //search the free list for a fit
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    //no fit found - get more memory and place the block there
    extend_size = MAX(asize, CHUNKSIZE);
    bp = extend_heap(extend_size / DSIZE);
    if (bp == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void* find_fit(size_t asize)
{
	void* bp;
	void* best_bp = (char*)NULL;
	size_t best;
	static char* seg;

	int i = 0;
	for (i = 32; i > 0; i--){
		if ((asize & (1<<i)) > 0){			//seg_find를 써도 될 것처럼 생겼으나
			break;							//seg_find returns seg_listp + (i * WSIZE) and we need i itself.
		}
	}

	int j = i;
	for(j = i; j <= 32; j++){               //j=i 부터 시작하는 이유는 size = 2^20 이렇게 딱 떨어지는 수일 수도 있으니까.(although 2^20 + blahblah is more likely)
		seg = seg_listp + (j*WSIZE);
		if(GET(seg) != NULL){               //처음엔 extend_heap(CHUNKSIZE)이 배정된 seg=12까지 찾아감
	
			for(bp = PRED_FREEP(seg); bp != NULL; bp = PRED_FREEP(bp)){     //해당 seg 안에서도 바로 asize<=(size of block)인 블록이 안 나올 수도 있음
				if (asize <= GET_SIZE(HDRP(bp))){   
					return bp;             
				}	                               
                    
			}
        }
    }
	return NULL;	                        //find_fit 못 하면 extend_heap 할 거니까 return NULL 꼭 넣어줘야 됨, 이거 없으면 core gets dumped
			
}	


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));                     //current size
    removeBlock(bp);                                       //remove block from free list
    if (csize - asize >= MIN_BLK_SIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        //pass leftover block into free list
        putFreeBlock(bp);
    }
    else //if leftover block does not meet MIN_BLK_SIZE
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void removeBlock(void *bp)
{
	if(PRED_FREEP(bp) != NULL){
		PUT(PRED_FREEP(bp)+WSIZE, SUCC_FREEP(bp));          //free list에서 내 pred의 succ을 내 succ로.
	}
	if(SUCC_FREEP(bp) != NULL){
		PUT(SUCC_FREEP(bp), PRED_FREEP(bp));                //free list에서 내 succ의 pred를 내 pred로.
	}
}

/* segregated fit 시 주목할 함수*/
void putFreeBlock(void* bp)
{
	char* seg;
	seg = seg_find(GET_SIZE(HDRP(bp)));

	if(PRED_FREEP(seg) != NULL){
		PUT(PRED_FREEP(seg)+WSIZE, bp);                     //seg의 PRED 바로 뒤에 bp 넣기
	}
	PUT(bp, PRED_FREEP(seg));                               //내 PRED는 seg의 PRED (그게 null이라면 null)
	PUT(bp + WSIZE, seg);                                   //내 SUCC 는 seg
	PUT(seg, bp);                                           //seg의 PRED는 나(bp)
}                                                           

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // CASE 1 : neither are available
    if (prev_alloc && next_alloc) {
        putFreeBlock(bp);
        return bp;
    }
    // CASE 2 : next block is available
    if (prev_alloc && !next_alloc)
    {
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // CASE 3 : previous block is available
    else if (!prev_alloc && next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // CASE 4 : both are available(unallocated)
    else //if (!prev_alloc && !next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    }
    //add newly coalesced block to free list
    putFreeBlock(bp);
    return bp;
}

void *mm_realloc(void *bp, size_t size)
{
    if (size <= 0){                         //reallocate to size of zero = epilogue 아니니까 그냥 해제해주기
        mm_free(bp);
        return 0;
    }
    if (bp == NULL){                        //unallocated address in memory
        return mm_malloc(size);
    }

    void *newp = mm_malloc(size);           //*newp = malloc(size), copy data from old bp and free that bp, return newp
    if (newp == NULL){
        return 0;
    }

    size_t oldsize = GET_SIZE(HDRP(bp));
    if (size < oldsize){
        oldsize = size;
    }
    memcpy(newp, bp, oldsize);
    mm_free(bp);
    return newp;
}