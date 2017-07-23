#ifndef ARRAY_H
#define ARRAY_H

// inspired by / ripped off from stb.h
#define array_free(arr) ((arr) ? free(array_ptr(arr)),NULL : NULL)
#define array_push(arr, x) (array_maybe_grow((arr), 1), (arr)[array_raw_len(arr)++] = (x))
#define array_last(arr) ((arr)[array_len(arr)-1])

#define array_len(arr) ((arr) ? array_raw_len(arr) : 0)
#define array_cap(arr) ((arr) ? array_raw_cap(arr) : 0)
#define array_ptr(arr) ((int *)(arr) - 2)
#define array_raw_len(arr) array_ptr(arr)[0]
#define array_raw_cap(arr) array_ptr(arr)[1]

#define array_available(arr) ((arr) ? (array_raw_cap(arr) - array_raw_len(arr)) : 0)
#define array_maybe_grow(arr, n) (array_available(arr) < (n) ? array_grow((arr), (n)) : 0)
#define array_grow(arr, n) ((arr) = __array_grow((arr), (n), sizeof(*(arr))))

#define array_copy(arr) ((arr) ? (_array_copy(arr, sizeof(arr[0]))) : NULL);

void *__array_grow(void *arr, int inc, int elem_size);
void *_array_copy(void *arr, int elem_size);

#endif // ARRAY_H
