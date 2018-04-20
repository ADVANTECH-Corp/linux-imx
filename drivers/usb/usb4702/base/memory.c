/*
 * misc.c
 *
 *  Created on: 2011-9-8
 *      Author: rocky
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>

#include <adv/linux/biokernbase.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

int daq_umem_get_pages(unsigned long uaddr, unsigned count, int write, daq_umem_t *mi)
{
   struct page **pages;
   int         nr_pages;
   int         ret;

   BUG_ON(!mi);

   if ((uaddr & ~PAGE_MASK) || !count || (uaddr + count < uaddr)){
      return -EINVAL;
   }
   memset(mi, 0, sizeof(*mi));

   nr_pages = (count + ~PAGE_MASK) >> PAGE_SHIFT;
   pages = kmalloc(nr_pages * sizeof(void *), GFP_KERNEL);
   if (pages == NULL){
      return -ENOMEM;
   }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
   down_read(&current->mm->mmap_sem);
   ret = get_user_pages(current, current->mm, uaddr, nr_pages, write, 0, pages, NULL);
   up_read(&current->mm->mmap_sem);
#else
   ret = get_user_pages_fast(uaddr, nr_pages, write, pages);
#endif

   if (ret == nr_pages){
      mi->nr_pages = nr_pages;
      mi->pages    = pages;
      return 0;
   }

   while (--ret >= 0) {
      page_cache_release(pages[ret]);
   }

   kfree(pages);

   return -ENOMEM;
}

int daq_umem_map_pages(daq_umem_t *mi)
{
   BUG_ON(!mi);

   if (unlikely(!mi->nr_pages || !mi->pages)){
      return -EINVAL;
   }

   if (mi->kaddr){
      return 0;
   }

   mi->kaddr = vmap(mi->pages, mi->nr_pages, VM_MAP, PAGE_KERNEL);
   return mi->kaddr != NULL ? 0 : -ENOMEM;
}

int daq_umem_map(unsigned long uaddr, unsigned count, int write, daq_umem_t *mi)
{
   int ret = daq_umem_get_pages(uaddr, count, write, mi);
   if (likely(!ret)){
      ret = daq_umem_map_pages(mi);
   }

   return ret;
}

void daq_umem_unmap(daq_umem_t *mi)
{
   unsigned i;

   BUG_ON(mi->nr_pages && (!mi->pages));

   if (mi->nr_pages){

      if (mi->kaddr){
         vunmap(mi->kaddr);
      }

      for (i = 0; i < mi->nr_pages; ++i){
         if (!PageReserved(mi->pages[i])){
            SetPageDirty(mi->pages[i]);
         }
         page_cache_release(mi->pages[i]);
      }
      kfree(mi->pages);
      memset(mi, 0, sizeof(*mi));
   }
}

EXPORT_SYMBOL_GPL(daq_umem_map);
EXPORT_SYMBOL_GPL(daq_umem_get_pages);
EXPORT_SYMBOL_GPL(daq_umem_map_pages);
EXPORT_SYMBOL_GPL(daq_umem_unmap);
