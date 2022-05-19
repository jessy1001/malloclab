/*
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000222 25603
 1       yes   99%    5848  0.000210 27808
 2       yes   99%    6648  0.000264 25172
 3       yes  100%    5380  0.000218 24713
 4       yes   66%   14400  0.000272 52922
 5       yes   96%    4800  0.001168  4110
 6       yes   95%    4800  0.001107  4336
 7       yes   55%   12000  0.000463 25912
 8       yes   51%   24000  0.001237 19399
 9       yes   31%   14401  0.123736   116
10       yes   30%   14401  0.003911  3682
Total          75%  112372  0.132809   846

Perf index = 45 (util) + 40 (thru) = 85/100
 */

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
    "Sixth",
    /* First member's full name */
    "Jack",
    /* First member's email address */
    "iacog@kakao.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// malloc 구현 위한 상수 및 매크로 선언부
#define WSIZE 4             /* 워드, 헤더/푸터 사이즈 */
#define DSIZE 8             /* 더블워드 사이즈 */
#define CHUNKSIZE (1 << 12) /* 힙영역 확장시 필요한 사이즈 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))                           /* 최대값 반환 */
#define PACK(size, alloc) ((size) | (alloc))                        /* 할당하는 size와 할당여부 bit를 합쳐준 값을 반환 */
#define GET(p) (*(unsigned int *)(p))                               /* 헤더/푸터 p에 담긴 값 반환 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))                  /* 헤더/푸터 p에 val값 저장 */
#define GET_SIZE(p) (GET(p) & ~0x7)                                 /* 헤더/푸터 p에서 사이즈만 추출 (7:111, ~7:000으로 할당여부 bit를 0으로 마스킹) */
#define GET_ALLOC(p) (GET(p) & 0x1)                                 /* 헤더/푸터 p에서 할당여부 bit만 추출 */
#define HDRP(bp) ((void *)(bp)-WSIZE)                               /* 블럭(페이로드)포인터 bp로부터 헤더p 계산 및 반환 */
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)        /* 블럭(페이로드)포인터 bp로부터 푸터p 계산 및 반환 */
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE((void *)(bp)-WSIZE)) /* 블럭포인터 bp로부터 다음 블럭포인터 bp 계산 및 반환 */
#define PREV_BLKP(bp) ((void *)(bp)-GET_SIZE((void *)(bp)-DSIZE))   /* 블럭포인터 bp로부터 이전 블럭포인터 bp 계산 및 반환 */

// segregated 구현을 위한 매크로 (전제 : free된 영역의 bp에만 사용 가능함.)
#define PUT_ADDRESS(p, val) (*((void **)(p)) = (void *)(val)) /* NXTP/PRVP p 에 주소값 저장 */
#define PRVP(bp) ((void *)(bp))                               /* next 가용영역을 가리키는 주소값을 저장하는 word의 주소 반환 */
#define NXTP(bp) ((void *)(bp) + WSIZE)                       /* prev 가용영역을 가리키는 주소값을 저장하는 word의 주소 반환 */
#define PREV_FBLKP(bp) (*((void **)(PRVP(bp))))               /* prev 가용영역의 주소값 반환 */
#define NEXT_FBLKP(bp) (*((void **)(NXTP(bp))))               /* next 가용영역의 주소값 반환 */

/*
 * mm_init - initialize the malloc package.
 */
//  초기화. 프로그램의 malloc/free 요청으로부터 메모리를 할당/반환할 수 있도록 heap영역 초기화함.
static void *extend_heap(size_t words);
static void *heap_listp; /* 루트 */
int mm_init(void)
{
    // 빈 가용리스트 4워드 할당 및 prologue block 초기화
    if ((heap_listp = mem_sbrk(14 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                                 /* 패딩 */
    PUT(heap_listp + (1 * WSIZE), PACK(6 * DSIZE, 1));  /* prologue header */
    PUT_ADDRESS(heap_listp + (2 * WSIZE), NULL);        /* prologue (~2**1)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (3 * WSIZE), NULL);        /* prologue (~2**2)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (4 * WSIZE), NULL);        /* prologue (~2**3)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (5 * WSIZE), NULL);        /* prologue (~2**4)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (6 * WSIZE), NULL);        /* prologue (~2**5)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (7 * WSIZE), NULL);        /* prologue (~2**6)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (8 * WSIZE), NULL);        /* prologue (~2**7)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (9 * WSIZE), NULL);        /* prologue (~2**8)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (10 * WSIZE), NULL);       /* prologue (~2**9)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (11 * WSIZE), NULL);       /* prologue (2**9)*DSIZE~ seglist ptr */
    PUT(heap_listp + (12 * WSIZE), PACK(6 * DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (13 * WSIZE), PACK(0, 1));         /* epilogue header */
    heap_listp += (2 * WSIZE);                          /* 프롤로그 푸터를 가리킴으로써 힙 영역에서 첫번째 bp의 역할을 함 */

    // 청크사이즈 바이트의 가용블럭으로 힙 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
void *mm_malloc(size_t size)
{
    size_t asize;      /* alignment가 적용된, 실제 할당이 필요한 사이즈 */
    size_t extendsize; /* 가용한 영역이 없을 경우 힙을 확장하기 위한 사이즈 */
    void *bp;

    // 잘못된 요청은 무시
    if (size == 0)
        return NULL;

    // 실제 할당이 필요한 사이즈 계산
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // asize만큼 할당할 수 있는 가용영역이 있으면 할당/영역분할 하고 bp 반환
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // 할당가능 영역이 없으면 heap extend후 할당 및 반환
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
static void *coalesce(void *bp);
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); /* 헤더의 할당여부bit를 0으로 변경 */
    PUT(FTRP(bp), PACK(size, 0)); /* 푸터의 할당여부bit를 0으로 변경 */
    coalesce(bp);                 /* 이전/이후 블럭이 가용블럭이면 연결 */
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *oldbp = bp;
    void *newbp;
    size_t copySize;

    // printf("---------- realloc(%p, %d) ----------\n", bp, size);

    newbp = mm_malloc(size);
    if (newbp == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(oldbp)) - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newbp, oldbp, copySize);
    mm_free(oldbp);
    return newbp;
}

// 초기화 또는 가용메모리 부족시 힙영역 확장
static void *extend_heap(size_t words)
{
    void *bp;
    size_t size;

    // 입력된 인자로 실제 할당에 필요한 사이즈 계산 및 할당
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) /* 할당이 안될경우 -1로 반환되기 때문에 long형으로 바꿔서 확인한듯 */
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         /* 확장된 힙영역을 가용리스트로 초기화(헤더) */
    PUT(FTRP(bp), PACK(size, 0));         /* 확장된 힙영역을 가용리스트로 초기화(푸터) */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 확장된 힙영역의 마지막 워드를 에필로그 헤더로 초기화 */
    PUT_ADDRESS(NXTP(bp), NULL);          /* 일단 NULL로 초기화 */
    PUT_ADDRESS(PRVP(bp), NULL);          /* 일단 NULL로 초기화 */

    return coalesce(bp); /* 이전 블럭이 가용하면 연결해서 반환 */
}

// 이전 이후 가용한 블럭에 대한 연결
static void add_to_freelist(void *bp);
static void change(void *bp);
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case1 : 이전 이후 모두 할당된 경우 - pass
    if (prev_alloc && next_alloc)
    {
    }

    // case2 : 이후 블럭이 가용한 경우
    else if (prev_alloc && !next_alloc)
    {
        change(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case3 : 이전 블럭이 가용한 경우
    else if (!prev_alloc && next_alloc)
    {
        change(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // case4 : 이전 이후 블럭 모두 가용한 경우
    else
    {
        change(PREV_BLKP(bp));
        change(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_to_freelist(bp); /* 분리된 가용리스트의 적절한 위치에 추가 */
    return bp;
}

// 요구 사이즈에 대해서 가용 영역 찾기
static void **get_seglist_ptr(size_t asize);
static void *find_fit(size_t asize)
{
    void **ptr;
    void *bp;
    for (ptr = get_seglist_ptr(asize); ptr != (void **)heap_listp + 10; ptr++) /* 적절한 위치로부터 큰위치로 이동하면서 탐색 */
        for (bp = *ptr; bp != NULL; bp = NEXT_FBLKP(bp))                       /* 다음이 없을때까지 */
            if (GET_SIZE(HDRP(bp)) >= asize)                                   /* 찾았으면 리턴 */
                return bp;
    return NULL; /* 못찾았으면 널 리턴 */
}

// 가용 영역의 포인터 bp에 대해서 헤더/푸터 초기화, 요구 사이즈보다 크면 분할
static void place(void *bp, size_t asize)
{
    size_t original_size = GET_SIZE(HDRP(bp));

    change(bp);                             /* 가용영역을 할당해주기 위해 일단 리스트에서 축출 */
    if (original_size >= asize + 2 * DSIZE) /* 할당할 영역이 지금 영역을 할당하고도 추가로 할당 가능할만큼 클때 */
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp); /* 남은 블럭으로 이동 */
        PUT(HDRP(bp), PACK(original_size - asize, 0));
        PUT(FTRP(bp), PACK(original_size - asize, 0));

        add_to_freelist(bp); /* 남은 블럭을 적당한 리스트에 추가 */
    }
    else /* 충분하지 않을때는 그냥 할당하고 패스 */
    {
        PUT(HDRP(bp), PACK(original_size, 1));
        PUT(FTRP(bp), PACK(original_size, 1));
    }
}

// segregated - 메모리 반환시 적절한 seglist에서 적절한 위치를 찾아 free된 영역 연결
static void add_to_freelist(void *bp)
{
    size_t bp_size = GET_SIZE(HDRP(bp));
    void **ptr = get_seglist_ptr(bp_size); /* 적절한 리스트의 첫번째 포인터 반환 */
    void *cur;
    void *final = NULL;

    for (cur = *ptr; cur != NULL; cur = NEXT_FBLKP(cur)) /* 해당 리스트 안에서 NULL 만나기 전까지 탐색 */
    {
        final = cur;                        /* 조건에 만족하는게 없어서 for문이 종료될 경우, 마지막 블럭 뒤에 삽입하기 위해 마지막 블럭 임시 저장 */
        if (GET_SIZE(HDRP(cur)) >= bp_size) /* 넣으려는 크기보다 크거나 같은 블럭이 있으면 그 앞에 삽입 */
        {
            PUT_ADDRESS(NXTP(bp), cur);
            PUT_ADDRESS(PRVP(bp), PREV_FBLKP(cur));
            PUT_ADDRESS(PREV_FBLKP(cur) == (void *)ptr ? (void *)ptr : NXTP(PREV_FBLKP(cur)), bp); /* 이전 블럭이 첫번째 블럭을 가리키는 리스트의 첫번째 포인터일 경우 따로 처리 */
            PUT_ADDRESS(PRVP(cur), bp);
            return;
        }
    }
    // cur가 NULL로 빠져나온 경우 (1. 애초에 처음에 연결된 블럭이 없는 경우 / 2. 조건을 만족하지 못해서 (다 작은 사이즈여서) 맨 끝까지 가서 나온 경우) 따로 처리
    PUT_ADDRESS(NXTP(bp), NULL);
    PUT_ADDRESS(PRVP(bp), final == NULL ? ptr : final); /* final이 NULL이면 아예 for문 못돈 1번 경우, NULL이 아니면 final은 해당 리스트의 마지막 블럭을 가리킴 */
    PUT_ADDRESS(final == NULL ? (char *)ptr : NXTP(final), bp);
    return;
}

// segregated - 반환되는 bp 기준 전/후 free 영역 연결
static void change(void *bp)
{
    void *prvp = PREV_FBLKP(bp);
    void *nxtp = NEXT_FBLKP(bp);

    if ((void **)prvp < (void **)heap_listp + 10) /* 이전꺼가 맨 처음이면 그냥 이전꺼에 direct로 nxt 연결함 */
        PUT_ADDRESS(prvp, nxtp);
    else
        PUT_ADDRESS(NXTP(prvp), nxtp); /* 아니면 prv에 nxt자리에 nxt연결 */
    if (nxtp != NULL)
        PUT_ADDRESS(PRVP(nxtp), prvp);
}

// 필요한 사이즈에 따라 필요한 seglist의 ptr를 반환
static char ceil_log2(size_t size);
static void **get_seglist_ptr(size_t asize)
{
    return (void **)heap_listp + ceil_log2(asize) - 4;
}

// 매개변수로 받은 size의 log2값의 올림 반환 (2의 n승일 경우 n, 2의 n승보다 큰 경우 n+1 반환. 리스트의 최대 인덱스 고려하여 최대인덱스보다 큰 값일 경우 가능한 최대값 반환)
// 들어올 때 최소 사이즈 16으로 들어옴.그러니까 16~31인 애들을 heap_listp가 가리키는 seg_list의 첫 칸에 넣고 싶은 거임.
static char ceil_log2(size_t size)
{
    size_t tmp_size = size;
    char cnt;
    for (cnt = -1; tmp_size != 0; cnt++)
        tmp_size >>= 1;

    if (size == 1 << cnt)
        return cnt <= 13 ? cnt : 13;
    else
        return (cnt + 1) <= 13 ? (cnt + 1) : 13;
}