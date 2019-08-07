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
#include <linux/pci.h>

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
      put_page(pages[ret]);
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
         put_page(mi->pages[i]);
      }
      kfree(mi->pages);
      memset(mi, 0, sizeof(*mi));
   }
}

int daq_umem_alloc(unsigned size, daq_umem_t *mi, int dma32)
{
   struct page **pages;
   void         *kaddr;
   int           nr_pages;
   int           page_flag;
   int           i;

   BUG_ON(!mi);

   if (!size || mi->pages){
      return -EINVAL;
   }

   memset(mi, 0, sizeof(*mi));

   nr_pages = (size + ~PAGE_MASK) >> PAGE_SHIFT;
   pages = kmalloc(nr_pages * sizeof(void *), GFP_KERNEL);
   if (pages == NULL){
      return -ENOMEM;
   }

#ifndef GFP_DMA32
#  define GFP_DMA32 0u
#endif

   page_flag = GFP_KERNEL;
   if (dma32) {
      page_flag |= GFP_DMA32;
   }

   for (i = 0; i < nr_pages; ++i){
      pages[i] = alloc_page(page_flag);
      if (pages[i] == NULL){
         break;
      }
   }

   if (i == nr_pages){
      kaddr = vmap(pages, nr_pages, VM_MAP, PAGE_KERNEL);
      if (kaddr != NULL) {
         mi->nr_pages = nr_pages;
         mi->pages    = pages;
         mi->kaddr    = kaddr;
         return 0;
      }
   }

   while (--i >= 0) {
      __free_page(pages[i]);
   }

   kfree(pages);
   return -ENOMEM;
}

void daq_umem_free(daq_umem_t *mi)
{
   unsigned i;

   BUG_ON(mi->nr_pages && (!mi->pages));

   if (mi->nr_pages){

      if (mi->kaddr != NULL){
         vunmap(mi->kaddr);
      }

      for (i = 0; i < mi->nr_pages; ++i){
         __free_page(mi->pages[i]);
      }
      kfree(mi->pages);
      memset(mi, 0, sizeof(*mi));
   }
}

int daq_dmem_alloc(struct device *dev, unsigned size, daq_dmem_t *mem)
{
   BUG_ON(!mem);

   mem->size = size;
   mem->kaddr= dma_alloc_coherent(dev, size, &mem->daddr, GFP_KERNEL);
   return mem->kaddr == NULL ? -ENOMEM : 0;
}

void daq_dmem_free(struct device *dev, daq_dmem_t *mem)
{
   BUG_ON(!mem);
   if (mem->kaddr) {
      dma_free_coherent(dev, mem->size, mem->kaddr, mem->daddr);
   }
   mem->kaddr = NULL;
}

EXPORT_SYMBOL_GPL(daq_umem_map);
EXPORT_SYMBOL_GPL(daq_umem_get_pages);
EXPORT_SYMBOL_GPL(daq_umem_map_pages);
EXPORT_SYMBOL_GPL(daq_umem_unmap);
EXPORT_SYMBOL_GPL(daq_umem_alloc);
EXPORT_SYMBOL_GPL(daq_umem_free);
EXPORT_SYMBOL_GPL(daq_dmem_alloc);
EXPORT_SYMBOL_GPL(daq_dmem_free);