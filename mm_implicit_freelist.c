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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Hyunjoo Lee",
    /* First member's email address */
    "jessy@jessy.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds 'up' to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) //~0x7 = 11111000 이니까 얘랑 비트연산하면 당연히 뒤에 세 자리를 버림.
#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) //size_t는 unsigned int(4byte)형. 근데 딱히 쓸일x...

// 여기부터 내가 define하는 것
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int*)(p))                            //(unsigned int*)p의 내용을 가져와라
#define PUT(p, val) (*(unsigned int*)(p) = val)
#define GET_SIZE(p) (GET(p) & ~0x7) //probably from header - 왜 hdrp 안함...?
#define GET_ALLOC(p) (GET(p) & 0x1) //probably from header
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static char *rover; //for next_fit()

// static variables and functions declared by ME
static void *heap_listp = NULL;

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1){
        // printf("failed to set heap_listp");
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));                            //header of prologue block
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));                            //footer of prologue block
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));                                //header of epilogue block
    heap_listp += (2*WSIZE);                                                //첫 block ptr(end of header)로 heap_listp 업데이트

    rover = heap_listp;                                                     //for next_fit()

    // Extend the empty heap with a free block of CHUNKSIZE bytes -> 근데 왜 CHUNKSIZE/WSIZE? bc we need the number of words
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        // printf("failed to extend heap");
        return -1;
    }
    return 0;
}


static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    size = words * DSIZE;                                                            //size가 2의 배수이도록 == double-word alignment 지키기 위함
    // size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;   //words는 word의 개수
    if ((long)(bp = mem_sbrk(size)) == -1){                     //bp에 새로 확보한 메모리의 start 주소
        return NULL;
    }
    //else: mem_sbrk(size)에서는 mem_brk를 size만큼 올리고 old_brk를 return. old_brk(original mem_brk) - word = epilogue header
    PUT(HDRP(bp), PACK(size, 0));                               //원래 epilogue block의 hdr -> hdr of new block
    PUT(FTRP(bp), PACK(size, 0));                               //ftr of new block
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));                       //new epilogue block header

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    //case 1: if both prev and next are allocated
    if (prev_alloc && next_alloc) {
        return bp;
    }
    //case 2: if only next block is free
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));                           //현재 block의 hdr 정보 update
        PUT(FTRP(bp), PACK(size, 0));                           // PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0))해야 되는 거 아닌가?????아님.애초에 ftr는 getsize(hdr)보고 찾아가기 때문에.
    }
    //case 3: if only prev block is free
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));                            //현재 block의 ftr update
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                 //prev blk의 hdr값 update
        bp = PREV_BLKP(bp);                                      //update bp to bp of previous block
    }
    //case 4: if both are available
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                 //update hdr of previous block
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));                 //update ftr of next block
        bp = PREV_BLKP(bp);                                      //update bp to bp of previous block
    }
    //for next_fit: make sure the rover doesn't point to somewhere inside the newly coalesced block
    if ((rover > (char*)bp) && (rover < NEXT_BLKP(bp)))
    {
        rover = bp;
    }

    return bp;
    //case2에선 현재 block의 footer, next block의 header 아직 취소(?) 안했음
    //case3에선 previous block footer, current block header 안 건드림
    //요약하자면, coalesce한 free block의 맨앞/뒤 hdr/ftr만 업데이트해주고 중간 내용은 굳이(?) 안 건드림
}

static void place(void *bp, size_t asize)
{   
    size_t csize = GET_SIZE(HDRP(bp));                              //get size of current block
    //새로 할당하려는 current block이 꽤나 크다면
    if (csize - asize >= (2 * DSIZE)){                              
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        //값을 넣어주고 나서 남는 공간이 minblocksize 이상이면 free list에 들어갈 수 있게.
        PUT(HDRP(bp), PACK(csize - asize, 0));                      
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    //current block이 내가 allocate하려는 size보다 작으면 헤더에 내가 원하는 큰 값 넣어주기.
    //current block 다음의 block에 대해선 아무 처리도 해주지 않음...?
    else{                                                           
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void *find_fit(size_t asize) //next_fit
{
    char *oldrover = rover;
    //가장 최근에 찾았던 fit block(rover) 부터 heap 끝까지 search
    for(rover = oldrover; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover)){
        if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover)))){        //rover is unallocated and its block is enough to fit asize
            return rover;
        }
    }
    //heap의 처음부터 old rover까지 search
    for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover)){
        if (!GET_ALLOC(HDRP(rover)) && asize <= GET_SIZE(HDRP(rover))){
            return rover;
        }
    }
    return NULL;
}
    
// static void *first_fit(size_t asize) //first fit
// {
//     void *bp;
//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){      //heap의 처음부터, block 크기가 0이 되기 전까지 각 block을 훑는다.
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){         //unallocated with a size >= than asize
//             return bp;                                                      //allocate 'here' 주소 돌려주기
//         }
//     }
//     //unable to find fit
//     return NULL;                                                             
// }


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }

void *mm_malloc(size_t size)            //allocates a block from the free list
{
    size_t asize;// = ALIGN(size + SIZE_T_SIZE);                       //adjusted block size
    size_t extendsize;                  //amount to extend heap if no fit
    char *bp;
    //get adjusted size of block
    if (size == 0){                     //ignore spurious requests
        return NULL;
    }
    if (size <= DSIZE){                 //adjust block size to include overhead and alignment reqs. 
        asize = 2*DSIZE;                //이부분 대신에 MAX(2*DSIZE, ALIGN(size) + DSIZE) 해도 됨.
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //if size > DSIZE일때. 그리고 asize가 int형이라 알아서 소수점아래 버림.
    }
    //search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    //no fit found - get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp)); //현재 블록의 사이즈를 구해서
    PUT(HDRP(bp), PACK(size, 0)); //pack(size, unallocated) 로 현재 블록의 header update
    PUT(FTRP(bp), PACK(size, 0)); //footer에도 똑같이
    coalesce(bp);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;
    
//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//       return NULL;
//     copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     if (size < copySize)
//       copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }
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
    if(newp == NULL){                       //failed to allocate
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(bp));    //원래 bp에 할당됐던 block의 size
    if(size < oldsize){
        oldsize = size;                     //either shrink or leave as is(최대 복사길이는 원래 bp block의 size)
    }
    memcpy(newp, bp, oldsize);              //(복사받을 메모리주소, 복사할 메모리주소, 복사할 메모리 길이)
    mm_free(bp);
    return newp;
}