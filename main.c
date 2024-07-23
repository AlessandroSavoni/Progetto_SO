#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define BUDDY_SIZE (1024 * 1024)  // 1 MB
#define PAGE_SIZE 4096
#define SMALL_THRESHOLD (PAGE_SIZE / 4)
#define BITMAP_SIZE (BUDDY_SIZE / 8)

static uint8_t* buddy_memory = NULL;
static uint8_t bitmap[BITMAP_SIZE];


typedef struct {
    void* address;
    size_t size;
    int is_free;
} Allocation;

#define MAX_ALLOCATIONS 1000
Allocation allocations[MAX_ALLOCATIONS];
int allocation_count = 0;





void init_buddy() {
    if (!buddy_memory) {
        buddy_memory = mmap(NULL, BUDDY_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (buddy_memory == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }
        memset(bitmap, 0, BITMAP_SIZE);
    }
}

int find_buddy(size_t size) {
    int order = 0;
    size_t block_size = BUDDY_SIZE;

    while (block_size > size) {
        block_size /= 2;
        order++;
    }

    for (int i = 0; i < BITMAP_SIZE; i++) {
        for (int j = 0; j < 8; j++) {
            if (!(bitmap[i] & (1 << j))) {
                int start = i * 8 + j;
                int end = start + (1 << order);
                int free = 1;
                for (int k = start; k < end; k++) {
                    if (bitmap[k / 8] & (1 << (k % 8))) {
                        free = 0;
                        break;
                    }
                }
                if (free) {
                    for (int k = start; k < end; k++) {
                        bitmap[k / 8] |= (1 << (k % 8));
                    }
                    return start * (BUDDY_SIZE / BITMAP_SIZE / 8);
                }
            }
        }
    }
    return -1;
}

void* pseudo_malloc(size_t size) {
    init_buddy();

    void* ptr = NULL;

    if (size < SMALL_THRESHOLD) {
        int offset = find_buddy(size);
        if (offset >= 0) {
            printf("buddy allocation used\n");
            ptr = buddy_memory + offset;
        }
    }

    // large allocation or buddy fail
    if(ptr == NULL) {
        int pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        size_t alloc_size = pages * PAGE_SIZE;
        ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
           perror("mmap failed");
           return NULL;
        }
        printf("mmap allocation used\n");
    }
    
    if (allocation_count < MAX_ALLOCATIONS) {
        allocations[allocation_count].address = ptr;
        allocations[allocation_count].size = size;
        allocations[allocation_count].is_free = 0;
        allocation_count++;
    } else {
        fprintf(stderr, "Attenzione: raggiunto il numero massimo di allocazioni.\n");
    }


    return ptr;
}

void pseudo_free(void* ptr) {
    for (int i = 0; i < allocation_count; i++) {
        if (allocations[i].address == ptr && !allocations[i].is_free) {
            if (ptr >= (void*)buddy_memory && ptr < (void*)(buddy_memory + BUDDY_SIZE)) {
                size_t offset = (uint8_t*)ptr - buddy_memory;
                int index = offset / (BUDDY_SIZE / BITMAP_SIZE / 8);
                bitmap[index / 8] &= ~(1 << (index % 8));
            } else {
                munmap(ptr, allocations[i].size);
            }
            allocations[i].is_free = 1;
            printf("La memoria all'indirizzo %p e' stata liberata.\n", ptr);
            return;
        }
    }
    fprintf(stderr, "Errore: Stai cercando di liberare un'allocazione gia libera.\n");
}


void print_allocations() {
    printf("\nStorico allocazioni:\n");
    for (int i = 0; i < allocation_count; i++) {
        if (!allocations[i].is_free) {
            printf("%d: Indirizzo: %p, Dimensione: %d bytes\n", i, allocations[i].address, allocations[i].size);
        }
    }
}

int main() {
    int choice;
    size_t size;
    int index;

    while (1) {
        printf("\nPseudo Allocator Menu:\n");
        printf("1. Alloca memoria\n");
        printf("2. Libera Memoria\n");
        printf("3. Stampa lo storico delle allocazioni\n");
        printf("4. Esci\n");
        printf("Digita il numero per scegliere cosa fare: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("Inserisci quanti byte vuoi allocare: ");
                scanf("%d", &size);
                void* ptr = pseudo_malloc(size);
                if (ptr) {
                    printf("Allocati %d bytes all'indirizzo %p\n", size, ptr);
                }
                break;
            case 2:
                print_allocations();
                printf("Inserisci l'indice dell'allocazione da liberare: ");
                scanf("%d", &index);
                if (index >= 0 && index < allocation_count && !allocations[index].is_free) {
                    pseudo_free(allocations[index].address);
                } else {
                    printf("Indice non valido, allocazione gia libera.\n");
                }
                break;
            case 3:
                print_allocations();
                break;
            case 4:
                return 0;
            default:
                printf("Scelta non valida, riprova...\n");
        }
    }

    return 0;
}
