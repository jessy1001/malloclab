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
static char *free_listp; // 가용리스트의 첫번째 블록을 가리키는 포인터 //근데 heap_listp랑 똑같아서 필요없음
static char *heap_listp;
static char *rover; //for next_fit()

/* explicit 추가 매크로*/
#define PRED_FREEP(bp) (*(void **)(bp))         // *(GET(PRED_FREEP(bp))) == 다음 가용리스트의 bp // Predecessor
#define SUCC_FREEP(bp) (*(void **)(bp + WSIZE)) // *(GET(SUCC_FREEP(bp))) == 다음 가용리스트의 bp // successor

/* Forward Declaration */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void removeBlock(void *bp);  //
void putFreeBlock(void *bp); //

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6 * WSIZE))== (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);                              
    PUT(heap_listp + WSIZE, PACK(2 * DSIZE, 1));     // header of prologue
    PUT(heap_listp + 2 * WSIZE, NULL);               // point at prev free block
    PUT(heap_listp + 3 * WSIZE, NULL);               // point at next free block
    PUT(heap_listp + 4 * WSIZE, PACK(2 * DSIZE, 1)); // footer of prologue
    PUT(heap_listp + 5 * WSIZE, PACK(0, 1));         // header of epilogue

    heap_listp += DSIZE; //****내가 추가. heap_listp as the first ever bp
    free_listp = heap_listp; // free_listp 초기화, 원래는 heap_listp 변경 안하고 
    //가용리스트에 블록이 추가될 때 마다 항상 리스트의 제일 앞에 추가될 것이므로 지금 생성한 프롤로그 블록은 항상 가용리스트의 끝에 위치하게 된다.

    if (extend_heap(CHUNKSIZE / DSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words)
{
    size_t size;
    char *bp;

    size = words * DSIZE; //처음부터 이 size로는 받아올 수가 없음, 우리가 받아와서 double-word align할거니까
    // size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

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

static void *find_fit(size_t asize)         //LIFO라서 next fit이랑은 원리가 안 맞음. thus, LIFO is designed for first-fit.
{    // first fit
    void *bp;
    bp = free_listp;
    //가용리스트 내부의 유일한 할당 블록은 맨 뒤의 프롤로그 블록이므로 할당 블록을 만나면 for문을 종료한다.
    for (bp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCC_FREEP(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize;
    csize = GET_SIZE(HDRP(bp));                     //current size
    // 할당될 블록이니 가용리스트 내부에서 제거해준다.
    removeBlock(bp);                                //remove block from free list
    if (csize - asize >= MIN_BLK_SIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        // 가용리스트 첫번째에 분할된 블럭을 넣는다.
        putFreeBlock(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// 새로 반환되거나 생성된 가용 블록을 가용리스트 맨 앞에 추가한다.
void putFreeBlock(void *bp)
{
    SUCC_FREEP(bp) = free_listp;                    //블럭의 그 자리에 이 pointer를 넣어줘
    PRED_FREEP(bp) = NULL;
    PRED_FREEP(free_listp) = bp;
    free_listp = bp;
}

// 항상 가용리스트 맨 뒤에 프롤로그 블록이 존재하고 있기 때문에 조건을 간소화할 수 있다. !!!!!중요!!!!!
void removeBlock(void *bp)
{
    // 첫번째 블럭을 없앨 때
    if (bp == free_listp) //bp is the first block in free list
    {
        PRED_FREEP(SUCC_FREEP(bp)) = NULL; //next free block의 pred_info 자리에 null(니 앞의 애는 이제 없어진다)
        free_listp = SUCC_FREEP(bp); //bp 다음 애로 free list 시작점 update
        
    }
    else //앞에 다른 블럭이 있을 때
    {
        SUCC_FREEP(PRED_FREEP(bp)) = SUCC_FREEP(bp); //내 pred free block의 succ_info를 내 succ_info로 update
        PRED_FREEP(SUCC_FREEP(bp)) = PRED_FREEP(bp); //내 succ free block의 pred_info에 내 pred_info update
        //만약 맨 뒤 free block이었다면 이 뒤에 prologue block이 있음 -> putFreeBlock하는 과정에서 PRED_FREEP(prologue) = bp로 되어있음
    }
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // CASE 1
    if (prev_alloc && next_alloc) {
        putFreeBlock(bp);
        return bp;
    }
    // CASE 2 :
    if (prev_alloc && !next_alloc)
    {
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // CASE 3 :
    else if (!prev_alloc && next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // CASE 4 :
    else //if (!prev_alloc && !next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        // PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // bp = PREV_BLKP(bp); //그냥 case 3랑 똑같이 해주면 안되나???
    }
    //연결된 블록을 가용리스트에 추가
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

    void *newp = mm_malloc(size);
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