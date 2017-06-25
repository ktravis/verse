#include <stdlib.h>

#include "array.h"

void *__array_grow(void *arr, int inc, int elem_size) {
    int doubled = array_cap(arr) * 2;
    int needed = array_len(arr) + inc;
    int new_size = doubled > needed ? doubled : needed;
    int *p = (int *) realloc(arr ? array_ptr(arr) : NULL, elem_size * new_size + sizeof(int) * 2);
    if (!arr) {
        // need to clear this
        p[0] = 0;
    } // otherwise keep the old value
    p[1] = new_size;
    return p + 2;
}

void *_array_copy(void *arr, int elem_size) {
    char *x = malloc(array_cap(arr) * elem_size + 2 * sizeof(int));
    int *raw = array_ptr(arr);
    for (int i = 0; i < elem_size * array_cap(arr) + sizeof(int)*2; i++) {
        x[i] = ((char*)raw)[i]; 
    }
    return ((int *)x) + 2;
}
