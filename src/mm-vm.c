//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

// static pthread_mutex_t mtx_lock;
// Pair used for storing information of processes
struct proc {
    int pid[1000];
    int offset[1000];
};

// Function to initialize the proc structure
void init_proc(struct proc *p) {
    // Initialize the pid and occupy arrays to some default values
    // For simplicity, let's initialize them to zero
    for (int i = 0; i < 1000; i++) {
        p->pid[i] = 0;
        p->offset[i] = 0;
    }
}
struct proc p_count;
/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  struct vm_rg_struct *tmp = (struct vm_rg_struct*)malloc(sizeof(struct vm_rg_struct));
  tmp->rg_end = rg_elmt.rg_end;
  tmp->rg_next = rg_elmt.rg_next;
  tmp->rg_start = rg_elmt.rg_start;

  if (rg_elmt.rg_start >= rg_elmt.rg_end)
    return -1;

  if (rg_node != NULL)
    tmp->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = tmp;

  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap;

  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  while (vmait < vmaid)
  {
    if(pvma == NULL)
	  return NULL;

    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  rgnode = *get_symrg_byid(caller->mm, rgid);
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;
    // print_list_fp(caller->mram->used_fp_list);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /*Attempt to increate limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  //int inc_limit_ret
  int old_sbrk ;

  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  inc_vma_limit(caller, vmaid, inc_sz);

  /*Successful increase limit */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;
  // print_list_fp(caller->mram->used_fp_list);
  // pthread_mutex_unlock(&mtx_lock);
  return 0;
}


/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct rgnode;
  // pthread_mutex_lock(&mtx_lock);
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* TODO: Manage the collect freed region to freerg_list */
  rgnode = *get_symrg_byid(caller->mm, rgid);
  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, rgnode);
  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
   return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int page_num, int *frame_num,
               struct pcb_t *pcb) {
  uint32_t page_entry = mm->pgd[page_num];

  if (!PAGING_PAGE_PRESENT(
          page_entry)) { /* Page is not online, make it actively living */
    int victim_page_num, swap_frame_num;
    int victim_frame_num;
    uint32_t victim_page_entry;

    int target_frame_num =
        PAGING_SWP(page_entry); // the target frame storing our variable
    // int free_frame_num;         // finding free frame in RAM

    /* TODO: Play with your paging theory here */

    /* Find victim page */
    find_victim_page(pcb->mm, &victim_page_num);
  

    victim_page_entry = pcb->mm->pgd[victim_page_num];
    victim_frame_num = PAGING_FPN(victim_page_entry);

    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(pcb->active_mswp, &swap_frame_num) < 0) {
      printf("Not enough space in swap\n");
      return -1;
    }

    __swap_cp_page(pcb->mram, victim_frame_num, pcb->active_mswp,
                   swap_frame_num);

    /* Copy target frame from swap to mem */
    __swap_cp_page(pcb->active_mswp, target_frame_num, pcb->mram,
                   victim_frame_num);

    MEMPHY_put_freefp(pcb->active_mswp, target_frame_num);

    /* Update page table */
    pte_set_swap(&pcb->mm->pgd[victim_page_num], 0, swap_frame_num);
    pte_set_fpn(&mm->pgd[page_num], victim_frame_num);

#ifdef CPU_TLB
    /* Update its online status of TLB (if needed) */
#endif

    enlist_pgn_node(&pcb->mm->fifo_pgn, page_num);
    
    //return 0;
  }

  *frame_num = PAGING_FPN(page_entry);
  return 0;
}
/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram,phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_write(caller->mram,phyaddr, value);

   return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  int size_rg = proc->mm->symrgtbl[source].rg_end - proc->mm->symrgtbl[source].rg_start;

	if (offset > size_rg)
	{
		printf("Invalid Reading: region of %d range from %ld to %ld but you read at %ld\n",
			   source,
			   proc->mm->symrgtbl[source].rg_start,
			   proc->mm->symrgtbl[source].rg_end,
			   proc->mm->symrgtbl[source].rg_start + offset);
		return -1;
	}
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  destination = (uint32_t) data;
  usleep(100);
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  currg->rg_start = 0;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  for(int x = 0; x < 1000; x++){
      if(p_count.pid[x] == 0 && p_count.offset[x] == 0){
        p_count.pid[x] = caller->pid;
        p_count.offset[x] = currg->rg_start + offset;
        pg_setval(caller->mm, currg->rg_start + offset, value, caller);
        break;
      }
      else if (p_count.pid[x] == caller->pid && p_count.offset[x] == currg->rg_start + offset){
        pg_setval(caller->mm, currg->rg_start + offset, value, caller);
        break;
      }
      else if (p_count.pid[x] != caller->pid && p_count.offset[x] == currg->rg_start + offset){
        printf("process has been occupied!\n");
        break;
      }
      else if (p_count.pid[x] != caller->pid && p_count.offset[x] != currg->rg_start + offset){
        continue;
      }
  }
  // for (int i = 0; i < 10; i++){
  //   printf("[%d, %d] ", p_count.pid[i], p_count.offset[i]);
  // }
  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;
  // if(offset == currg->rg_start + offset){
  //   return -1;
  // }

  // printf("test offset: %lu \n", (temp->rg_start));
  printf("process in writing: %d \n", caller->pid);
  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
  int size_rg = proc->mm->symrgtbl[destination].rg_end - proc->mm->symrgtbl[destination].rg_start;
  printf("proccess: %d \n",proc->pid);
  
	if (offset > size_rg)
	{
		printf("Invalid Writing: region of %d range from %ld to %ld but you write at %ld\n",
			   destination,
			   proc->mm->symrgtbl[destination].rg_start,
			   proc->mm->symrgtbl[destination].rg_end,
			   proc->mm->symrgtbl[destination].rg_start + offset);
		return -1;
	}
  int val = __write(proc, 0, destination, offset, data);
  usleep(100);
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }
  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  if(newrg->rg_end > cur_vma->sbrk){
    newrg->rg_end = cur_vma->sbrk; // TODO
  }
  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = caller->mm->mmap;

  /* TODO validate the planned memory area is not overlapped */
  while(vma!=NULL){
    if(vmaid != vma->vm_id){
      // start với end nằm xen kẽ nhau
      if(OVERLAP(vmastart, vmaend, vma->vm_start, vma->vm_end)){
        if(vmastart!=vma->vm_end || vmaend!=vma->vm_start){
          return -1;
        }
      }
    }
    vma = vma->vm_next;
  }
  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1; /*Overlap and failed allocation */

  /* The obtained vm area (only) 
   * now will be alloc real ram region */
  cur_vma->vm_end += inc_sz;
  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
    return -1; /* Map the memory to MEMRAM */

  return 0;

}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
  struct pgn_t *pg = mm->fifo_pgn;
  struct pgn_t *pg_prev = NULL;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (pg == NULL){
    *retpgn = -1;
    return -1;
  }
  
  while(pg->pg_next != NULL){
    pg_prev = pg;
    pg = pg->pg_next;
  }

  if(pg_prev == NULL){
    mm->fifo_pgn = NULL;
  }
  else{
    pg_prev->pg_next = NULL;
  }

  *retpgn = pg->pgn;
	free(pg);

  return 0;
}


/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    }
  }

 if(newrg->rg_start == -1) // new region not found
   return -1;

 return 0;
}

//#endif
