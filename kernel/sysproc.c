#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sys_flip_display: zero-copy page flip.
//
// Syscall argument 0: user virtual address of a page-aligned buffer
// that is exactly GPU_FB_PAGES (300) * PGSIZE bytes (i.e. 640x480x4 =
// 1,228,800 bytes).  The buffer must already be fully mapped in the
// calling process's address space.
//
// TODO: Students implement this syscall.
uint64
sys_flip_display(void)
{
  return -1;
}

// sys_map_display: map the GPU's kernel framebuffer pages (fb[]) directly
// into the calling process's address space with PTE_U|PTE_R|PTE_W.
//
// Syscall argument 0: desired user virtual address (must be page-aligned).
//   Pass 0 to let the kernel auto-select the next available VA above p->sz.
//
// Returns the mapped virtual address on success, (uint64)-1 on failure.
uint64
sys_map_display(void)
{
  uint64 addr;
  argaddr(0, &addr);

  struct proc *p = myproc();
  void **fb = virtio_gpu_get_fb();
  uint64 fbsize = (uint64)GPU_FB_PAGES * PGSIZE;

  if (addr == 0) {
    addr = PGROUNDUP(p->sz);
  } else {
    if (addr % PGSIZE != 0)
      return (uint64)-1;
    // Collision check: none of the pages in [addr, addr+fbsize) may be mapped
    for (uint64 va = addr; va < addr + fbsize; va += PGSIZE) {
      if (walkaddr(p->pagetable, va) != 0)
        return (uint64)-1;
    }
  }

  if (addr + fbsize > TRAPFRAME)
    return (uint64)-1;

  // Map each fb page (physically non-contiguous) into user space
  for (int i = 0; i < GPU_FB_PAGES; i++) {
    uint64 pa = (uint64)fb[i];
    if (mappages(p->pagetable, addr + (uint64)i * PGSIZE, PGSIZE, pa,
                 PTE_U | PTE_R | PTE_W) != 0) {
      // Partial failure: remove already-installed mappings without freeing pages
      if (i > 0)
        uvmunmap(p->pagetable, addr, i, 0);
      return (uint64)-1;
    }
  }

  p->fb_va = addr;
  return addr;
}
