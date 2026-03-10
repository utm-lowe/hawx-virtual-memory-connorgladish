#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "mem.h"
#include "string.h"
#include "console.h"


///////////////////////////////////////////////////////////////////////////////
// Static Helper Prototypes
///////////////////////////////////////////////////////////////////////////////
static pagetable_t make_kernel_pagetable(void);
static void kernel_map_pages(pagetable_t, uint64, uint64, uint64, int);
static int  kernel_map_range(pagetable_t, uint64, uint64, uint64, int);
static void free_range(void *, void *);



///////////////////////////////////////////////////////////////////////////////
// Global Definitions
///////////////////////////////////////////////////////////////////////////////
extern char end[];
struct frame {
  struct frame *next;
};

struct frame *frame_table;
pagetable_t kernel_pagetable;
extern char etext[];
extern char trampoline[];


///////////////////////////////////////////////////////////////////////////////
// Page Allocation and Virtual Memory API
///////////////////////////////////////////////////////////////////////////////

void
vm_init(void)
{
  free_range(end, (void*)PHYSTOP);
  kernel_pagetable = make_kernel_pagetable();
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}


void *
vm_page_alloc(void)
{
  struct frame *f = frame_table;
  if (f == 0)
    return 0;
  frame_table = f->next;
  memset((void*)f, 0, PGSIZE);
  return (void*)f;
}


void
vm_page_free(void *pa)
{
  struct frame *f = (struct frame*)pa;
  f->next = frame_table;
  frame_table = f;
}


pagetable_t
vm_create_pagetable(void)
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)vm_page_alloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}


uint64
vm_lookup(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;

  if (va >= MAXVA)
    return 0;

  pte = walk_pgtable(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if (!(*pte & PTE_V))
    return 0;

  return PTE2PA(*pte);
}


int
vm_page_insert(pagetable_t pagetable, uint64 va, uint64 pa, int perm)
{
  pte_t *pte;

  va = PGROUNDDOWN(va);

  pte = walk_pgtable(pagetable, va, 1);
  if (pte == 0)
    return -1;
  if (*pte & PTE_V)
    panic("remap");

  *pte = PA2PTE(pa) | perm | PTE_V;
  return 0;
}


void
vm_page_remove(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  pte_t *pte;

  for (uint64 i = 0; i < npages; i++, va += PGSIZE) {
    pte = walk_pgtable(pagetable, va, 0);
    if (pte == 0)
      panic("vm_page_remove: walk_pgtable returned 0");
    if (!(*pte & PTE_V))
      panic("vm_page_remove: page not present");
    if (do_free)
      vm_page_free((void*)PTE2PA(*pte));
    *pte = 0;
  }
}


int
vm_map_range(pagetable_t pagetable, uint64 va, uint64 size, int perm)
{
  uint64 a = PGROUNDDOWN(va);
  uint64 last = PGROUNDDOWN(va + size - 1);
  void *pa;

  for (; a <= last; a += PGSIZE) {
    pa = vm_page_alloc();
    if (pa == 0)
      return -1;
    if (vm_page_insert(pagetable, a, (uint64)pa, perm) != 0) {
      vm_page_free(pa);
      return -1;
    }
  }
  return 0;
}


pte_t * 
walk_pgtable(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk_pgtable");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)vm_page_alloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}


///////////////////////////////////////////////////////////////////////////////
// Static Helper Functions
///////////////////////////////////////////////////////////////////////////////

static pagetable_t
make_kernel_pagetable(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) vm_page_alloc();
  memset(kpgtbl, 0, PGSIZE);

  kernel_map_pages(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  kernel_map_pages(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  kernel_map_pages(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  kernel_map_pages(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  kernel_map_pages(kpgtbl, PGROUNDUP((uint64)etext), PGROUNDUP((uint64)etext),
  PHYSTOP-PGROUNDUP((uint64)etext), PTE_R | PTE_W);
  kernel_map_pages(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  return kpgtbl;
}


static void
kernel_map_pages(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(kernel_map_range(kpgtbl, va, sz, pa, perm) != 0)
    panic("kernel_map_pages");
}


static int
kernel_map_range(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a = PGROUNDDOWN(va);
  uint64 last = PGROUNDDOWN(va + size - 1);

  for (; a <= last; a += PGSIZE, pa += PGSIZE) {
    pte_t *pte = walk_pgtable(pagetable, a, 1);
    if (pte == 0)
      return -1;
    if (*pte & PTE_V)
      panic("kernel_map_range: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
  }
  return 0;
}


static void
free_range(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    vm_page_free(p);
  }
}

int vm_copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
  uint64 n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = vm_lookup(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

int vm_copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
  uint64 n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(dstva);
    pa0 = vm_lookup(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}