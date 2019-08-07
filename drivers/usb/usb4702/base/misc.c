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
#include <asm/io.h>

#include <adv/bdaqdef.h>
#include <adv/hw/plx905x.h>
#include <adv/linux/biokernbase.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

unsigned daq_ai_calc_log_chan_count(unsigned char *chType, unsigned count)
{
   unsigned logCount = count;
   unsigned i;

   for (i = 0; i < count; i += 2) {
      if (chType[i] == Differential) {
         --logCount;
      }
   }

   return logCount;
}

unsigned daq_ai_calc_phy_chan_range(
   unsigned char *chType,  unsigned count,
   unsigned      phyStart, unsigned logCount)
{
   unsigned nextChan;

   BUG_ON(phyStart >= count || logCount > count);

   if (chType[phyStart] == Differential) {
      phyStart &= ~0x1;
   }

   nextChan = phyStart;
   while (logCount--) {
      if (chType[nextChan] == Differential) {
         nextChan += 2;
      } else {
         ++nextChan;
      }
      nextChan %= count;
   }

   return (((nextChan - 1) % count) << 8) | phyStart;
}

unsigned daq_plx905x_calc_sgl_length(unsigned data_len, unsigned sect_len)
{
   unsigned count = 0;

   if (sect_len >= PAGE_SIZE){
      count = data_len / PAGE_SIZE + ((data_len % PAGE_SIZE) != 0);
      if (sect_len % PAGE_SIZE) {
         count += data_len / sect_len;
      }
   } else {
      unsigned i, remains = 0;
      if (PAGE_SIZE % sect_len) {
         for (i = 0; i < data_len / PAGE_SIZE; i++) {
            count   += remains != 0;
            count   += (PAGE_SIZE - remains) / sect_len;
            remains  = (PAGE_SIZE - remains) % sect_len ;
            count   += remains != 0;
            remains  = sect_len - remains;
         }
      } else {
         count = (data_len / PAGE_SIZE) * (PAGE_SIZE / sect_len);
      }

      count += (remains != 0);
      data_len %= PAGE_SIZE;
      if (data_len > remains) {
         data_len -= remains;
         remains   = data_len % sect_len;
         count    += data_len / sect_len + (remains != 0);
      }
   }

   return count * sizeof(PLX_SGL_ENTRY);
}

int daq_plx905x_alloc_sgl_mem(unsigned sgl_len, daq_plx_sgl_t *sgl_mem)
{
   unsigned long *pages;
   int           nr_pages;
   int           i;

   BUG_ON(!sgl_mem);

   if (!sgl_len || sgl_mem->pages){
      return -EINVAL;
   }

   memset(sgl_mem, 0, sizeof(*sgl_mem));

   nr_pages = (sgl_len + ~PAGE_MASK) >> PAGE_SHIFT;
   pages = kmalloc(nr_pages * sizeof(unsigned long), GFP_KERNEL);
   if (pages == NULL){
      return -ENOMEM;
   }

#ifndef GFP_DMA32
#  define GFP_DMA32 0u
#endif
   for (i = 0; i < nr_pages; ++i){
      pages[i] = __get_free_page(GFP_KERNEL | GFP_DMA32);
      if (pages[i] == 0){
         break;
      }
   }

   if (i == nr_pages){
      sgl_mem->nr_pages = nr_pages;
      sgl_mem->pages    = pages;
      sgl_mem->start    = (PLX_SGL_ENTRY*)pages[0];
      sgl_mem->count    = sgl_len / sizeof(PLX_SGL_ENTRY);
      sgl_mem->startPA  = virt_to_phys((void*)pages[0]);
      return 0;
   }

   while (--i >= 0) {
      free_page(pages[i]);
   }

   kfree(pages);
   return -ENOMEM;
}

void daq_plx905x_free_sgl_mem(daq_plx_sgl_t *sgl_mem)
{
   int i;

   BUG_ON(!sgl_mem || (sgl_mem->nr_pages && !sgl_mem->pages));

   if (sgl_mem->nr_pages) {
      for (i = 0; i < sgl_mem->nr_pages; ++i){
         free_page(sgl_mem->pages[i]);
      }

      kfree(sgl_mem->pages);
      memset(sgl_mem, 0, sizeof(*sgl_mem));
   }
}

static
PLX_SGL_ENTRY * daq_plx905x_alloc_entry(daq_plx_sgl_t *sgl_mem, unsigned *entry_offset)
{
   PLX_SGL_ENTRY *entry = NULL;
   unsigned long next_entry;
   unsigned      page_index;
   unsigned      page_offset;


   if (*entry_offset < (sgl_mem->nr_pages << PAGE_SHIFT)) {
      page_index  = *entry_offset >> PAGE_SHIFT;
      page_offset = *entry_offset &  ~PAGE_MASK;
      entry       = (PLX_SGL_ENTRY *)(sgl_mem->pages[page_index] + page_offset);

      *entry_offset += sizeof(PLX_SGL_ENTRY);
      page_index    = *entry_offset >> PAGE_SHIFT;
      page_offset   = *entry_offset &  ~PAGE_MASK;

      if (page_index < sgl_mem->nr_pages) {
         next_entry = sgl_mem->pages[page_index] +  page_offset;
      } else {
         next_entry = sgl_mem->pages[0];
      }

      *(__u32*)&entry->DescPtr = (__u32)virt_to_phys((void*)next_entry);
   }

   return entry;
}

int daq_plx905x_build_sgl(
   daq_umem_t *data_mem, unsigned data_len,    unsigned  sect_len,
   unsigned    dev_addr, unsigned from_device, daq_plx_sgl_t *sgl_mem)
{
   struct page    **data_pages = data_mem->pages;
   PLX_SGL_ENTRY  *entry       = NULL;
   unsigned       entry_offset = 0;
   unsigned       bp           = sect_len;
   unsigned       desc_flag;
   unsigned       curLen;
   dma_addr_t     curPA;
   
#ifdef PLX_DMA_ADDR_64
#   define DUMP_SGL_ENTRY(entry)   \
       daq_trace((KERN_INFO "entry(%p, %p): 0x%x:0x%x, %d, 0x%x\n", \
         entry, (void*)virt_to_phys(entry), entry->PciAddrHigh, entry->PciAddrLow, entry->DataLength, *(__u32*)&entry->DescPtr));
#else
#   define DUMP_SGL_ENTRY(entry)   \
      daq_trace((KERN_INFO "entry(%p, %p): 0:0x%x, %d, 0x%x\n", \
         entry, (void*)virt_to_phys(entry), entry->PciAddrLow, entry->DataLength, *(__u32*)&entry->DescPtr));
#endif

   BUG_ON(!sgl_mem || !sgl_mem->nr_pages || !sgl_mem->pages);
   BUG_ON(!data_mem || !data_mem->nr_pages || !data_mem->pages);

   if (from_device){
      desc_flag = PLX_DESC_IN_PCI | (PLX_DESC_DIR_FROM_DEV << 3);
   } else {
      desc_flag = PLX_DESC_IN_PCI | (PLX_DESC_DIR_TO_DEV << 3);
   }

   while (data_len) {
      curPA     = page_to_phys(*data_pages++);
      curLen    = min((unsigned)PAGE_SIZE, data_len);
      data_len -= curLen;

      while (curLen) {
         entry = daq_plx905x_alloc_entry(sgl_mem, &entry_offset);
         if (!entry) {
            return -EINVAL;
         }

         *(__u32*)&entry->DescPtr |= desc_flag;
         entry->LocAddress = dev_addr;
         PLX_SET_PCI_ADDR(entry, curPA);

         if (curLen >= bp) {
            entry->DescPtr.TermCountInt = 1;
            entry->DataLength = bp;
            curPA += bp;
            bp     = sect_len;
         } else {
            entry->DataLength  = curLen;
            bp                -= curLen;
         }

         curLen -= entry->DataLength;
         DUMP_SGL_ENTRY(entry);
      }
   }

   entry->DescPtr.TermCountInt = 1;
   entry->DescPtr.Address      = PLX_DESC_PTR_ADDR(sgl_mem->startPA);
   DUMP_SGL_ENTRY(entry); 

   sgl_mem->end = entry;

   return 0;
}

static unsigned char __dummy_buffer[128];
int daq_plx9054_flush_fifo(unsigned bridge_base, int dma_chan)
{
   AdxIoOutD(bridge_base, BR_PLX_INTCSR, 0);

   {
      PLX_DMA_MODE dmamode = {0};
      dmamode.LocBusWidth   = 1;
      dmamode.IntlWaitState = 3;
      dmamode.LocBurstEn    = 1;
      dmamode.LocAddrMode   = 1;
      AdxIoOutD(bridge_base, dma_chan ? BR_PLX_DMAMODE1 : BR_PLX_DMAMODE0, dmamode.Value);
   }

   AdxIoOutD(bridge_base, dma_chan ? BR_PLX_DMAPADR1 : BR_PLX_DMAPADR0, virt_to_phys(__dummy_buffer));

   AdxIoOutD(bridge_base, dma_chan ? BR_PLX_DMALADR1 : BR_PLX_DMALADR0, 0);

   AdxIoOutD(bridge_base, dma_chan ? BR_PLX_DMASIZ1 :BR_PLX_DMASIZ0, 128);

   {
      unsigned reg_offset = dma_chan ? BR_PLX_DMACSR1 : BR_PLX_DMACSR0;
      unsigned try_count = 0; 
      PLX_DMA_CSR dmacsr  = {0};
      dmacsr.Enable   = 1;
      dmacsr.ClearInt = 1;
      AdxIoOutB(bridge_base, reg_offset, dmacsr.Value);

      dmacsr.Start = 1;
      AdxIoOutW(bridge_base, reg_offset, dmacsr.Value);

      dmacsr.Value = AdxIoInB(bridge_base, reg_offset);
      while (!dmacsr.Done && try_count < 1000) {
         dmacsr.Value = AdxIoInB(bridge_base, reg_offset);
         ++try_count;
      }
      if (try_count >= 1000)
      {
         printk("Warning, clear FIFO out of time!\n");
      }
   }

   return 0;
}

unsigned daq_ver_string_to_integer(char * ver_str)
{
   unsigned ver = 0;
   unsigned   x = 0;

   BUG_ON(!ver_str);

   do {
      for (x = 0; *ver_str; ++ver_str) {
         if (*ver_str == '.') {
            ++ver_str;
            break;
         }
         x = x * 10 + (*ver_str - '0');
      }
      ver = (ver << 8) | (x & 0xff);
   } while (*ver_str);

   return ver;
}

EXPORT_SYMBOL_GPL(daq_ai_calc_log_chan_count);
EXPORT_SYMBOL_GPL(daq_ai_calc_phy_chan_range);
EXPORT_SYMBOL_GPL(daq_plx905x_calc_sgl_length);
EXPORT_SYMBOL_GPL(daq_plx905x_alloc_sgl_mem);
EXPORT_SYMBOL_GPL(daq_plx905x_free_sgl_mem);
EXPORT_SYMBOL_GPL(daq_plx905x_build_sgl);
EXPORT_SYMBOL_GPL(daq_plx9054_flush_fifo);
EXPORT_SYMBOL_GPL(daq_ver_string_to_integer);
