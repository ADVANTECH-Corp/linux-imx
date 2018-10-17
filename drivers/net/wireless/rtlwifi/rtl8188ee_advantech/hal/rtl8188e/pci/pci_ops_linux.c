/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _HCI_OPS_OS_C_

#include <drv_types.h>
#include <rtl8188e_hal.h>

#if defined(PLATFORM_LINUX) && defined (PLATFORM_WINDOWS)

	#error "Shall be Linux or Windows, but not both!\n"

#endif


static int rtl8188ee_init_rx_ring(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct net_device	*dev = padapter->pnetdev;
	struct recv_stat	*entry = NULL;
	dma_addr_t *mapping = NULL;
	struct sk_buff *skb = NULL;
	int i, rx_queue_idx;


	/* rx_queue_idx 0:RX_MPDU_QUEUE */
	/* rx_queue_idx 1:RX_CMD_QUEUE */
	for (rx_queue_idx = 0; rx_queue_idx < 1/*RX_MAX_QUEUE*/; rx_queue_idx++) {
		precvpriv->rx_ring[rx_queue_idx].desc =
			pci_alloc_consistent(pdev,
			sizeof(*precvpriv->rx_ring[rx_queue_idx].desc) * precvpriv->rxringcount,
				     &precvpriv->rx_ring[rx_queue_idx].dma);

		if (!precvpriv->rx_ring[rx_queue_idx].desc
		    || (unsigned long)precvpriv->rx_ring[rx_queue_idx].desc & 0xFF) {
			RTW_INFO("Cannot allocate RX ring\n");
			return _FAIL;
		}

		_rtw_memset(precvpriv->rx_ring[rx_queue_idx].desc, 0, sizeof(*precvpriv->rx_ring[rx_queue_idx].desc) * precvpriv->rxringcount);
		precvpriv->rx_ring[rx_queue_idx].idx = 0;

		for (i = 0; i < precvpriv->rxringcount; i++) {
			skb = rtw_skb_alloc(precvpriv->rxbuffersize);
			if (!skb) {
				RTW_INFO("Cannot allocate skb for RX ring\n");
				return _FAIL;
			}

			entry = &precvpriv->rx_ring[rx_queue_idx].desc[i];

			precvpriv->rx_ring[rx_queue_idx].rx_buf[i] = skb;

			mapping = (dma_addr_t *)skb->cb;

			/* just set skb->cb to mapping addr for pci_unmap_single use */
			*mapping = pci_map_single(pdev, skb_tail_pointer(skb),
						  precvpriv->rxbuffersize,
						  PCI_DMA_FROMDEVICE);

			/* entry->BufferAddress = cpu_to_le32(*mapping); */
			entry->rxdw6 = cpu_to_le32(*mapping);

			/* entry->Length = precvpriv->rxbuffersize; */
			/* entry->OWN = 1; */
			entry->rxdw0 |= cpu_to_le32(precvpriv->rxbuffersize & 0x00003fff);
			entry->rxdw0 |= cpu_to_le32(OWN);
		}

		/* entry->EOR = 1; */
		entry->rxdw0 |= cpu_to_le32(EOR);
	}


	return _SUCCESS;
}

static void rtl8188ee_free_rx_ring(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	int i, rx_queue_idx;


	/* rx_queue_idx 0:RX_MPDU_QUEUE */
	/* rx_queue_idx 1:RX_CMD_QUEUE */
	for (rx_queue_idx = 0; rx_queue_idx < 1/*RX_MAX_QUEUE*/; rx_queue_idx++) {
		for (i = 0; i < precvpriv->rxringcount; i++) {
			struct sk_buff *skb = precvpriv->rx_ring[rx_queue_idx].rx_buf[i];
			if (!skb)
				continue;

			pci_unmap_single(pdev,
					 *((dma_addr_t *) skb->cb),
					 precvpriv->rxbuffersize,
					 PCI_DMA_FROMDEVICE);
			kfree_skb(skb);
		}

		pci_free_consistent(pdev,
			    sizeof(*precvpriv->rx_ring[rx_queue_idx].desc) *
				    precvpriv->rxringcount,
				    precvpriv->rx_ring[rx_queue_idx].desc,
				    precvpriv->rx_ring[rx_queue_idx].dma);
		precvpriv->rx_ring[rx_queue_idx].desc = NULL;
	}

}


static int rtl8188ee_init_tx_ring(_adapter *padapter, unsigned int prio, unsigned int entries)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct tx_desc	*ring;
	dma_addr_t		dma;
	int	i;


	/* RTW_INFO("%s entries num:%d\n", __func__, entries); */
	ring = pci_alloc_consistent(pdev, sizeof(*ring) * entries, &dma);
	if (!ring || (unsigned long)ring & 0xFF) {
		RTW_INFO("Cannot allocate TX ring (prio = %d)\n", prio);
		return _FAIL;
	}

	_rtw_memset(ring, 0, sizeof(*ring) * entries);
	pxmitpriv->tx_ring[prio].qid = prio;
	pxmitpriv->tx_ring[prio].desc = ring;
	pxmitpriv->tx_ring[prio].dma = dma;
	pxmitpriv->tx_ring[prio].idx = 0;
	pxmitpriv->tx_ring[prio].entries = entries;
	_rtw_init_queue(&pxmitpriv->tx_ring[prio].queue);
	pxmitpriv->tx_ring[prio].qlen = 0;

	/* RTW_INFO("%s queue:%d, ring_addr:%p\n", __func__, prio, ring); */

	for (i = 0; i < entries; i++)
		ring[i].txdw10 = cpu_to_le32((u32) dma + ((i + 1) % entries) * sizeof(*ring));


	return _SUCCESS;
}

static void rtl8188ee_free_tx_ring(_adapter *padapter, unsigned int prio)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct rtw_tx_ring *ring = &pxmitpriv->tx_ring[prio];
	struct xmit_buf	*pxmitbuf;


	while (ring->qlen) {
		pxmitbuf = list_first_entry(&ring->queue.queue, struct xmit_buf, list);
		rtw_list_delete(&(pxmitbuf->list));
		pci_unmap_single(pdev, le32_to_cpu(pxmitbuf->desc->txdw8), pxmitbuf->len, PCI_DMA_TODEVICE);
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		ring->qlen--;
	}

	pci_free_consistent(pdev, sizeof(*ring->desc) * ring->entries, ring->desc, ring->dma);
	ring->desc = NULL;
	ring->idx = 0;

}

static void init_desc_ring_var(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	u8 i = 0;

	for (i = 0; i < HW_QUEUE_ENTRY; i++)
		pxmitpriv->txringcount[i] = TX_DESC_NUM_8188EE;

	/* we just alloc 2 desc for beacon queue, */
	/* because we just need first desc in hw beacon. */
	pxmitpriv->txringcount[BCN_QUEUE_INX] = 1;

	/* BE queue need more descriptor for performance consideration */
	/* or, No more tx desc will happen, and may cause mac80211 mem leakage. */
	/* if(!padapter->registrypriv.wifi_spec) */
	/*	pxmitpriv->txringcount[BE_QUEUE_INX] = TXDESC_NUM_BE_QUEUE; */

	pxmitpriv->txringcount[BE_QUEUE_INX]  = BE_QUEUE_TX_DESC_NUM_8188EE;
	pxmitpriv->txringcount[TXCMD_QUEUE_INX] = 1;

	precvpriv->rxbuffersize = MAX_RECVBUF_SZ;	/* 2048; */ /* 1024; */
	precvpriv->rxringcount = PCI_MAX_RX_COUNT;	/* 64; */
}


u32 rtl8188ee_init_desc_ring(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	int	i, ret = _SUCCESS;


	init_desc_ring_var(padapter);

	ret = rtl8188ee_init_rx_ring(padapter);
	if (ret == _FAIL)
		return ret;

	/* general process for other queue */
	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++) {
		ret = rtl8188ee_init_tx_ring(padapter, i, pxmitpriv->txringcount[i]);
		if (ret == _FAIL)
			goto err_free_rings;
	}

	return ret;

err_free_rings:

	rtl8188ee_free_rx_ring(padapter);

	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++)
		if (pxmitpriv->tx_ring[i].desc)
			rtl8188ee_free_tx_ring(padapter, i);


	return ret;
}

u32 rtl8188ee_free_desc_ring(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	u32 i;


	/* free rx rings */
	rtl8188ee_free_rx_ring(padapter);

	/* free tx rings */
	for (i = 0; i < HW_QUEUE_ENTRY; i++)
		rtl8188ee_free_tx_ring(padapter, i);


	return _SUCCESS;
}

void rtl8188ee_reset_desc_ring(_adapter *padapter)
{
	_irqL	irqL;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	int i, rx_queue_idx;

	for (rx_queue_idx = 0; rx_queue_idx < 1; rx_queue_idx++) {
		if (precvpriv->rx_ring[rx_queue_idx].desc) {
			struct recv_stat *entry = NULL;
			for (i = 0; i < precvpriv->rxringcount; i++) {
				entry = &precvpriv->rx_ring[rx_queue_idx].desc[i];
				entry->rxdw0 |= cpu_to_le32(OWN);
			}
			precvpriv->rx_ring[rx_queue_idx].idx = 0;
		}
	}

	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);
	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++) {
		if (pxmitpriv->tx_ring[i].desc) {
			struct rtw_tx_ring *ring = &pxmitpriv->tx_ring[i];
			struct xmit_buf	*pxmitbuf;

			while (ring->qlen) {
				pxmitbuf = list_first_entry(&ring->queue.queue, struct xmit_buf, list);
				rtw_list_delete(&(pxmitbuf->list));
				pci_unmap_single(pdvobjpriv->ppcidev, le32_to_cpu(pxmitbuf->desc->txdw8), pxmitbuf->len, PCI_DMA_TODEVICE);
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				ring->qlen--;
			}
			ring->idx = 0;
		}
	}
	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);
}

static void rtl8188ee_xmit_beacon(PADAPTER Adapter)
{
#if defined(CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME)
	struct mlme_priv *pmlmepriv = &Adapter->mlmepriv;

	if (MLME_IS_AP(Adapter) || MLME_IS_MESH(Adapter)) {
		/* send_beacon(Adapter); */
		if (pmlmepriv->update_bcn == _TRUE)
			tx_beacon_hdl(Adapter, NULL);
	}
#endif
}

void rtl8188ee_prepare_bcn_tasklet(void *priv)
{
	_adapter	*padapter = (_adapter *)priv;

	rtl8188ee_xmit_beacon(padapter);
}

static u8 check_tx_desc_resource(_adapter *padapter, int prio)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct rtw_tx_ring	*ring;

	ring = &pxmitpriv->tx_ring[prio];

	/* for now we reserve two free descriptor as a safety boundary */
	/* between the tail and the head */
	/*  */
	if ((ring->entries - ring->qlen) >= 2)
		return _TRUE;
	else {
		/* RTW_INFO("do not have enough desc for Tx\n"); */
		return _FALSE;
	}
}

static void rtl8188ee_tx_isr(PADAPTER Adapter, int prio)
{
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
	struct rtw_tx_ring	*ring = &pxmitpriv->tx_ring[prio];
	struct xmit_buf	*pxmitbuf;

	while (ring->qlen) {
		pxmitbuf = list_first_entry(&ring->queue.queue,
					    struct xmit_buf, list);

		if (le32_to_cpu(pxmitbuf->desc->txdw0) & OWN)
			return; /* not done */
		else
			ring->idx = (ring->idx + 1) % ring->entries;

		rtw_list_delete(&(pxmitbuf->list));
		ring->qlen--;

		pci_unmap_single(pdvobjpriv->ppcidev, le32_to_cpu(pxmitbuf->desc->txdw8), pxmitbuf->len, PCI_DMA_TODEVICE);
		rtw_sctx_done(&pxmitbuf->sctx);
		rtw_free_xmitbuf(&(pxmitbuf->padapter->xmitpriv), pxmitbuf);
	}

	if (check_tx_desc_resource(Adapter, prio)
	    && rtw_xmit_ac_blocked(Adapter) != _TRUE
	   )
		rtw_mi_xmit_tasklet_schedule(Adapter);
}


s32	rtl8188ee_interrupt(PADAPTER Adapter)
{
	_irqL	irqL;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	RT_ISR_CONTENT	isr_content;

	static PADAPTER temp = NULL;

	/* PRT_ISR_CONTENT	pisr_content = &pdvobjpriv->isr_content; */
	int	ret = _SUCCESS;

	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);

	DBG_COUNTER(Adapter->int_logs.all);

	/* read ISR: 4/8bytes */
	InterruptRecognized8188EE(Adapter, &isr_content);

	/* Shared IRQ or HW disappared */
	if (!isr_content.IntArray[0] || isr_content.IntArray[0] == 0xffff) {
		DBG_COUNTER(Adapter->int_logs.err);
		ret = _FAIL;
		goto done;
	}

	/* <1> beacon related */
	if (isr_content.IntArray[0] & IMR_TBDOK_88E)
		DBG_COUNTER(Adapter->int_logs.tbdok);

	if (isr_content.IntArray[0] & IMR_TBDER_88E)
		DBG_COUNTER(Adapter->int_logs.tbder);

	if (isr_content.IntArray[0] & IMR_BCNDERR0_88E) {
		/*re-transmit beacon to HW*/
		struct tasklet_struct  *bcn_tasklet;
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);/*Modify for MI temporary,this processor cannot apply to multi-ap*/

		DBG_COUNTER(Adapter->int_logs.bcnderr);
		bcn_adapter->mlmepriv.update_bcn = _TRUE;
		bcn_tasklet = &bcn_adapter->recvpriv.irq_prepare_beacon_tasklet;
		tasklet_hi_schedule(bcn_tasklet);
	}

	if (isr_content.IntArray[0] & IMR_BCNDMAINT0_88E) {
		struct tasklet_struct  *bcn_tasklet;
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);/*Modify for MI temporary,this processor cannot apply to multi-ap*/

		DBG_COUNTER(Adapter->int_logs.bcndma);
		bcn_tasklet = &bcn_adapter->recvpriv.irq_prepare_beacon_tasklet;
		tasklet_hi_schedule(bcn_tasklet);
	}

#ifdef CONFIG_CONCURRENT_MODE
	if (isr_content.IntArray[0] & IMR_BCNDMAINT_E_88E) {
		struct tasklet_struct  *bcn_tasklet;
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);/*Modify for MI temporary,this processor cannot apply to multi-ap*/

		DBG_COUNTER(Adapter->int_logs.bcndma_e);
		bcn_tasklet = &bcn_adapter->recvpriv.irq_prepare_beacon_tasklet;

		tasklet_hi_schedule(bcn_tasklet);
	}
#endif

	/* <2> Rx related */
	if ((isr_content.IntArray[0] & (IMR_ROK_88E | IMR_RDU_88E)) || (isr_content.IntArray[1] & IMR_RXFOVW_88E)) {

		DBG_COUNTER(Adapter->int_logs.rx);

		if (isr_content.IntArray[0] & IMR_RDU_88E)
			DBG_COUNTER(Adapter->int_logs.rx_rdu);

		if (isr_content.IntArray[1] & IMR_RXFOVW_88E)
			DBG_COUNTER(Adapter->int_logs.rx_fovw);

		pHalData->IntrMaskToSet[0] &= (~(IMR_ROK_88E | IMR_RDU_88E));
		pHalData->IntrMaskToSet[1] &= (~IMR_RXFOVW_88E);
		rtw_write32(Adapter, REG_HIMR_88E, pHalData->IntrMaskToSet[0]);
		rtw_write32(Adapter, REG_HIMRE_88E, pHalData->IntrMaskToSet[1]);
		tasklet_hi_schedule(&Adapter->recvpriv.recv_tasklet);
	}

	/* <3> Tx related */
	if (isr_content.IntArray[1] & IMR_TXFOVW_88E) {
		DBG_COUNTER(Adapter->int_logs.txfovw);
		if (printk_ratelimit())
			RTW_INFO("IMR_TXFOVW!\n");
	}

	/* if (pisr_content->IntArray[0] & IMR_TX_MASK) { */
	/*	tasklet_hi_schedule(&pxmitpriv->xmit_tasklet); */
	/* } */

	if (isr_content.IntArray[0] & IMR_MGNTDOK_88E) {
		/* RTW_INFO("Manage ok interrupt!\n");		 */
		DBG_COUNTER(Adapter->int_logs.mgntok);
		rtl8188ee_tx_isr(Adapter, MGT_QUEUE_INX);
	}

	if (isr_content.IntArray[0] & IMR_HIGHDOK_88E) {
		/* RTW_INFO("HIGH_QUEUE ok interrupt!\n"); */
		DBG_COUNTER(Adapter->int_logs.highdok);
		rtl8188ee_tx_isr(Adapter, HIGH_QUEUE_INX);
	}

	if (isr_content.IntArray[0] & IMR_BKDOK_88E) {
		/* RTW_INFO("BK Tx OK interrupt!\n"); */
		DBG_COUNTER(Adapter->int_logs.bkdok);
		rtl8188ee_tx_isr(Adapter, BK_QUEUE_INX);
	}

	if (isr_content.IntArray[0] & IMR_BEDOK_88E) {
		/* RTW_INFO("BE TX OK interrupt!\n"); */
		DBG_COUNTER(Adapter->int_logs.bedok);
		rtl8188ee_tx_isr(Adapter, BE_QUEUE_INX);
	}

	if (isr_content.IntArray[0] & IMR_VIDOK_88E) {
		/* RTW_INFO("VI TX OK interrupt!\n"); */
		DBG_COUNTER(Adapter->int_logs.vidok);
		rtl8188ee_tx_isr(Adapter, VI_QUEUE_INX);
	}

	if (isr_content.IntArray[0] & IMR_VODOK_88E) {
		/* RTW_INFO("Vo TX OK interrupt!\n"); */
		DBG_COUNTER(Adapter->int_logs.vodok);
		rtl8188ee_tx_isr(Adapter, VO_QUEUE_INX);
	}

done:

	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);

	return ret;
}

static void rtl8188ee_rx_mpdu(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	_queue			*pfree_recv_queue = &precvpriv->free_recv_queue;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(padapter);
	union recv_frame	*precvframe = NULL;
	struct rx_pkt_attrib	*pattrib = NULL;
	int	rx_queue_idx = RX_MPDU_QUEUE;
	u32	count = precvpriv->rxringcount;

	/*RX NORMAL PKT*/
	while (count--) {
		struct recv_stat *prxstat = &precvpriv->rx_ring[rx_queue_idx].desc[precvpriv->rx_ring[rx_queue_idx].idx];/* rx descriptor */
		struct sk_buff *skb = precvpriv->rx_ring[rx_queue_idx].rx_buf[precvpriv->rx_ring[rx_queue_idx].idx];/* rx pkt */

		if (le32_to_cpu(prxstat->rxdw0) & OWN) { /* OWN bit */
			/* wait data to be filled by hardware */
			return;
		} else {
			DBG_COUNTER(padapter->rx_logs.intf_rx);
			precvframe = rtw_alloc_recvframe(pfree_recv_queue);
			if (precvframe == NULL) {
				RTW_INFO("%s: precvframe==NULL\n", __func__);
				DBG_COUNTER(padapter->rx_logs.intf_rx_err_recvframe);
				goto done;
			}

			_rtw_init_listhead(&precvframe->u.hdr.list);
			precvframe->u.hdr.len = 0;

			pci_unmap_single(pdvobjpriv->ppcidev,
					 *((dma_addr_t *)skb->cb),
					 precvpriv->rxbuffersize,
					 PCI_DMA_FROMDEVICE);

			rtl8188e_query_rx_desc_status(precvframe, prxstat);
			pattrib = &precvframe->u.hdr.attrib;

#ifdef CONFIG_RX_PACKET_APPEND_FCS
			if (check_fwstate(&padapter->mlmepriv, WIFI_MONITOR_STATE) == _FALSE)
				if ((pattrib->pkt_rpt_type == NORMAL_RX) && rtw_hal_rcr_check(padapter, RCR_APPFCS))
					pattrib->pkt_len -= IEEE80211_FCS_LEN;
#endif

			if (rtw_os_alloc_recvframe(padapter, precvframe,
				(skb->data + pattrib->drvinfo_sz + pattrib->shift_sz), skb) == _FAIL) {

				rtw_free_recvframe(precvframe, &precvpriv->free_recv_queue);
				RTW_INFO("rtl8188ee_rx_mpdu:can not allocate memory for skb copy\n");
				*((dma_addr_t *) skb->cb) = pci_map_single(pdvobjpriv->ppcidev, skb_tail_pointer(skb), precvpriv->rxbuffersize, PCI_DMA_FROMDEVICE);
				DBG_COUNTER(padapter->rx_logs.intf_rx_err_skb);
				goto done;
			}

			recvframe_put(precvframe, pattrib->pkt_len);

			/*			rtl8188e_translate_rx_signal_stuff(precvframe, pphy_info);*/

			if (pattrib->pkt_rpt_type == NORMAL_RX) /*Normal rx packet*/
				pre_recv_entry(precvframe, pattrib->physt ? skb->data : NULL);
			else { /* pkt_rpt_type == TX_REPORT1-CCX, TX_REPORT2-TX RTP,HIS_REPORT-USB HISR RTP				 */
				DBG_COUNTER(padapter->rx_logs.intf_rx_report);

				/* enqueue recvframe to txrtp queue */
				if (pattrib->pkt_rpt_type == TX_REPORT1) {
					/* RTW_INFO("rx CCX\n"); */
					/* CCX-TXRPT ack for xmit mgmt frames. */
					handle_txrpt_ccx_88e(padapter, precvframe->u.hdr.rx_data);
				}
				else if (pattrib->pkt_rpt_type == TX_REPORT2) {
					/* RTW_INFO("recv TX RPT\n"); */
					odm_ra_tx_rpt2_handle_8188e(
						&pHalData->odmpriv,
						precvframe->u.hdr.rx_data,
						pattrib->pkt_len,
						pattrib->MacIDValidEntry[0],
						pattrib->MacIDValidEntry[1]
					);

				} else if (pattrib->pkt_rpt_type == HIS_REPORT)
					RTW_INFO("rx USB HISR\n");
				rtw_free_recvframe(precvframe, pfree_recv_queue);
			}
			/* precvpriv->rx_ring[rx_queue_idx].rx_buf[precvpriv->rx_ring[rx_queue_idx].idx] = skb; */
			*((dma_addr_t *) skb->cb) = pci_map_single(pdvobjpriv->ppcidev, skb_tail_pointer(skb), precvpriv->rxbuffersize, PCI_DMA_FROMDEVICE);
		}
done:

		prxstat->rxdw6 = cpu_to_le32(*((dma_addr_t *)skb->cb));

		prxstat->rxdw0 |= cpu_to_le32(precvpriv->rxbuffersize & 0x00003fff);
		prxstat->rxdw0 |= cpu_to_le32(OWN);

		if (precvpriv->rx_ring[rx_queue_idx].idx == precvpriv->rxringcount - 1)
			prxstat->rxdw0 |= cpu_to_le32(EOR);
		precvpriv->rx_ring[rx_queue_idx].idx = (precvpriv->rx_ring[rx_queue_idx].idx + 1) % precvpriv->rxringcount;

	}

}

void rtl8188ee_recv_tasklet(void *priv)
{
	_irqL	irqL;
	_adapter	*padapter = (_adapter *)priv;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	rtl8188ee_rx_mpdu(padapter);
	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);
	pHalData->IntrMaskToSet[0] |= (IMR_ROK_88E | IMR_RDU_88E);
	pHalData->IntrMaskToSet[1] |= IMR_RXFOVW_88E;
	rtw_write32(padapter, REG_HIMR_88E, pHalData->IntrMaskToSet[0]);
	rtw_write32(padapter, REG_HIMRE_88E, pHalData->IntrMaskToSet[1]);
	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);
}

static u8 pci_read8(struct intf_hdl *pintfhdl, u32 addr)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;
	/*	printk("%s, addr=%08x,  val=%02x\n", __func__, addr,  readb((u8 *)pdvobjpriv->pci_mem_start + addr)); */
	return 0xff & readb((u8 *)pdvobjpriv->pci_mem_start + addr);
}

static u16 pci_read16(struct intf_hdl *pintfhdl, u32 addr)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;
	/*	printk("%s, addr=%08x,  val=%04x\n", __func__, addr,  readw((u8 *)pdvobjpriv->pci_mem_start + addr)); */
	return readw((u8 *)pdvobjpriv->pci_mem_start + addr);
}

static u32 pci_read32(struct intf_hdl *pintfhdl, u32 addr)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;
	/*	printk("%s, addr=%08x,  val=%08x\n", __func__, addr,  readl((u8 *)pdvobjpriv->pci_mem_start + addr)); */
	return readl((u8 *)pdvobjpriv->pci_mem_start + addr);
}

/* 2009.12.23. by tynli. Suggested by SD1 victorh. For ASPM hang on AMD and Nvidia.
 * 20100212 Tynli: Do read IO operation after write for all PCI bridge suggested by SD1.
 * Origianally this is only for INTEL. */
static int pci_write8(struct intf_hdl *pintfhdl, u32 addr, u8 val)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;

	writeb(val, (u8 *)pdvobjpriv->pci_mem_start + addr);
	/* readb((u8 *)pdvobjpriv->pci_mem_start + addr); */
	return 1;
}

static int pci_write16(struct intf_hdl *pintfhdl, u32 addr, u16 val)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;
	writew(val, (u8 *)pdvobjpriv->pci_mem_start + addr);
	/* readw((u8 *)pdvobjpriv->pci_mem_start + addr); */
	return 2;
}

static int pci_write32(struct intf_hdl *pintfhdl, u32 addr, u32 val)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;
	writel(val, (u8 *)pdvobjpriv->pci_mem_start + addr);
	/* readl((u8 *)pdvobjpriv->pci_mem_start + addr); */
	return 4;
}


static void pci_read_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{

}

static void pci_write_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

}

static u32 pci_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	return 0;
}

void rtl8188ee_xmit_tasklet(void *priv)
{
	/* _irqL irqL; */
	_adapter			*padapter = (_adapter *)priv;
	/* struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter); */
	/* PRT_ISR_CONTENT	pisr_content = &pdvobjpriv->isr_content; */

	/*_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);

	if (pisr_content->IntArray[0] & IMR_BCNDERR0_88E) {

		rtl8188ee_tx_isr(padapter, BCN_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_MGNTDOK) {

		rtl8188ee_tx_isr(padapter, MGT_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_HIGHDOK) {

		rtl8188ee_tx_isr(padapter, HIGH_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_BKDOK) {

		rtl8188ee_tx_isr(padapter, BK_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_BEDOK) {

		rtl8188ee_tx_isr(padapter, BE_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_VIDOK) {

		rtl8188ee_tx_isr(padapter, VI_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_VODOK) {

		rtl8188ee_tx_isr(padapter, VO_QUEUE_INX);
	}

	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);

	if (check_fwstate(&padapter->mlmepriv, _FW_UNDER_SURVEY) != _TRUE)
	{*/
	/* try to deal with the pending packets */
	rtl8188ee_xmitframe_resume(padapter);
	/* } */

}

static u32 pci_write_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{
	_adapter			*padapter = (_adapter *)pintfhdl->padapter;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
	netif_trans_update(padapter->pnetdev);
#else
	padapter->pnetdev->trans_start = jiffies;
#endif

	return 0;
}

void rtl8188ee_set_intf_ops(struct _io_ops	*pops)
{

	_rtw_memset((u8 *)pops, 0, sizeof(struct _io_ops));

	pops->_read8 = &pci_read8;
	pops->_read16 = &pci_read16;
	pops->_read32 = &pci_read32;

	pops->_read_mem = &pci_read_mem;
	pops->_read_port = &pci_read_port;

	pops->_write8 = &pci_write8;
	pops->_write16 = &pci_write16;
	pops->_write32 = &pci_write32;

	pops->_write_mem = &pci_write_mem;
	pops->_write_port = &pci_write_port;


}
