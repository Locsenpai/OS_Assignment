/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef MM_TLB
/*
 * Memory physical based TLB Cache
 * TLB cache module tlb/tlbcache.c
 *
 * TLB cache is physically memory phy
 * supports random access 
 * and runs at high speed
 */


#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

#define init_tlbcache(mp,sz,...) init_memphy(mp, sz, (1, ##__VA_ARGS__))

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_read(struct memphy_struct *mp, int pid, int pgnum, BYTE *value)
{
    /* TODO: the identify info is mapped to 
     *      cache line by employing:
     *      direct mapped, associated mapping etc.
     */
   if (mp == NULL || pgnum < 0) return -1;

   int i;

   // Tìm kiếm trong cache
   for (i = 0; i < mp->tlbnum; i++) {
      if(mp->tlbcache) {
         if (mp->tlbcache[i].pgn == pgnum && mp->tlbcache[i].pid == pid) {
            *value = mp->tlbcache[i].val;
            return 0;
         }
      }
   }
   return -1;
}

/*
 *  tlb_cache_write write TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_write(struct memphy_struct *mp, int pid, int pgnum, BYTE value)
{
   /* TODO: the identify info is mapped to 
	*      cache line by employing:
	*      direct mapped, associated mapping etc.
	*/
   if (mp == NULL || pgnum < 0) return -1; 

   if(mp->tlbnum>= mp->maxsz) mp->tlbnum = 0;
   int sz = mp->tlbnum;
   int i=0;

   for(i = 0; i<mp->tlbnum;++i){
      if(mp->tlbcache) {
         if (mp->tlbcache[i].pgn == pgnum && mp->tlbcache[i].pid == pid) {
            sz = i;
            break;
         }
      }
   }

   mp->tlbcache[sz].pid = pid;
   mp->tlbcache[sz].val = value;
   mp->tlbcache[sz].pgn = pgnum;

   if(sz == mp->tlbnum)  
   {
      ++mp->tlbnum;
      return 1;
   }
   return 0;
}

/*
 *  TLBMEMPHY_read natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int TLBMEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{
   if (mp == NULL)
	 return -1;

   /* TLB cached is random access by native */
   *value = mp->storage[addr];

   return 0;
}


/*
 *  TLBMEMPHY_write natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int TLBMEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{
   if (mp == NULL)
	 return -1;

   /* TLB cached is random access by native */
   mp->storage[addr] = data;

   return 0;
}

/*
 *  TLBMEMPHY_format natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 */

int TLBMEMPHY_dump(struct memphy_struct * mp)
{
   /*TODO dump memphy contnt mp->storage 
	*     for tracing the memory content
	*/
   if(mp==NULL) return -1;
#ifdef DUMP_TO_FILE
   FILE* file;
   file = fopen("TLB_status.txt", "w");
   fprintf(file, "TLB Memory content [pos, content] at: %p\n",mp);
   // Display the content of the memory
   for(int i=0;i<mp->tlbnum;++i)
   {
      if(mp->tlbcache[i].val!=-1)
	   fprintf(file, "[%d, %d] ",i, (int) mp->tlbcache[i].val);
   }

   fprintf(file, "\n\n");
   fclose(file);
#else
   printf("TLB Memory content [pos, content] at: %p\n",mp);
   // Display the content of the memory
   for(int i=0;i<mp->tlbnum;++i)
   {
      if(mp->tlbcache[i].val!=-1)
	   printf("[%d, %d] ",i, (int) mp->tlbcache[i].val);
   }

   printf("\n");
#endif
   return 0;
}


/*
 *  Init TLBMEMPHY struct
 */
int init_tlbmemphy(struct memphy_struct *mp, int max_size)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;


   mp->tlbcache = (struct tlbEntry *)malloc(max_size*sizeof(struct tlbEntry));
   mp->tlbnum = 0;
   for(int i=0;i<max_size;++i) mp->tlbcache[i].val=-1;

   mp->rdmflg = 1;

   return 0;
}

//#endif
