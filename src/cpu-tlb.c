/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
/*
 * CPU TLB
 * TLB module cpu/cpu-tlb.c
 */
 
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#ifdef CPU_TLB

int tlb_change_all_page_tables_of(struct pcb_t *proc,  struct memphy_struct * mp)
{
  /* TODO update all page table directory info 
   *      in flush or wipe TLB (if needed)
  */
  tlb_flush_tlb_of(proc, mp);
  return 0;
}

int tlb_flush_tlb_of(struct pcb_t *proc, struct memphy_struct * mp)
{
  /* TODO flush tlb cached*/
  if (mp == NULL) return -1;

  for (int i = 0; i < mp->tlbnum; i++) 
  {
    if (mp->tlbcache[i].pid == proc->pid)
    {
      //mp->tlbcache[i] = NULL;
      mp->tlbcache[i].pid = -1;
      mp->tlbcache[i].pgn = -1;
      mp->tlbcache[i].val = -1;
    }
  }
  mp->tlbnum = 0;

  return 0;
}

/*tlballoc - CPU TLB-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr, val;

  /* By default using vmaid = 0 */
  val = __alloc(proc, 0, reg_index, size, &addr);
  /* TODO update TLB CACHED frame num of the new allocated page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  if (val == 0)
  {
    uint32_t num_pages = DIV_ROUND_UP(size, PAGE_SIZE);
    uint32_t page_start = addr / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) 
    {
        tlb_cache_write(proc->tlb, proc->pid, page_start + i, -1);
    }
  }
  return val;
}

/*pgfree - CPU TLB-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlbfree_data(struct pcb_t *proc, uint32_t reg_index)
{

  struct vm_rg_struct *region = get_symrg_byid(proc->mm, reg_index);
  if (region == NULL) 
  {
    return 1; // Return error if region not found
  }

  uint32_t start_page = region->rg_start / PAGE_SIZE;
  uint32_t end_page = (region->rg_end + PAGE_SIZE - 1) / PAGE_SIZE;

  for (uint32_t page = start_page; page < end_page; page++) 
  {
    tlb_cache_write(proc->tlb, proc->pid, page, 0);
  }

  int val = __free(proc, 0, reg_index);
  if (val != 0) return 1;
  /* TODO update TLB CACHED frame num of freed page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/


  return 0;
}


/*tlbread - CPU TLB-based read a region memory
 *@proc: Process executing the instruction
 *@source: index of source register
 *@offset: source address = [source] + [offset]
 *@destination: destination storage
 */
int tlbread(struct pcb_t * proc, uint32_t source,
            uint32_t offset, 	uint32_t destination) 
{
  BYTE data, frmnum = -1;
	
  /* TODO retrieve TLB CACHED frame num of accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  /* frmnum is return value of tlb_cache_read/write value*/
	int pgn = PAGING_PGN((proc->regs[source] + offset));
  frmnum = tlb_cache_read(proc->tlb, proc->pid, pgn, &data);
  usleep(100);
#ifdef IODUMP
  if (frmnum >= 0)
  {
    printf("TLB hit at read region=%d offset=%d\n", 
	         source, offset);
  }
  else 
    printf("TLB miss at read region=%d offset=%d\n", 
	         source, offset);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  TLBMEMPHY_dump(proc->tlb);
#endif
  if(frmnum >= 0) return 0;

  int val = __read(proc, 0, source, offset, &data);

  destination = (uint32_t) data;

  if (val == 0)
  {
    tlb_cache_write(proc->tlb, proc->pid, pgn, data);
    proc->regs[destination] = data;
  }

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

  return val;
}

/*tlbwrite - CPU TLB-based write a region memory
 *@proc: Process executing the instruction
 *@data: data to be wrttien into memory
 *@destination: index of destination register
 *@offset: destination address = [destination] + [offset]
 */
int tlbwrite(struct pcb_t * proc, BYTE data, uint32_t destination, uint32_t offset)
{
  int val;
  int frmnum = -1;

  /* TODO retrieve TLB CACHED frame num of accessing page(s))*/
  /* by using tlb_cache_read()/tlb_cache_write()
  frmnum is return value of tlb_cache_read/write value*/
  frmnum = tlb_cache_read(proc->tlb, proc->pid, destination + offset, &data);
  val = __write(proc, 0, destination, offset, data);

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  tlb_cache_write(proc->tlb, proc->pid, destination + offset, data);
  usleep(100);
#ifdef IODUMP
  if (frmnum >= 0)
    printf("TLB hit at write region=%d offset=%d value=%d\n",
	          destination, offset, data);
	else
    printf("TLB miss at write region=%d offset=%d value=%d\n",
            destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  TLBMEMPHY_dump(proc->tlb);
#endif
  return val;
}


#endif
