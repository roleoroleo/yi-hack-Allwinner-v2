#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern void *lw_malloc(size_t size);
extern void lw_free(void *ptr);
extern void *lw_realloc(void *ptr, size_t size);
extern void *lw_calloc(size_t nmemb, size_t size);

void *malloc(size_t size) { return lw_malloc(size); }

void free(void *ptr) { lw_free(ptr); }

void *realloc(void *ptr, size_t size) { return lw_realloc(ptr, size); }

void *calloc(size_t nmemb, size_t size) { return lw_calloc(nmemb, size); }

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) (x > y ? x : y)
#define PACK(size, alloc) (size | alloc)
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (size_t)(val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define NEXT_BLKP_S(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define GET_NEXT(bp) (*(void **)((char *)(bp) + WSIZE))
#define GET_PREV(bp) (*(void **)(bp))
#define SEGSIZE 62
#define DEFAULT_HEAP  1 * (1 << 20)
#define GET_ROOT(class) (*(void **)((char *)(free_listp) + (class * WSIZE)))
#define IS_BUFFER(p) ((GET(HDRP(p)) >> 1) & 0x1)
#define IS_BIN(p) ((GET(HDRP(p)) >> 2) & 0x1)
#define IS_BIN_N_BUF(p) (GET(HDRP(p)) & 0x6)
#define IS_BUF_N_ALOC(p) (GET(HDRP(p)) & 0x3)
#define GET_NEXT_S(bp) (*(void **)(bp))

static void *lw_find_fit(size_t size);
static void *lw_place(void *bp, size_t size);
static void lw_remove_free_block(void *bp);
static void lw_add_free_block(void *bp);
inline int lw_get_class(size_t size);
static void lw_deferred_coalescing();
inline void set_block(void* ptr, size_t size, int alloc);
static char *mem_start_brk;
static char *mem_max_addr;
static char *mem_brk;
static char *heap_listp = NULL;
static char *free_listp = NULL;

inline void set_block(void* ptr, size_t size, int alloc) {
	*(size_t *)((char *)(ptr) - WSIZE) = (size | alloc);
    *(size_t *)((char *)(ptr) + size - DSIZE) = (size | alloc);
}

void lw_mem_init(void)
{
	if ((mem_start_brk = (char *)sbrk(DEFAULT_HEAP)) == NULL)
		exit(1);

	mem_max_addr = mem_start_brk + DEFAULT_HEAP;
	mem_brk = mem_start_brk;
}

void *lw_sbrk(int incr)
{
	char *old_brk = mem_brk;
	if ((mem_brk + incr) > mem_max_addr)
	{
		int size = MAX(incr, DEFAULT_HEAP);
		sbrk(size);
		mem_max_addr = mem_max_addr + size;
	}

	mem_brk += incr;
	return (void *)old_brk;
}

inline int lw_get_class(size_t size)
{
	int ind = 7;
	while ((1 << ind) < size)
	{
		ind++;
	}

	ind = ind + 9;

	if (ind > SEGSIZE - 1)
		return SEGSIZE - 1;

	return ind;
}

static void lw_remove_free_block(void *bp)
{
	int size = (int)GET_SIZE(HDRP(bp));
	int class = lw_get_class(size);

	if (bp == GET_ROOT(class))
	{
		GET_ROOT(class) = GET_NEXT(GET_ROOT(class));
		return;
	}
	GET_NEXT(GET_PREV(bp)) = GET_NEXT(bp);

	if (GET_NEXT(bp) != NULL)
		GET_PREV(GET_NEXT(bp)) = GET_PREV(bp);
}

static void lw_add_free_block(void *bp)
{
	int class = lw_get_class(GET_SIZE(HDRP(bp)));

	GET_NEXT(bp) = GET_ROOT(class);
	if (GET_ROOT(class) != NULL)
		GET_PREV(GET_ROOT(class)) = bp;

	GET_ROOT(class) = bp;
}

void alloc_init(void)
{
	lw_mem_init();

	if ((heap_listp = lw_sbrk((SEGSIZE + 4) * WSIZE)) == (void *)-1)
		return -1;

	PUT(heap_listp, 0);
	PUT(heap_listp + (1 * WSIZE), PACK((SEGSIZE + 2) * WSIZE, 1));
	for (int i = 0; i < SEGSIZE; i++)
		PUT(heap_listp + ((2 + i) * WSIZE), NULL);
	PUT(heap_listp + ((2 + SEGSIZE) * WSIZE), PACK((SEGSIZE + 2) * WSIZE, 1));
	PUT(heap_listp + ((3 + SEGSIZE) * WSIZE), PACK(0, 1));
	free_listp = heap_listp + 2 * WSIZE;
}

void *lw_malloc(size_t size)
{
	if (heap_listp == NULL)
		alloc_init();

	size_t asize;
	size_t extendsize;
	char *bp;
	char *ptr;
	if (size <= 120)
	{
		size_t asize = ALIGN(size + WSIZE);
		if (asize == 8)
			asize = 16;

		int class = (asize >> 3);

		if ((bp = GET_ROOT(class)) != NULL)
		{
			if (!GET_ALLOC(HDRP(bp)))
			{
				size_t csize = GET_SIZE(HDRP(bp));

				if (csize < (asize << 1))
				{
					PUT(HDRP(bp), PACK(asize, 5));
					GET_ROOT(class) = NULL;
					return bp;
				}
				else
				{
					PUT(HDRP(bp), PACK(asize, 5));
					PUT(HDRP(NEXT_BLKP_S(bp)), PACK(csize - asize, 4));
					GET_ROOT(class) = NEXT_BLKP_S(bp);
					return bp;
				}
			}
			GET_ROOT(class) = GET_NEXT_S(bp);
			return bp;
		}
		else
		{
			if ((bp = lw_sbrk(CHUNKSIZE)) == NULL)
				return NULL;

			set_block(bp, CHUNKSIZE, 1);

			PUT(HDRP(NEXT_BLKP_S(bp)), PACK(0, 1));

			bp += WSIZE;

			PUT(HDRP(bp), PACK(asize, 5));

			PUT(HDRP(NEXT_BLKP_S(bp)), PACK(CHUNKSIZE - DSIZE - asize, 4));
			GET_ROOT(class) = NEXT_BLKP_S(bp);
			return bp;
		}
		return NULL;
	}

	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = ALIGN(2 * WSIZE + size);

	lw_deferred_coalescing();

	if ((bp = lw_find_fit(asize)) != NULL)
	{
		return lw_place(bp, asize);
	}
	else
	{
		size_t new_size = asize;
		if (!GET_ALLOC(HDRP(PREV_BLKP(mem_brk))))
		{
			size_t end_size = GET_SIZE(HDRP(PREV_BLKP(mem_brk)));
			if (asize >= end_size)
			{
				bp = PREV_BLKP(mem_brk);
				lw_remove_free_block(bp);
				new_size = asize - end_size;

				if (lw_sbrk(new_size) == NULL)
				{
					return NULL;
				}

				set_block(bp, asize, 1);
				PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
				return bp;
			}
		}
		else
		{

			if ((bp = lw_sbrk(asize)) == NULL)
			{
				return NULL;
			}
			set_block(bp, asize, 1);
			PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
			return bp;
		}
	}
}

void lw_free(void *bp)
{
	if (bp == NULL)
		return NULL;

	size_t size = GET_SIZE(HDRP(bp));

	if (IS_BIN(bp))
	{
		int class = (size >> 3);
		GET_NEXT_S(bp) = GET_ROOT(class);
		GET_ROOT(class) = bp;
		return;
	}

	int prev_buf_n_alloc = IS_BUF_N_ALOC(PREV_BLKP(bp));
	int next_buf_n_alloc = IS_BUF_N_ALOC(NEXT_BLKP(bp));

	if ((prev_buf_n_alloc == 2) || (next_buf_n_alloc == 2))
	{
		set_block(bp, size, 6);
		return;
	}
	else if ((prev_buf_n_alloc == 0) || (next_buf_n_alloc == 0))
	{
		set_block(bp, size, 2);
		GET_NEXT(bp) = GET_ROOT(1);
		if (GET_ROOT(1) != NULL)
			GET_PREV(GET_ROOT(1)) = bp;
		GET_ROOT(1) = bp;
	}
	else if ((prev_buf_n_alloc == 1) && (next_buf_n_alloc == 1))
	{
		set_block(bp, size, 0);
		lw_add_free_block(bp);
	}
}

void *lw_calloc(size_t nmemb, size_t size)
{
	size_t bytes = nmemb * size;
	void *new_ptr = lw_malloc(bytes);
	if (bytes <= 120)
		bytes = GET_SIZE(HDRP(new_ptr)) - WSIZE;
	else
		bytes = GET_SIZE(HDRP(new_ptr)) - DSIZE;
	if (new_ptr == NULL)
		return NULL;

	memset(new_ptr, 0, bytes);
	return new_ptr;
}

void *lw_realloc(void *ptr, size_t size)
{
	size_t asize;
	void *newptr;

	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = ALIGN(2 * WSIZE + size);

	if (ptr == NULL)
		return lw_malloc(size);

	size_t oldsize = GET_SIZE(HDRP(ptr));

	if (size <= 0)
	{
		lw_free(ptr);
		return 0;
	}

	if (asize <= oldsize)
		return ptr;

	lw_deferred_coalescing();

	if ((GET_SIZE(HDRP(ptr)) <= 128) && IS_BIN(ptr))
	{
		if (size > 120)
		{
			newptr = lw_malloc(size);
			size_t oldsize = GET_SIZE(HDRP(ptr));
			if (newptr == NULL)
				return NULL;

			memcpy(newptr, ptr, (GET_SIZE(HDRP(ptr)) - WSIZE));
			lw_free(ptr);

			return newptr;
		}
		else
		{
			newptr = lw_malloc(size);
			size_t oldsize = GET_SIZE(HDRP(ptr));
			if (newptr == NULL)
				return NULL;

			memcpy(newptr, ptr, (GET_SIZE(HDRP(ptr)) - WSIZE));
			lw_free(ptr);
			return newptr;
		}
	}

	int prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
	size_t prev_size = GET_SIZE(FTRP(PREV_BLKP(ptr)));
	int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
	size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
	if (PREV_BLKP(mem_brk) == ptr)
	{
		next_alloc = 1;
	}

	if (!next_alloc)
	{
		if ((oldsize + next_size) >= asize)
		{
			lw_remove_free_block(NEXT_BLKP(ptr));
			set_block(ptr, oldsize+next_size, 1);
			return ptr;
		}
	}
	else if ((!next_alloc) && (!prev_alloc) && ((oldsize + prev_size + next_size) >= asize))
	{
		void *prev_block = PREV_BLKP(ptr);
		if (prev_size >= oldsize)
		{
			lw_remove_free_block(PREV_BLKP(ptr));
			lw_remove_free_block(NEXT_BLKP(ptr));
			if ((prev_size + oldsize + next_size - asize) <= 128)
			{
				memcpy(prev_block, ptr, (GET_SIZE(HDRP(ptr)) - DSIZE));
				set_block(prev_block, prev_size + oldsize + next_size - asize, 1);
				return prev_block;
			}
			else
			{
				memcpy(prev_block, ptr, (GET_SIZE(HDRP(ptr)) - DSIZE));
				set_block(prev_block, asize, 1);
				set_block(NEXT_BLKP(prev_block), prev_size + oldsize + next_size - asize, 0);

				lw_add_free_block(NEXT_BLKP(prev_block));
				return prev_block;
			}
		}
		else if (prev_size < oldsize)
		{
			lw_remove_free_block(PREV_BLKP(ptr));
			lw_remove_free_block(NEXT_BLKP(ptr));
			int total_movesize = GET_SIZE(HDRP(ptr)) - DSIZE;
			int sep_movesize = GET_SIZE(HDRP(prev_block));
			int n = total_movesize / sep_movesize;

			for (int i = 0; i < n; i++)
				memcpy(prev_block + i * sep_movesize, ptr + i * sep_movesize, sep_movesize);
			memcpy(prev_block + n * sep_movesize, ptr + n * sep_movesize, total_movesize - (sep_movesize * n));

			if (((prev_size + oldsize + next_size) - asize) <= 128)
			{
				set_block(prev_block, prev_size + oldsize + next_size, 1);
				return prev_block;
			}
			else
			{
				set_block(prev_block, asize, 1);
				set_block(NEXT_BLKP(prev_block), prev_size + oldsize + next_size - asize, 0);
				lw_add_free_block(NEXT_BLKP(prev_block));
				return prev_block;
			}
		}
	}
	else if ((next_alloc) && (!prev_alloc) && ((oldsize + prev_size) >= asize))
	{
		void *prev_block = PREV_BLKP(ptr);
		if (prev_size >= oldsize)
		{
			lw_remove_free_block(PREV_BLKP(ptr));
			if ((prev_size + oldsize - asize) <= 128)
			{
				memcpy(prev_block, ptr, (GET_SIZE(HDRP(ptr)) - DSIZE));
				set_block(prev_block, prev_size + oldsize, 1);
				return prev_block;
			}
			else
			{
				memcpy(prev_block, ptr, (GET_SIZE(HDRP(ptr)) - DSIZE));
				set_block(prev_block, asize, 1);
				set_block(NEXT_BLKP(prev_block), prev_size + oldsize - asize, 0);

				lw_add_free_block(NEXT_BLKP(prev_block));
				return prev_block;
			}
		}
		else if (prev_size < oldsize)
		{
			lw_remove_free_block(PREV_BLKP(ptr));
			int total_movesize = GET_SIZE(HDRP(ptr)) - DSIZE;
			int sep_movesize = GET_SIZE(HDRP(prev_block));
			int n = total_movesize / sep_movesize;

			for (int i = 0; i < n; i++)
			{
				memcpy(prev_block + i * sep_movesize, ptr + i * sep_movesize, sep_movesize);
			}
			memcpy(prev_block + n * sep_movesize, ptr + n * sep_movesize, total_movesize - (sep_movesize * n));

			if (((prev_size + oldsize) - asize) <= 128)
			{
				set_block(prev_block, prev_size + oldsize, 1);
				return prev_block;
			}
			else
			{
				set_block(prev_block, asize, 1);
				set_block(NEXT_BLKP(prev_block), prev_size + oldsize - asize, 0);
				lw_add_free_block(NEXT_BLKP(prev_block));
				return prev_block;
			}
		}
	}

	newptr = lw_malloc(size);
	if (newptr == NULL)
		return NULL;

	memcpy(newptr, ptr, (GET_SIZE(HDRP(ptr)) - DSIZE));
	lw_free(ptr);

	return newptr;
}

static void lw_deferred_coalescing()
{
	int class = 1;
	char *ptr = GET_ROOT(class);
	void *bp;

	if (ptr == NULL)
		return;

	if (ptr == 0)
		return;

	while (ptr != NULL)
	{
		char *start_ptr = ptr;
		char *next_ptr = ptr;
		size_t totalsize = 0;
		while (start_ptr >= (char *)heap_listp + 2 * WSIZE)
		{
			totalsize += GET_SIZE(HDRP(start_ptr));
			if (IS_BUFFER(start_ptr) && !IS_BIN(start_ptr))
			{
				if (start_ptr == GET_ROOT(class))
				{
					GET_ROOT(class) = GET_NEXT(GET_ROOT(class));
				}
				else
				{
					GET_NEXT(GET_PREV(start_ptr)) = GET_NEXT(start_ptr);

					if (GET_NEXT(start_ptr) != NULL)
						GET_PREV(GET_NEXT(start_ptr)) = GET_PREV(start_ptr);
				}
			}

			else if (!IS_BUFFER(start_ptr) && !IS_BIN(start_ptr))
				lw_remove_free_block(start_ptr);

			if (GET_ALLOC(HDRP(PREV_BLKP(start_ptr))))
				break;

			else
			{
				if (start_ptr == PREV_BLKP(start_ptr))
					break;
				start_ptr = PREV_BLKP(start_ptr);
			}
		}

		while (next_ptr <= ((char *)mem_brk - WSIZE))
		{
			next_ptr = NEXT_BLKP(next_ptr);

			if (!GET_ALLOC(HDRP(next_ptr)))
			{
				totalsize += GET_SIZE(HDRP(next_ptr));

				if (IS_BUFFER(next_ptr) && !IS_BIN(next_ptr))
				{
					if (next_ptr == GET_ROOT(class))
					{
						GET_ROOT(class) = GET_NEXT(GET_ROOT(class));
					}
					else
					{
						GET_NEXT(GET_PREV(next_ptr)) = GET_NEXT(next_ptr);

						if (GET_NEXT(next_ptr) != NULL)
							GET_PREV(GET_NEXT(next_ptr)) = GET_PREV(next_ptr);
					}
				}

				else if (!IS_BUFFER(next_ptr) && !IS_BIN(next_ptr))
					lw_remove_free_block(next_ptr);
			}
			else
				break;
		}
		set_block(start_ptr, totalsize, 0);
		lw_add_free_block(start_ptr);

		ptr = GET_ROOT(class);
	}
}

static void *lw_find_fit(size_t asize)
{
	int class = lw_get_class(asize);
	void *bp = GET_ROOT(class);

	while (class < SEGSIZE)
	{
		bp = GET_ROOT(class);
		while (bp != NULL)
		{

			if ((asize <= GET_SIZE(HDRP(bp))))
				return bp;

			bp = GET_NEXT(bp);
		}
		class += 1;
	}
	return NULL;
}

static void *find_fit_one_class(int class)
{
	void *bp = GET_ROOT(class);
	while (bp != NULL)
		return bp;

	return NULL;
}

static void *lw_place(void *bp, size_t asize)
{
	lw_remove_free_block(bp);
	size_t csize = GET_SIZE(HDRP(bp));

	if ((csize - asize) <= 128)
	{
		set_block(bp, csize, 1);
		return bp;
	}
	else
	{
		set_block(bp, asize, 1);
		set_block(NEXT_BLKP(bp), csize - asize, 0);
		lw_add_free_block(NEXT_BLKP(bp));
		return bp;
	}
}