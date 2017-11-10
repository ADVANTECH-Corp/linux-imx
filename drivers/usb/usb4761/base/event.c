/*
 * event.c
 *
 *  Created on: 2011-9-27
 *      Author: rocky
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <adv/linux/biokernbase.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#define STACK_BUF_THRESHOLD 6

/************************************************************************
 * Private Types
 ************************************************************************/
struct daq_event{
   struct kref      kref;
   struct list_head wait_list;
   int              signal_state;
};

typedef struct daq_wait_item{
   struct list_head    list_entry;
   struct daq_wait_ctx *wait_ctx;
}daq_wait_item_t;

typedef struct daq_wait_ctx{
   void            *wait_task;
   daq_wait_item_t *wait_items;
   int             item_count;
   int             wait_result;
}daq_wait_ctx_t;

/************************************************************************
 * Private Data
 ************************************************************************/
static DEFINE_SPINLOCK(s_event_dispatch_lock);

/************************************************************************
 * Private Method
 ************************************************************************/
static
int daq_event_probe(int count, daq_event_t *event[])
{
   unsigned long flags;
   int i;

   spin_lock_irqsave(&s_event_dispatch_lock, flags);
   for (i = 0; i < count; ++i){
      if (event[i]->signal_state > 0){
         event[i]->signal_state = 0;
         break;
      }
   }
   spin_unlock_irqrestore(&s_event_dispatch_lock, flags);

   return i < count ? i : -1;
}

static
void daq_event_release(struct kref *kref)
{
   daq_event_t *event = container_of(kref, daq_event_t, kref);
   kfree(event);
}

/************************************************************************
 * KLib Public Methods
 *************************************************************************/
daq_event_t * daq_event_create()
{
   daq_event_t *event = kmalloc(sizeof(*event), GFP_KERNEL);

   if (event != NULL){
      kref_init(&event->kref);
      INIT_LIST_HEAD(&event->wait_list);
      event->signal_state = 0;
   }

   return event;
}

void daq_event_close(daq_event_t *event)
{
   if (event != NULL){
      kref_put(&event->kref, daq_event_release);
   }
}

void daq_event_set(daq_event_t *event)
{
   daq_wait_item_t *curr, *next;
   daq_wait_ctx_t  *ctx;
   unsigned long   flags;

   if (event == NULL){
      return;
   }

   spin_lock_irqsave(&s_event_dispatch_lock, flags);
   if (event->signal_state == 0){
      event->signal_state = 1;
      list_for_each_entry_safe(curr, next, &event->wait_list, list_entry){
         list_del_init(&curr->list_entry);

         ctx = curr->wait_ctx;
         if (ctx->wait_result == -1) {
            event->signal_state = 0;
            ctx->wait_result = curr - ctx->wait_items;
            wake_up_process(ctx->wait_task);
         }
      }
   }
   spin_unlock_irqrestore(&s_event_dispatch_lock, flags);
}

void daq_event_reset(daq_event_t *event)
{
   unsigned long flags;

   if (event != NULL){
      spin_lock_irqsave(&s_event_dispatch_lock, flags);
      event->signal_state = 0;
      spin_unlock_irqrestore(&s_event_dispatch_lock, flags);
   }
}

int daq_event_wait(
   unsigned    count,
   daq_event_t *events[],
   int         waitAll,
   long        timeout)
{
   daq_wait_ctx_t  ctx;
   daq_wait_item_t stack_buf[STACK_BUF_THRESHOLD];
   unsigned long   flags;
   int i;

   if (count > MAX_WAIT_OBJECTS){
      return -EINVAL;
   }

   for (i = 0; i < count; ++i){
      if (events[i] == NULL){
         return -EINVAL;
      }
   }

   if (!timeout){
      return daq_event_probe(count, events);
   }

   ctx.wait_task   = current;
   ctx.wait_result = -1;
   ctx.item_count  = count;
   if (count <= STACK_BUF_THRESHOLD){
      ctx.wait_items = stack_buf;
   } else {
      ctx.wait_items = kmalloc(sizeof(daq_wait_item_t) * count, GFP_KERNEL);
      if (ctx.wait_items == NULL){
         return -ENOMEM;
      }
   }

   spin_lock_irqsave(&s_event_dispatch_lock, flags);
   for (i = 0; i < count; ++i){
      if (events[i]->signal_state > 0) {
          events[i]->signal_state = 0;
          ctx.wait_result = i;
          break;
      }
      kref_get(&events[i]->kref);
      ctx.wait_items[i].wait_ctx = &ctx;
      list_add(&ctx.wait_items[i].list_entry, &events[i]->wait_list);
   }

   if (ctx.wait_result < 0){
      timeout = msecs_to_jiffies(timeout);
      do {
         if (signal_pending(current)){
            ctx.wait_result = -EINTR;
            break;
         }
         set_current_state(TASK_INTERRUPTIBLE);
         spin_unlock_irqrestore(&s_event_dispatch_lock, flags);
         timeout = schedule_timeout(timeout);
         spin_lock_irqsave(&s_event_dispatch_lock, flags);
      } while (ctx.wait_result < 0 && timeout);
      __set_current_state(TASK_RUNNING);
   }

   while (--i >= 0){
      list_del(&ctx.wait_items[i].list_entry);
      kref_put(&events[i]->kref, daq_event_release);
   }
   spin_unlock_irqrestore(&s_event_dispatch_lock, flags);

   if (count > STACK_BUF_THRESHOLD){
      kfree(ctx.wait_items);
   }

   return ctx.wait_result;
}

EXPORT_SYMBOL_GPL(daq_event_create);
EXPORT_SYMBOL_GPL(daq_event_close);
EXPORT_SYMBOL_GPL(daq_event_set);
EXPORT_SYMBOL_GPL(daq_event_reset);
EXPORT_SYMBOL_GPL(daq_event_wait);

