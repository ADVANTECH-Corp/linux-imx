/*
 * ai.c
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */


#include "kdriver.h"
#include "hw.h"

static inline
void daq_ai_set_channel_sctype(daq_device_t *daq_dev, __u8 type)
{
   __u32  i;
   if (type != daq_dev->shared.HwAiChType) {
      for (i = 0; i < AI_CHL_COUNT; ++i) {
         daq_dev->shared.AiChanType[i] = type;
      }
      daq_dev->shared.HwAiChType = type;

      daq_usb_dev_set_board_id_oscsd(daq_dev, daq_dev->shared.BoardId, daq_dev->shared.HwAiChType, daq_dev->shared.Oscillator);
   }
}

static inline
void daq_ai_set_channel_gain(daq_device_t *daq_dev, __u32 phyChan, __u8 gain)
{
   daq_dev->shared.AiChanGain[phyChan] = gain;
   daq_usb_ai_configure_channel(daq_dev, phyChan, daq_dev->shared.HwAiChType, gain);
}

static inline
int32 CalibrateData_4702(int32 rawData, int32 offset, int32 span)
{
   __u32 const shift  = 4;
   __u32 const adjust = 0x800;

   rawData >>= shift;
   rawData += offset - 128;
   rawData += (rawData * (span - 128)) >> 10;
   rawData += adjust;

   return max(min(rawData, AI_DATA_MASK_4702), 0);
}

static inline
int32 CalibrateData_4704(int32 rawData, int32 offset, int32 span)
{
   int32 const shift = 2;
   int32 const adjust = 0x2000;

   rawData >>= shift;
   rawData += offset - 128;
   rawData += (rawData * (span - 128)) >> 10;
   rawData += adjust;

   return max(min(rawData, AI_DATA_MASK_4704), 0);
}

//
// Must hold the lock when call this function.
static inline
void daq_fai_update_status(FAI_CONFIG *cfg, FAI_STATUS *st, unsigned inc_count)
{
   st->BufState = 0;

   if (st->WritePos + inc_count >= st->BufLength) {
      st->BufState   =  DAQ_IN_DATAREADY;
      st->WritePos  += inc_count;
      st->WritePos  %= st->BufLength;
      st->WPRunBack += 1;
   } else {
      if ((st->WritePos % cfg->SectionSize) + inc_count >= cfg->SectionSize) {
         st->BufState = DAQ_IN_DATAREADY;
      }
      st->WritePos += inc_count;
   }

   if (st->WPRunBack) {
      st->BufState |= DAQ_IN_BUF_FULL;
      if (st->WPRunBack > 1 || st->WritePos > st->ReadPos){
         st->BufState |= DAQ_IN_BUF_OVERRUN;
      }
   }
}

static
int daq_fai_xfer_complete_4702(struct urb *urb, void *context)
{
   daq_device_t  *daq_dev = (daq_device_t*)context;
   DEVICE_SHARED *shared  = &daq_dev->shared;
   FAI_STATUS    *faiStatus = &shared->FaiStatus;
   unsigned long flags;
   __u32         buf_state;

   //daq_trace((KERN_INFO "recved data: %d, status: %d\n", urb->actual_length, urb->status));
   if (urb->status){
      schedule_work(&daq_dev->fai_stop_work);
      return DAQ_UR_BREAK;
   }

   if (!urb->actual_length){
      return DAQ_UR_CONTINUE;
   }

   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   if (faiStatus->FnState != DAQ_FN_RUNNING
      || DAQ_IN_MUST_STOP(faiStatus->BufState, faiStatus->AcqMode)) {
      spin_unlock_irqrestore(&daq_dev->fai_lock, flags);
      return DAQ_UR_BREAK;
   } else {
       int16 *src, *dest;
       __u32 dataNum, segTail, segHead;

       dataNum = urb->actual_length / 2;
       src    = (int16*)urb->transfer_buffer;
       dest   = (int16*)daq_dev->fai_buffer.kaddr + faiStatus->WritePos;

       if (!(faiStatus->AcqMode == DAQ_ACQ_INFINITE)) {
          dataNum = min(dataNum, faiStatus->BufLength - faiStatus->WritePos);
          segTail = dataNum;
          segHead = 0;
       } else {
          if (dataNum <= faiStatus->BufLength) {
             segTail = min(dataNum, faiStatus->BufLength - faiStatus->WritePos);
             segHead = dataNum - segTail;
          } else {
             __u32 skipped, wpos;
             skipped = dataNum - faiStatus->BufLength;
             wpos = (faiStatus->WritePos + skipped) % faiStatus->BufLength;
             faiStatus->CurrChan = (faiStatus->CurrChan + skipped) % shared->FaiParam.LogChanCount;
             src += skipped;
             dest = (int16*)daq_dev->fai_buffer.kaddr + wpos;
             segTail = faiStatus->BufLength - wpos;
             segHead = wpos;
          }
       }

       //
       // single-ended
       if (!shared->AiChanType[0]) {
          for (; segTail; --segTail, ++src, ++dest) {
             AI_CALI_INFO cali;
             cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (int16)CalibrateData_4702((*src << 1) - 0x8000, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }

          dest = (int16*)daq_dev->fai_buffer.kaddr;
          for (; segHead; --segHead, ++src, ++dest) {
             AI_CALI_INFO cali;
             cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (int16)CalibrateData_4702((*src << 1) - 0x8000, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }
       } else {
          //
          // differential
          //
          for (; segTail; --segTail, ++src, ++dest) {
             AI_CALI_INFO cali;
             cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (int16)CalibrateData_4702(*src, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }

          dest = (int16*)daq_dev->fai_buffer.kaddr;
          for (; segHead; --segHead, ++src, ++dest) {
             AI_CALI_INFO cali;
             cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (int16)CalibrateData_4702(*src, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }
       }


       daq_fai_update_status(&shared->FaiParam, faiStatus, dataNum);
       buf_state = faiStatus->BufState;
   }
   spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

   if (faiStatus->AcqMode == DAQ_ACQ_INFINITE) {
      schedule_work(&daq_dev->fai_check_work);
   }

   if ((faiStatus->HwFlag & USB_FLAG_FAIOVERRUN) && !shared->IsEvtSignaled[KdxAiCacheOverflow]) {
      shared->IsEvtSignaled[KdxAiCacheOverflow] = 1;
      daq_device_signal_event(daq_dev, KdxAiCacheOverflow);
   }

   if ((buf_state & DAQ_IN_BUF_OVERRUN) && !shared->IsEvtSignaled[KdxAiOverrun]) {
      shared->IsEvtSignaled[KdxAiOverrun] = 1;
      daq_device_signal_event(daq_dev, KdxAiOverrun);
   }

   if ((buf_state & DAQ_IN_DATAREADY) && !shared->IsEvtSignaled[KdxAiDataReady]) {
      shared->IsEvtSignaled[KdxAiDataReady] = 1;
      daq_device_signal_event(daq_dev, KdxAiDataReady);
   }

   if (DAQ_IN_MUST_STOP(buf_state, faiStatus->AcqMode)) {
      schedule_work(&daq_dev->fai_stop_work);
      return DAQ_UR_BREAK;
   }

   return DAQ_UR_CONTINUE;
}

static
int daq_fai_xfer_complete_4704(struct urb *urb, void *context)
{
   daq_device_t  *daq_dev = (daq_device_t*)context;
   DEVICE_SHARED *shared  = &daq_dev->shared;
   FAI_STATUS    *faiStatus = &shared->FaiStatus;
   unsigned long flags;
   __u32         buf_state;

   //daq_trace((KERN_INFO "recved data: %d, status: %d\n", urb->actual_length, urb->status));
   if (urb->status){
      schedule_work(&daq_dev->fai_stop_work);
      return DAQ_UR_BREAK;
   }

   if (!urb->actual_length){
      return DAQ_UR_CONTINUE;
   }

   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   if (faiStatus->FnState != DAQ_FN_RUNNING
      || DAQ_IN_MUST_STOP(faiStatus->BufState, faiStatus->AcqMode)) {
      spin_unlock_irqrestore(&daq_dev->fai_lock, flags);
      return DAQ_UR_BREAK;
   } else {
       int16 *src, *dest;
       __u32 dataNum, segTail, segHead;

       dataNum = urb->actual_length / 2;
       src    = (int16*)urb->transfer_buffer;
       dest   = (int16*)daq_dev->fai_buffer.kaddr + faiStatus->WritePos;

       if (!(faiStatus->AcqMode == DAQ_ACQ_INFINITE)) {
          dataNum = min(dataNum, faiStatus->BufLength - faiStatus->WritePos);
          segTail = dataNum;
          segHead = 0;
       } else {
          if (dataNum <= faiStatus->BufLength) {
             segTail = min(dataNum, faiStatus->BufLength - faiStatus->WritePos);
             segHead = dataNum - segTail;
          } else {
             __u32 skipped, wpos;
             skipped = dataNum - faiStatus->BufLength;
             wpos = (faiStatus->WritePos + skipped) % faiStatus->BufLength;
             faiStatus->CurrChan = (faiStatus->CurrChan + skipped) % shared->FaiParam.LogChanCount;
             src += skipped;
             dest = (int16*)daq_dev->fai_buffer.kaddr + wpos;
             segTail = faiStatus->BufLength - wpos;
             segHead = wpos;
          }
       }
       //
       // single-ended
       if (!shared->AiChanType[0]) {
          for (; segTail; --segTail, ++src, ++dest) {
             AI_CALI_INFO cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (int16)CalibrateData_4704((*src << 1) - 0x8000, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }

          dest = (int16*)daq_dev->fai_buffer.kaddr;
          for (; segHead; --segHead, ++src, ++dest) {
             AI_CALI_INFO cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (int16)CalibrateData_4704((*src << 1) - 0x8000, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }
       } else {
          //
          // differential
          //
          for (; segTail; --segTail, ++src, ++dest) {
             AI_CALI_INFO cali;
             cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (__u16)CalibrateData_4704(*src, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }

          dest = (__u16*)daq_dev->fai_buffer.kaddr;
          for (; segHead; --segHead, ++src, ++dest) {
             AI_CALI_INFO cali;
             cali = faiStatus->ChanCaliData[faiStatus->CurrChan++];
             *dest = (__u16)CalibrateData_4704(*src, cali.Offset, cali.Span);
             faiStatus->CurrChan %= shared->FaiParam.LogChanCount;
          }
       }

       daq_fai_update_status(&shared->FaiParam, faiStatus, dataNum);
       buf_state = faiStatus->BufState;
   }
   spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

   if (faiStatus->AcqMode == DAQ_ACQ_INFINITE) {
      schedule_work(&daq_dev->fai_check_work);
   }

   if ((faiStatus->HwFlag & USB_FLAG_FAIOVERRUN) && !shared->IsEvtSignaled[KdxAiCacheOverflow]) {
      shared->IsEvtSignaled[KdxAiCacheOverflow] = 1;
      daq_device_signal_event(daq_dev, KdxAiCacheOverflow);
   }

   if ((buf_state & DAQ_IN_BUF_OVERRUN) && !shared->IsEvtSignaled[KdxAiOverrun]) {
      shared->IsEvtSignaled[KdxAiOverrun] = 1;
      daq_device_signal_event(daq_dev, KdxAiOverrun);
   }

   if ((buf_state & DAQ_IN_DATAREADY) && !shared->IsEvtSignaled[KdxAiDataReady]) {
      shared->IsEvtSignaled[KdxAiDataReady] = 1;
      daq_device_signal_event(daq_dev, KdxAiDataReady);
   }

   if (DAQ_IN_MUST_STOP(buf_state, faiStatus->AcqMode)) {
      schedule_work(&daq_dev->fai_stop_work);
      return DAQ_UR_BREAK;
   }

   return DAQ_UR_CONTINUE;
}

//-------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------
void daq_ai_initialize_hw(daq_device_t *daq_dev)
{
   DEVICE_SHARED *shared  = &daq_dev->shared;
   __u32          i;

   if (shared->AiChanType[0] != shared->HwAiChType) {
      daq_ai_set_channel_sctype(daq_dev, shared->AiChanType[0]);
   }

   for (i = 0; i < AI_CHL_COUNT; ++i) {
      daq_ai_set_channel_gain(daq_dev, i, shared->AiChanGain[i]);
   }

   shared->AiLogChanCount = daq_ai_calc_log_chan_count(shared->AiChanType, AI_CHL_COUNT);
}

void daq_fai_stop_acquisition(daq_device_t *daq_dev, int cleanup)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   unsigned long flags;

   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   if (shared->FaiStatus.FnState != DAQ_FN_RUNNING){
      spin_unlock_irqrestore(&daq_dev->fai_lock, flags);
   } else {
      shared->FaiStatus.FnState = DAQ_FN_STOPPED;
      spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

      daq_usb_reader_stop(&daq_dev->usb_reader);
      daq_usb_fai_stop(daq_dev);

      daq_device_signal_event(daq_dev, KdxAiStopped);
      wake_up_interruptible(&daq_dev->fai_queue);
   }

   if (cleanup) {
      daq_umem_unmap(&daq_dev->fai_buffer);

      spin_lock_irqsave(&daq_dev->fai_lock, flags);
      daq_dev->shared.FaiStatus.FnState = DAQ_FN_IDLE;
      spin_unlock_irqrestore(&daq_dev->fai_lock, flags);
   }
}

void daq_fai_check_work_func(struct work_struct *work)
{
   daq_device_t *daq_dev = container_of(work, daq_device_t, fai_check_work);

   if (daq_dev->shared.FaiStatus.FnState == DAQ_FN_RUNNING){
      daq_usb_dev_get_flag(daq_dev, &daq_dev->shared.FaiStatus.HwFlag);
   }
}

void daq_fai_stop_work_func(struct work_struct *work)
{
   daq_device_t *daq_dev = container_of(work, daq_device_t, fai_stop_work);

   daq_fai_stop_acquisition(daq_dev, 0);
}

int daq_ioctl_ai_set_channel(daq_device_t *daq_dev, unsigned long arg)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   AI_SET_CHAN   xbuf;
   AI_CHAN_CFG   cfg[AI_CHL_COUNT];
   __u8          gain;
   __u32         i;
   __u32         phych;

   if (unlikely(shared->FaiStatus.FnState == DAQ_FN_RUNNING)){
      return -EBUSY;
   }

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   if (unlikely(xbuf.PhyChanCount > AI_CHL_COUNT)){
      xbuf.PhyChanCount = AI_CHL_COUNT;
   }

   if (unlikely(copy_from_user(cfg, (void *)xbuf.ChanCfg, sizeof(AI_CHAN_CFG) * xbuf.PhyChanCount))){
      return -EFAULT;
   }

   if (xbuf.SetWhich & AI_SET_CHSCTYPE){
      daq_ai_set_channel_sctype(daq_dev, cfg[0].SCType);
   }

   if (xbuf.SetWhich & AI_SET_CHGAIN){
      for (i = 0; i < xbuf.PhyChanCount; ++i){
         phych = (xbuf.PhyChanStart + i) & AI_CHL_MASK;
         gain  = cfg[i].Gain;
         if (!gain || gain != shared->AiChanGain[phych]) {
            daq_ai_set_channel_gain(daq_dev, phych, gain);
         }
      }
   }

   if (xbuf.SetWhich & AI_SET_CHSCTYPE) {
      shared->AiLogChanCount = daq_ai_calc_log_chan_count(shared->AiChanType, AI_CHL_COUNT);
   }

   return 0;
}

int daq_ioctl_ai_read_sample(daq_device_t *daq_dev, unsigned long arg)
{
   AI_READ_SAMPLES xbuf;
   __u16           sample[AI_CHL_COUNT];
   __u32           i;
   __u32           phyChanStart;
   __u32           logChanCount;

   if (unlikely(daq_dev->shared.FaiStatus.FnState == DAQ_FN_RUNNING)){
      return -EBUSY;
   }

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   phyChanStart = xbuf.PhyChanStart & AI_CHL_MASK;
   logChanCount =  min(xbuf.LogChanCount, (__u32)AI_CHL_COUNT);
   for (i = 0; i < logChanCount; ++i)
   {
      if (daq_usb_ai_read_channel(daq_dev, phyChanStart, sample + i) < 0)
         return -EIO;
      phyChanStart += ( daq_dev->shared.AiChanType[0] ? 2 : 1 );
      phyChanStart %= AI_CHL_COUNT;
   }

   if (unlikely(copy_to_user((void *)xbuf.Data, sample, xbuf.LogChanCount * sizeof(__u16)))) {
      return -EFAULT;
   }

   return 0;
}

int daq_ioctl_fai_set_param(daq_device_t *daq_dev, unsigned long arg)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   FAI_CONFIG    xbuf;
   unsigned long flags;
   int           ret = 0;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   if (unlikely(shared->FaiStatus.FnState != DAQ_FN_IDLE)){
      ret = -EBUSY;
   } else {
      shared->FaiParam              = xbuf;
      shared->FaiParam.PhyChanStart = xbuf.PhyChanStart & AI_CHL_MASK;
      shared->FaiParam.LogChanCount = min(xbuf.LogChanCount, shared->AiLogChanCount);
   }
   spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

   if (likely(!ret)){
      daq_device_signal_event(daq_dev, KdxDevPropChged);
   }

   return ret;
}

int daq_ioctl_fai_set_buffer(daq_device_t *daq_dev, unsigned long arg)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   FAI_CONFIG  *faiParam = &shared->FaiParam;
   FAI_STATUS *faiStatus = &shared->FaiStatus;

   unsigned long flags;
   int           ret = 0;

   if (unlikely(!faiParam->SampleCount)) {
      return -EINVAL;
   }

   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   if (unlikely(faiStatus->FnState != DAQ_FN_IDLE)){
      ret = -EBUSY;
   } else {
      faiStatus->FnState   = DAQ_FN_READY;
      faiStatus->BufLength = faiParam->SampleCount;
   }
   spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

   do {
      if (unlikely(ret)){
         break;
      }

      ret = daq_umem_map(arg, faiParam->SampleCount * AI_DATA_SIZE, 1, &daq_dev->fai_buffer);
      if (unlikely(ret)){
         break;
      }

      if (!is_reader_inited(&daq_dev->usb_reader)){
         if (shared->ProductId == BD_USB4702) {
            ret = daq_usb_reader_init(daq_dev->udev, daq_dev->xfer_epd,
                     FAI_PACKET_SIZE, FAI_PACKET_NUM, daq_fai_xfer_complete_4702, daq_dev, &daq_dev->usb_reader);
         } else {
            ret = daq_usb_reader_init(daq_dev->udev, daq_dev->xfer_epd,
                     FAI_PACKET_SIZE, FAI_PACKET_NUM, daq_fai_xfer_complete_4704, daq_dev, &daq_dev->usb_reader);
         }
         if (unlikely(ret)){
            break;
         }
      }

   } while (0);

   if (ret){
      daq_umem_unmap(&daq_dev->fai_buffer);
      faiStatus->FnState = DAQ_FN_IDLE;
   }

   return ret;
}

int daq_ioctl_fai_start(daq_device_t *daq_dev, unsigned long arg)
{
   DEVICE_SHARED  *shared = &daq_dev->shared;
   unsigned long  flags;
   int            ret = 0;

   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   do{
      if (shared->FaiStatus.FnState == DAQ_FN_IDLE){
         ret = -EINVAL;
         break;
      }

      if (shared->FaiStatus.FnState == DAQ_FN_RUNNING){
         ret = -EBUSY;
         break;
      }

      memset(shared->IsEvtSignaled, 0, sizeof(shared->IsEvtSignaled));
      memset(&shared->FaiStatus, 0, sizeof(FAI_STATUS));
      shared->FaiStatus.FnState   = DAQ_FN_RUNNING;
      shared->FaiStatus.AcqMode   = (__u32)arg;
      shared->FaiStatus.BufLength = shared->FaiParam.SampleCount;
   } while (0);
   spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

   if (ret){
      return ret;
   }

   daq_device_clear_event(daq_dev, KdxAiDataReady);
   daq_device_clear_event(daq_dev, KdxAiOverrun);
   daq_device_clear_event(daq_dev, KdxAiStopped);
   daq_device_clear_event(daq_dev, KdxAiCacheOverflow);

   ret = daq_usb_reader_start(&daq_dev->usb_reader);
   if (ret){
      goto start_reader_err;
   }

   {
      __u16 phyChanStart, phyChanEnd, ChanConfig;
      __u32 ch_range, SampleRate, ch, gain, fwDataCount, totalFwDataCount;
      int i;

      ch_range = daq_ai_calc_phy_chan_range(shared->AiChanType, AI_CHL_COUNT, shared->FaiParam.PhyChanStart, shared->FaiParam.LogChanCount);
      phyChanEnd = ch_range >> 8;

      phyChanStart = shared->FaiParam.PhyChanStart;
      ChanConfig = shared->AiChanType[0] ? 0xFF : 0; // It must be all differential or all single-ended.
      if (ChanConfig) {
         phyChanStart &= ~1;
      }

      //kernel_fpu_begin();
      SampleRate = shared->FaiParam.ConvClkRatePerCH  * shared->FaiParam.LogChanCount;
      //kernel_fpu_end();

      shared->FaiStatus.CurrChan = 0; // '0' indicates the first scanning channel
      for (i = 0; i < shared->FaiParam.LogChanCount; ++i) {
         if (ChanConfig) { // differential
            ch = phyChanStart + i * 2;
            gain = shared->AiChanGain[ch];
            if (gain == 0) {
               gain = 7;
            }
         } else { // single-ended
            ch = phyChanStart + i;
            gain = shared->AiChanGain[ch];
         }

         if (daq_usb_fai_get_cali_info(daq_dev, gain, ch, &shared->FaiStatus.ChanCaliData[i].Offset, &shared->FaiStatus.ChanCaliData[i].Span) < 0){
            ret = -EIO;
            goto start_hw_err;
         }
      }

	  totalFwDataCount = shared->FaiParam.SectionSize * shared->FaiParam.LogChanCount;
	  while (totalFwDataCount % 10 == 0)
	  {
		  totalFwDataCount = totalFwDataCount / 10;
	  }
      fwDataCount = shared->FaiParam.SectionSize * 2;
      if ((shared->FaiParam.SectionSize * AI_DATA_SIZE) % FAI_CONTINUOUS_READER_PACKET_SIZE) {
         if (fwDataCount < 10) {
            fwDataCount = 10;
         } else {
            int power = -1;
            __u32 x;
            for (x = totalFwDataCount; x; x >>= 1) { ++power; }
            if (totalFwDataCount == (__u32)1 << power) {
               fwDataCount += shared->FaiParam.LogChanCount * 2;
            }
         }
      }

      if (daq_usb_fai_start(daq_dev, phyChanStart, (__u16)shared->FaiParam.LogChanCount, shared->FaiParam.ConvClkSource == SigInternalClock ? 0 : 1,
                            fwDataCount, 1, SampleRate, ChanConfig, phyChanEnd, shared->AiChanGain) < 0){
         ret = -EIO;
         goto start_hw_err;
      }
   }

   if (shared->FaiStatus.AcqMode == DAQ_ACQ_FINITE_SYNC){
      ret = wait_event_interruptible(daq_dev->fai_queue, shared->FaiStatus.FnState != DAQ_FN_RUNNING);
   }

   return ret;

start_hw_err:
   daq_usb_reader_stop(&daq_dev->usb_reader);

start_reader_err:
   spin_lock_irqsave(&daq_dev->fai_lock, flags);
   shared->FaiStatus.FnState = DAQ_FN_READY;
   spin_unlock_irqrestore(&daq_dev->fai_lock, flags);

   return ret;
}

int daq_ioctl_fai_stop(daq_device_t *daq_dev, unsigned long arg)
{
   DEVICE_SHARED *shared = &daq_dev->shared;

   if (shared->FaiStatus.FnState == DAQ_FN_IDLE){
      return 0;
   }

   daq_fai_stop_acquisition(daq_dev, arg & FAI_STOP_FREE_RES);

   return 0;
}

