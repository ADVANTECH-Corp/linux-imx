/*
 * counter.c
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */
#include "kdriver.h"


static
void CntrCheckFmState(struct _CNTR_STATE *cntrState, __u32 timeStamp)
{
   __u32 cntrDelta,timeDelta;

   cntrState->CurrValue = __be32_to_cpu(cntrState->CurrValue);
   //daq_trace(("CntrCheckFmStateByAdaptivePeriod: CurValue(After Swap)=0x%x\n", cntrState->CurrValue));
   cntrDelta = (cntrState->CurrValue - cntrState->PrevVal);
   //daq_trace(("cntrDelta = 0x%x\n", cntrDelta));
   timeDelta = timeStamp - cntrState->PrevTime;
   if (cntrState->CheckPeriod <= timeDelta){
      if (cntrState->AdaptivePeriod){
         if (cntrDelta >= 10 * CNTR_VAL_THRESHOLD_BASE){
            cntrState->CheckPeriod = CNTR_CHK_PERIOD_NS;
            //daq_trace(("reduce check period:%u, delta:%u\n", cntrState->CheckPeriod, cntrDelta ));
         } else if (cntrDelta < CNTR_VAL_THRESHOLD_BASE){
            cntrState->CheckPeriod <<= 3; // enlarge the check period 8 times.
            if (cntrState->CheckPeriod > CNTR_CHK_PERIOD_MAX_NS){
               cntrState->CheckPeriod = CNTR_CHK_PERIOD_MAX_NS;
            }
            //daq_trace(("enlarge check period: %u, delta:%u\n", cntrState->CheckPeriod, cntrDelta ));
         }
      }

      ++cntrState->Tail;
      cntrState->Tail &= CNTR_RBUF_POSMASK;

      if ( cntrState->Tail == cntrState->Head )
      {
         cntrState->SummedValue -= cntrState->CntrDelta[cntrState->Head];
         cntrState->TotalTime   -= cntrState->TimeDelta[cntrState->Head];
         cntrState->Head        = ( cntrState->Tail + 1 ) & CNTR_RBUF_POSMASK;
      }

      // read the value
      cntrState->CntrDelta[cntrState->Tail] = cntrDelta;
      cntrState->SummedValue               += cntrDelta;
      cntrState->TimeDelta[cntrState->Tail] = timeDelta;
      cntrState->TotalTime                 += timeDelta;
      cntrState->PrevVal                    = cntrState->CurrValue;
      cntrState->PrevTime                   = timeStamp;
   }
}

//-------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------
void daq_cntr_update_state_work_func(struct work_struct *work)
{
   daq_device_t        *daq_dev = container_of(delayed_work_ptr(work), daq_device_t, cntr_fm_work);
   DEVICE_SHARED       *shared  = &daq_dev->shared;

   struct timespec     tm_now;
   __s64               tm_now_ns;
   __u32               i, active= shared->CntrChkTimerOwner;

   getnstimeofday(&tm_now);

   for (i = 0; i < CNTR_CHL_COUNT; ++i){
      if (active & (1 << i)) {
         tm_now_ns = timespec_to_ns(&tm_now);
         if (daq_usb_cntr_read_event_count_aysn(daq_dev, i, &shared->CntrState[0].CurrValue) < 0)
            break;
         CntrCheckFmState(&shared->CntrState[i], tm_now_ns);
      }
   }

   if (daq_dev->shared.CntrChkTimerOwner){
      schedule_delayed_work(delayed_work_ptr(work), msecs_to_jiffies(CNTR_CHK_PERIOD_NS / 1000000L));
   }
}

static
int daq_cntr_start_event_count(daq_device_t *daq_dev, __u32 start, __u32 count)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   CNTR_STATE    *cntr;
   unsigned long flags;
   int           ret = 0;

   cntr = &shared->CntrState[start];
   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   if (cntr->Operation == CNTR_IDLE) {
      cntr->Operation = InstantEventCount;
   } else {
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
      return -EBUSY;
   }
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   ret = daq_usb_cntr_start_event_count(daq_dev, start);
   if (ret < 0) {
      cntr->Operation = CNTR_IDLE;
   }

   return ret < 0 ? ret : 0;
}

static
int daq_cntr_start_freq_measure(daq_device_t *daq_dev, __u32 start, __u32 count)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   CNTR_STATE    *cntr;
   unsigned long flags;
   struct timespec tm_now;
   int           ret = 0;

   cntr = &shared->CntrState[start];
   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   if (cntr->Operation == CNTR_IDLE) {
      cntr->Operation = InstantFreqMeter;
   } else {
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
      return -EBUSY;
   }
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   memset(&shared->CntrState[start], 0, sizeof(CNTR_STATE));

   if (shared->CntrConfig.FmPeroid[start]){
      shared->CntrState[start].AdaptivePeriod = false;
      shared->CntrState[start].CheckPeriod = max(CNTR_CHK_PERIOD_MS, (int)(shared->CntrConfig.FmPeroid[start] / CNTR_RBUF_DEPTH));
   } else {
      shared->CntrState[start].AdaptivePeriod = true;
      shared->CntrState[start].CheckPeriod = CNTR_CHK_PERIOD_MS; // default check period for self-adaptive measurement.
   }
   shared->CntrState[start].CheckPeriod *= 1000 * 10; // convert from ms to 100ns unit

   ret = daq_usb_cntr_start_event_count(daq_dev, start);
   if (ret < 0) {
      cntr->Operation = CNTR_IDLE;
      return ret;
   }

   tm_now = current_kernel_time();
   shared->CntrState[start].PrevTime = timespec_to_ns(&tm_now);
   shared->CntrChkTimerOwner |= 0x1 << start;
   schedule_delayed_work(&daq_dev->cntr_fm_work, 1);

   return ret < 0 ? ret : 0;
}

void daq_cntr_reset(daq_device_t *daq_dev, __u32 start, __u32 count)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   unsigned long flags;

   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   while (count--) {
      if (shared->CntrState[start].Operation != CNTR_IDLE) {
         spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
         daq_usb_cntr_reset(daq_dev, start);
         spin_lock_irqsave(&daq_dev->dev_lock, flags);
      }

      shared->CntrChkTimerOwner &= ~(0x1 << start);
      shared->CntrState[start].Operation = CNTR_IDLE;
      ++start;
      start %= CNTR_CHL_COUNT;
   }
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
}

int daq_ioctl_cntr_set_param(daq_device_t *daq_dev, unsigned long arg)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   CNTR_SET_CFG  cntr;
   void          *dataPtr;
   __u32         valLen;

   if (unlikely(copy_from_user(&cntr, (void *)arg, sizeof(cntr)))){
      return -EFAULT;
   }

   if (cntr.Start >= CNTR_CHL_COUNT) {
      return -EINVAL;
   }

   if (cntr.Count > CNTR_CHL_COUNT - cntr.Start) {
      cntr.Count = CNTR_CHL_COUNT - cntr.Start;
   }

   switch (cntr.PropID)
   {
   case CFG_FmCollectionPeriodOfCounters:
      dataPtr = &shared->CntrConfig.FmPeroid[cntr.Start];
      valLen  = sizeof(__u32)*cntr.Count;
      break;
   default:
      return -EINVAL;
   }

   if (unlikely(copy_from_user(dataPtr, cntr.Value, valLen))){
      return -EFAULT;
   }

   return 0;
}

int daq_ioctl_cntr_start(daq_device_t *daq_dev, unsigned long arg)
{
   CNTR_START cntr;

   if (unlikely(copy_from_user(&cntr, (void *)arg, sizeof(cntr)))){
      return -EFAULT;
   }

   if (cntr.Start >= CNTR_CHL_COUNT || cntr.Count > CNTR_CHL_COUNT){
      return -EINVAL;
   }

   switch(cntr.Operation)
   {
   case InstantEventCount:
      return daq_cntr_start_event_count(daq_dev, cntr.Start, cntr.Count);
   case InstantFreqMeter:
      return daq_cntr_start_freq_measure(daq_dev, cntr.Start, cntr.Count);
   default:
      return -ENOSYS;
   }
}

int daq_ioctl_cntr_read(daq_device_t *daq_dev, unsigned long arg)
{
   CNTR_READ  cntr;
   __u32      start, count, outLen;
   int        ret = 0;
   CNTR_VALUE ecVal[CNTR_CHL_COUNT];

   if (unlikely(copy_from_user(&cntr, (void *)arg, sizeof(cntr)))){
      return -EFAULT;
   }

   if (cntr.Start >= CNTR_CHL_COUNT || cntr.Count > CNTR_CHL_COUNT){
      return -EINVAL;
   }

   // save the input parameter before we write the output value into the buffer.
   start = cntr.Start;
   count = cntr.Count;

   outLen = sizeof(CNTR_VALUE) * cntr.Count;

   while (cntr.Count--) {
      ret = daq_usb_cntr_read_event_count(daq_dev, cntr.Start, &ecVal[cntr.Count].Value, &ecVal[cntr.Count].Overflow);

      ++cntr.Start;
      cntr.Start %= CNTR_CHL_COUNT;
   }

   if (ret >= 0){
      ret = copy_to_user(cntr.Value, ecVal, outLen);
   }

   return ret;
}

int daq_ioctl_cntr_reset(daq_device_t *daq_dev, unsigned long arg)
{
   CNTR_RESET cntr;

   if (unlikely(copy_from_user(&cntr, (void *)arg, sizeof(cntr)))){
      return -EFAULT;
   }

   if (cntr.Start >= CNTR_CHL_COUNT || cntr.Count > CNTR_CHL_COUNT){
      return -EINVAL;
   }

   daq_cntr_reset(daq_dev, cntr.Start, cntr.Count);

   return 0;
}
