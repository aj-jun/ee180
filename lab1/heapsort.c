/**********************************************************
* heapsort.c                                             *
*                                                        *
* This program sorts using a simple heapsort algorithm.  * 
**********************************************************/

#include <stdio.h>
#include <stdlib.h>

static void swap(unsigned *a, unsigned *b);
static void heapify(unsigned *arr, unsigned n, unsigned i);
static void build_max_heap(unsigned *arr, unsigned n);
void heapsort(unsigned *arr, unsigned n);

int main() {
    unsigned *array, i, array_size;

    printf("How many elements to be sorted? ");
    int tokens_read = scanf("%u", &array_size);
    if (tokens_read != 1) {
        printf("Could not read array size.\n");
        exit(1);
    }

    array = (unsigned *) malloc(sizeof(unsigned) * (array_size + 1));

    if (array == NULL) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    for (i = 0; i < array_size; i++) {
        printf("Enter next element: ");
        tokens_read = scanf("%u", &(array[i]));
        if (tokens_read != 1) {
            printf("Could not read the next element.\n");
            exit(1);
        }
    }

    // heap sort call
    if (array_size > 0) {
        heapsort(array, array_size);
    }

    printf("The sorted list is:\n");
    for (i = 0; i < array_size; i++) {
        printf("%u ", array[i]);
    }
    printf("\n");

    free(array);
    return 0;
}


/* Swap two unsigned integers by pointer */
static void swap(unsigned *a, unsigned *b) {
    unsigned tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Recursive heapify for a max-heap.
 * Restores the max-heap property for the subtree rooted at index i,
 * assuming the subtrees below i are already max-heaps.
 *
 * arr: array base pointer
 * n:   current heap size (valid indices are 0..n-1)
 * i:   index to heapify
 */
static void heapify(unsigned *arr, unsigned n, unsigned i) {
    unsigned largest = i;
    // Heap index mapping (0-indexed):
    // left = 2*i + 1, right = 2*i + 2, parent = (i-1)/2
    unsigned left  = 2 * i + 1;
    unsigned right = 2 * i + 2;

    if (left < n && arr[left] > arr[largest]) {
        largest = left;
    }
    if (right < n && arr[right] > arr[largest]) {
        largest = right;
    }

    // If a child is larger, swap and recurse into that child to fix that subtree.
    if (largest != i) {
        swap(&arr[i], &arr[largest]);
        heapify(arr, n, largest);
    }
}

/* Build a max-heap from an arbitrary array */
static void build_max_heap(unsigned *arr, unsigned n) {
    // Last internal node (node that has at least one child) is (n/2)-1
    // Indices n/2 through n-1 are leaves in a 0-indexed heap.
    for (int i = (int)(n / 2) - 1; i >= 0; i--) {
        heapify(arr, n, (unsigned)i);
    }
}

/* Heap sort: sorts arr[0..n-1] ascending */
void heapsort(unsigned *arr, unsigned n) {
    if (n <= 1) return;

    build_max_heap(arr, n);

    // Repeatedly move max (root) to end, shrink heap, heapify root.
    for (unsigned end = n - 1; end > 0; end--) {
        swap(&arr[0], &arr[end]);
        heapify(arr, end, 0);
    }
}


