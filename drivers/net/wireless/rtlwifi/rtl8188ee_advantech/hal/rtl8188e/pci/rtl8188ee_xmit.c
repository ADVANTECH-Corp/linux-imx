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
#define _RTL8188E_XMIT_C_

#include <drv_types.h>
#include <rtl8188e_hal.h>

#if defined(PLATFORM_LINUX) && defined (PLATFORM_WINDOWS)
	#error "Shall be Linux or Windows, but not both!\n"
#endif


s32	rtl8188ee_init_xmit_priv(_adapter *padapter)
{
	s32	ret = _SUCCESS;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	_rtw_spinlock_init(&pdvobjpriv->irq_th_lock);
	/*	_rtw_spinlock_init(&pHalData->rf_lock); */

#ifdef PLATFORM_LINUX
	tasklet_init(&pxmitpriv->xmit_tasklet,
		     (void(*)(unsigned long))rtl8188ee_xmit_tasklet,
		     (unsigned long)padapter);
#endif

	return ret;
}

void	rtl8188ee_free_xmit_priv(_adapter *padapter)
{
	/* u8	i; */
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	_rtw_spinlock_free(&pdvobjpriv->irq_th_lock);
	/*	_rtw_spinlock_free(&pHalData->rf_lock); */

}

static u16 ffaddr2dma(u32 addr)
{
	u16	dma_ctrl;
	switch (addr) {
	case VO_QUEUE_INX:
		dma_ctrl = BIT3;
		break;
	case VI_QUEUE_INX:
		dma_ctrl = BIT2;
		break;
	case BE_QUEUE_INX:
		dma_ctrl = BIT1;
		break;
	case BK_QUEUE_INX:
		dma_ctrl = BIT0;
		break;
	case BCN_QUEUE_INX:
		dma_ctrl = BIT4;
		break;
	case MGT_QUEUE_INX:
		dma_ctrl = BIT6;
		break;
	case HIGH_QUEUE_INX:
		dma_ctrl = BIT7;
		break;
	default:
		dma_ctrl = 0;
		break;
	}

	return dma_ctrl;
}


static void fill_txdesc_sectype(struct pkt_attrib *pattrib, struct tx_desc *ptxdesc)
{
	if ((pattrib->encrypt > 0) && !pattrib->bswenc) {
		switch (pattrib->encrypt) {
		/* SEC_TYPE */
		case _WEP40_:
		case _WEP104_:
			ptxdesc->txdw1 |= cpu_to_le32((0x01 << 22) & 0x00c00000);
			break;
		case _TKIP_:
		case _TKIP_WTMIC_:
			/* ptxdesc->txdw1 |= cpu_to_le32((0x02<<22)&0x00c00000); */
			ptxdesc->txdw1 |= cpu_to_le32((0x01 << 22) & 0x00c00000);
			break;
		case _AES_:
			ptxdesc->txdw1 |= cpu_to_le32((0x03 << 22) & 0x00c00000);
			break;
		case _NO_PRIVACY_:
		default:
			break;

		}

	}

}

static void fill_txdesc_vcs(struct pkt_attrib *pattrib, u32 *pdw)
{
	/* RTW_INFO("cvs_mode=%d\n", pattrib->vcs_mode);	 */

	switch (pattrib->vcs_mode) {
	case RTS_CTS:
		*pdw |= cpu_to_le32(BIT(12));
		break;
	case CTS_TO_SELF:
		*pdw |= cpu_to_le32(BIT(11));
		break;
	case NONE_VCS:
	default:
		break;
	}

	if (pattrib->vcs_mode) {
		*pdw |= cpu_to_le32(HW_RTS_EN);

		/* Set RTS BW */
		if (pattrib->ht_en) {
			*pdw |= (pattrib->bwmode & CHANNEL_WIDTH_40) ?	cpu_to_le32(BIT(27)) : 0;

			if (pattrib->ch_offset == HAL_PRIME_CHNL_OFFSET_LOWER)
				*pdw |= cpu_to_le32((0x01 << 28) & 0x30000000);
			else if (pattrib->ch_offset == HAL_PRIME_CHNL_OFFSET_UPPER)
				*pdw |= cpu_to_le32((0x02 << 28) & 0x30000000);
			else if (pattrib->ch_offset == HAL_PRIME_CHNL_OFFSET_DONT_CARE)
				*pdw |= 0;
			else
				*pdw |= cpu_to_le32((0x03 << 28) & 0x30000000);
		}
	}
}

static void fill_txdesc_phy(struct pkt_attrib *pattrib, u32 *pdw)
{
	/* RTW_INFO("bwmode=%d, ch_off=%d\n", pattrib->bwmode, pattrib->ch_offset); */

	if (pattrib->ht_en) {
		*pdw |= (pattrib->bwmode & CHANNEL_WIDTH_40) ?	cpu_to_le32(BIT(25)) : 0;

		if (pattrib->ch_offset == HAL_PRIME_CHNL_OFFSET_LOWER)
			*pdw |= cpu_to_le32((0x01 << 20) & 0x003f0000);
		else if (pattrib->ch_offset == HAL_PRIME_CHNL_OFFSET_UPPER)
			*pdw |= cpu_to_le32((0x02 << 20) & 0x003f0000);
		else if (pattrib->ch_offset == HAL_PRIME_CHNL_OFFSET_DONT_CARE)
			*pdw |= 0;
		else
			*pdw |= cpu_to_le32((0x03 << 20) & 0x003f0000);
	}
}
/*
 * Description: In normal chip, we should send some packet to Hw which will be used by Fw
 *			in FW LPS mode. The function is to fill the Tx descriptor of this packets, then
 *			Fw can tell Hw to send these packet derectly.
 *   */
void rtl8188e_fill_fake_txdesc(
	PADAPTER	padapter,
	u8		*pDesc,
	u32		BufferLen,
	u8		IsPsPoll,
	u8		IsBTQosNull,
	u8		bDataFrame)
{
	struct tx_desc *ptxdesc;


	/* Clear all status */
	ptxdesc = (struct tx_desc *)pDesc;
	_rtw_memset(pDesc, 0, TXDESC_SIZE);

	/* offset 0 */
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE + OFFSET_SZ) << OFFSET_SHT) & 0x00ff0000); /* 32 bytes for TX Desc */

	ptxdesc->txdw0 |= cpu_to_le32(BufferLen & 0x0000ffff); /* Buffer size + command header */

	/* offset 4 */
	ptxdesc->txdw1 |= cpu_to_le32((QSLT_MGNT << QSEL_SHT) & 0x00001f00); /* Fixed queue of Mgnt queue */

	/* Set NAVUSEHDR to prevent Ps-poll AId filed to be changed to error vlaue by Hw. */
	if (IsPsPoll)
		ptxdesc->txdw1 |= cpu_to_le32(NAVUSEHDR);
	else {
		ptxdesc->txdw4 |= cpu_to_le32(BIT(7)); /* Hw set sequence number */
		ptxdesc->txdw3 |= cpu_to_le32((8 << 28)); /* set bit3 to 1. Suugested by TimChen. 2009.12.29. */
	}

	if (_TRUE == IsBTQosNull) {
		ptxdesc->txdw2 |= cpu_to_le32(BIT(23)); /* BT NULL */
	}

	/* offset 16 */
	ptxdesc->txdw4 |= cpu_to_le32(BIT(8));/* driver uses rate */

	/*  */
	/* Encrypt the data frame if under security mode excepct null data. Suggested by CCW. */
	/*  */
	if (_TRUE == bDataFrame) {
		u32 EncAlg;

		EncAlg = padapter->securitypriv.dot11PrivacyAlgrthm;
		switch (EncAlg) {
		case _NO_PRIVACY_:
			SET_TX_DESC_SEC_TYPE_8188E(pDesc, 0x0);
			break;
		case _WEP40_:
		case _WEP104_:
		case _TKIP_:
			SET_TX_DESC_SEC_TYPE_8188E(pDesc, 0x1);
			break;
		case _SMS4_:
			SET_TX_DESC_SEC_TYPE_8188E(pDesc, 0x2);
			break;
		case _AES_:
			SET_TX_DESC_SEC_TYPE_8188E(pDesc, 0x3);
			break;
		default:
			SET_TX_DESC_SEC_TYPE_8188E(pDesc, 0x0);
			break;
		}
	}

	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);  /* own, bFirstSeg, bLastSeg; */
}

static s32 update_txdesc(struct xmit_frame *pxmitframe, u8 *pmem, s32 sz)
{
	uint	qsel;
	bool sgi = 0;
	u8	data_rate = 0, pwr_status;
	dma_addr_t	mapping;
	_adapter				*padapter = pxmitframe->padapter;
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct dvobj_priv		*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pkt_attrib		*pattrib = &pxmitframe->attrib;
	struct ht_priv			*phtpriv = &pmlmepriv->htpriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(padapter);
	struct tx_desc		*ptxdesc = (struct tx_desc *)pmem;
	sint	bmcst = IS_MCAST(pattrib->ra);
#ifdef CONFIG_P2P
	struct wifidirect_info	*pwdinfo = &padapter->wdinfo;
#endif /* CONFIG_P2P */


	mapping = pci_map_single(pdvobjpriv->ppcidev, pxmitframe->buf_addr , sz, PCI_DMA_TODEVICE);

	_rtw_memset(ptxdesc, 0, TX_DESC_NEXT_DESC_OFFSET);

	if ((pxmitframe->frame_tag & 0x0f) == DATA_FRAMETAG) {
		/* RTW_INFO("pxmitframe->frame_tag == DATA_FRAMETAG\n");			 */

		/* offset 4 */
		ptxdesc->txdw1 |= cpu_to_le32((pattrib->mac_id) & 0x1f);

		qsel = (uint)(pattrib->qsel & 0x0000001f);
		ptxdesc->txdw1 |= cpu_to_le32((qsel << QSEL_SHT) & 0x00001f00);

		ptxdesc->txdw1 |= cpu_to_le32((pattrib->raid << 16) & 0x000f0000);

		fill_txdesc_sectype(pattrib, ptxdesc);

#if defined(CONFIG_CONCURRENT_MODE)
		if (bmcst)
			fill_txdesc_force_bmc_camid(pattrib, ptxdesc);
#endif


		/* offset 8 */
		if (pattrib->ampdu_en == _TRUE) {
			ptxdesc->txdw2 |= cpu_to_le32(AGG_EN);/* AGG EN */
			ptxdesc->txdw2 |= (pattrib->ampdu_spacing << AMPDU_DENSITY_SHT) & 0x00700000;
		} else {
			ptxdesc->txdw2 |= cpu_to_le32(AGG_BK);/* AGG BK */
		}

		/* offset 12 */
		ptxdesc->txdw3 |= cpu_to_le32((pattrib->seqnum & 0xFFF) << SEQ_SHT);

		/* offset 16 , offset 20 */
		if (pattrib->qos_en)
			ptxdesc->txdw4 |= cpu_to_le32(BIT(6));/* QoS */

		if ((pattrib->ether_type != 0x888e) &&
		    (pattrib->ether_type != 0x0806) &&
		    (pattrib->dhcp_pkt != 1)) {
			/* Non EAP & ARP & DHCP type data packet */

			fill_txdesc_vcs(pattrib, &ptxdesc->txdw4);
			fill_txdesc_phy(pattrib, &ptxdesc->txdw4);

			ptxdesc->txdw4 |= cpu_to_le32(0x00000008);/* RTS Rate=24M */
			ptxdesc->txdw5 |= cpu_to_le32(0x0001ff00);
			/* ptxdesc->txdw5 |= cpu_to_le32(0x0000000b); */ /* DataRate - 54M */

#if (RATE_ADAPTIVE_SUPPORT == 1)
			if (pHalData->fw_ractrl == _FALSE) {
				/* driver-based RA*/
				/* offset 16 */
				ptxdesc->txdw4 |= cpu_to_le32(USERATE);/* driver uses rate	 */

				if (pattrib->ht_en)
					sgi = odm_ra_get_sgi_8188e(&pHalData->odmpriv, pattrib->mac_id);

				data_rate = odm_ra_get_decision_rate_8188e(&pHalData->odmpriv, pattrib->mac_id);

#if (POWER_TRAINING_ACTIVE == 1)
				pwr_status = odm_ra_get_hw_pwr_status_8188e(&pHalData->odmpriv, pattrib->mac_id);
				ptxdesc->txdw4 |= cpu_to_le32((pwr_status & 0x7) << PWR_STATUS_SHT);
#endif
			} else
#endif	/* #if (RATE_ADAPTIVE_SUPPORT == 1) */
			{/* FW-based RA, TODO */
				if (pattrib->ht_en)
					sgi = 1;

				data_rate = 0x13; /* default rate: MCS7 */
			}

			if (bmcst) {
				data_rate = MRateToHwRate(pattrib->rate);
				ptxdesc->txdw4 |= cpu_to_le32(USERATE);
				ptxdesc->txdw4 |= cpu_to_le32(DISDATAFB);
			}
			if (padapter->fix_rate != 0xFF) {
				data_rate = padapter->fix_rate;
				ptxdesc->txdw4 |= cpu_to_le32(USERATE);
				if (!padapter->data_fb)
					ptxdesc->txdw4 |= cpu_to_le32(DISDATAFB);
				sgi = (padapter->fix_rate & BIT(7)) ? 1 : 0;
			}

			if (sgi)
				ptxdesc->txdw5 |= cpu_to_le32(SGI);

			ptxdesc->txdw5 |= cpu_to_le32(data_rate & 0x3F);
		} else {
			/* EAP data packet and ARP packet and DHCP. */
			/* Use the 1M data rate to send the EAP/ARP packet. */
			/* This will maybe make the handshake smooth. */
			/* offset 16 */
			ptxdesc->txdw4 |= cpu_to_le32(USERATE);/* driver uses rate	 */
			ptxdesc->txdw2 |= cpu_to_le32(AGG_BK);/* AGG BK		   */

			if (pmlmeinfo->preamble_mode == PREAMBLE_SHORT)
				ptxdesc->txdw4 |= cpu_to_le32(BIT(24));/* DATA_SHORT */

			ptxdesc->txdw5 |= cpu_to_le32(MRateToHwRate(pmlmeext->tx_rate));
		}
#ifdef CONFIG_TCP_CSUM_OFFLOAD_TX
		/* offset 24 */
		if (pattrib->hw_tcp_csum == 1) {
			/* ptxdesc->txdw6 = 0; */ /* clear TCP_CHECKSUM and IP_CHECKSUM. It's zero already!! */
			u8 ip_hdr_offset = 32 + pattrib->hdrlen + pattrib->iv_len + 8;
			ptxdesc->txdw7 = (1 << 31) | (ip_hdr_offset << 16);
			RTW_INFO("ptxdesc->txdw7 = %08x\n", ptxdesc->txdw7);
		}
#endif

#ifdef CONFIG_TDLS
#ifdef CONFIG_XMIT_ACK
		/* CCX-TXRPT ack for xmit mgmt frames. */
		if (pxmitframe->ack_report) {
#ifdef DBG_CCX
			static u16 ccx_sw = 0x123;

			ptxdesc->txdw7 |= cpu_to_le32(((ccx_sw) << 16) & 0x0fff0000);
			RTW_INFO("%s set ccx, sw:0x%03x\n", __func__, ccx_sw);
			ccx_sw = (ccx_sw + 1) % 0xfff;
#endif
			ptxdesc->txdw2 |= cpu_to_le32(BIT(19));
		}
#endif /* CONFIG_XMIT_ACK */
#endif
	} else if ((pxmitframe->frame_tag & 0x0f) == MGNT_FRAMETAG) {
		/* RTW_INFO("pxmitframe->frame_tag == MGNT_FRAMETAG\n");	 */
		/* offset 16 */
		ptxdesc->txdw4 |= cpu_to_le32(USERATE);/* driver uses rate	 */
		/* offset 4		 */
		ptxdesc->txdw1 |= cpu_to_le32(pattrib->mac_id & 0x3f);

		qsel = (uint)(pattrib->qsel & 0x0000001f);
		ptxdesc->txdw1 |= cpu_to_le32((qsel << QSEL_SHT) & 0x00001f00);

		ptxdesc->txdw1 |= cpu_to_le32((pattrib->raid << RATE_ID_SHT) & 0x000f0000);

		/* fill_txdesc_sectype(pattrib, ptxdesc); */

		/* offset 8		 */

#ifdef CONFIG_XMIT_ACK
		/* CCX-TXRPT ack for xmit mgmt frames. */
		if (pxmitframe->ack_report) {
#ifdef DBG_CCX
			static u16 ccx_sw = 0x123;

			ptxdesc->txdw7 |= cpu_to_le32(((ccx_sw) << 16) & 0x0fff0000);
			RTW_INFO("%s set ccx, sw:0x%03x\n", __func__, ccx_sw);
			ccx_sw = (ccx_sw + 1) % 0xfff;
#endif
			ptxdesc->txdw2 |= cpu_to_le32(BIT(19));
		}
#endif /* CONFIG_XMIT_ACK */

		/* offset 12 */
		ptxdesc->txdw3 |= cpu_to_le32((pattrib->seqnum << SEQ_SHT) & 0x0fff0000);

		/* offset 20 */
		ptxdesc->txdw5 |= cpu_to_le32(RTY_LMT_EN);/* retry limit enable */
		if (pattrib->retry_ctrl == _TRUE)
			ptxdesc->txdw5 |= cpu_to_le32(0x00180000);/* retry limit = 6 */
		else
			ptxdesc->txdw5 |= cpu_to_le32(0x00300000);/* retry limit = 12 */

		ptxdesc->txdw5 |= cpu_to_le32(MRateToHwRate(pattrib->rate));

	} else if ((pxmitframe->frame_tag & 0x0f) == TXAGG_FRAMETAG)
		RTW_INFO("pxmitframe->frame_tag == TXAGG_FRAMETAG\n");
#ifdef CONFIG_MP_INCLUDED
	else if (((pxmitframe->frame_tag & 0x0f) == MP_FRAMETAG) &&
		 (padapter->registrypriv.mp_mode == 1))
		fill_txdesc_for_mp(padapter, (u8 *)ptxdesc);
#endif
	else {
		RTW_INFO("pxmitframe->frame_tag = %d\n", pxmitframe->frame_tag);

		/* offset 4 */
		ptxdesc->txdw1 |= cpu_to_le32((4) & 0x3f);	/* CAM_ID(MAC_ID) */
		ptxdesc->txdw1 |= cpu_to_le32((6 << RATE_ID_SHT) & 0x000f0000); /* raid */

		/* offset 12 */
		ptxdesc->txdw3 |= cpu_to_le32((pattrib->seqnum << RATE_ID_SHT) & 0x0fff0000);

		/* offset 16 */
		ptxdesc->txdw4 |= cpu_to_le32(USERATE);/* driver uses rate	 */

		/* offset 20 */
		ptxdesc->txdw5 |= cpu_to_le32(MRateToHwRate(pattrib->rate));
	}

#ifdef CONFIG_ANTENNA_DIVERSITY
	if (!bmcst && pattrib->psta)
		odm_set_tx_ant_by_tx_info(adapter_to_phydm(padapter), pmem, pattrib->psta->cmn.mac_id);
#endif

	/* offset 28 */
	ptxdesc->txdw7 |= cpu_to_le32(sz & 0x0000ffff);

	/* offset 32 */
	ptxdesc->txdw8 = cpu_to_le32(mapping);

	/* 2009.11.05. tynli_test. Suggested by SD4 Filen for FW LPS. */
	/* (1) The sequence number of each non-Qos frame / broadcast / multicast / */
	/* mgnt frame should be controled by Hw because Fw will also send null data */
	/* which we cannot control when Fw LPS enable. */
	/* --> default enable non-Qos data sequense number. 2010.06.23. by tynli. */
	/* (2) Enable HW SEQ control for beacon packet, because we use Hw beacon. */
	/* (3) Use HW Qos SEQ to control the seq num of Ext port non-Qos packets. */
	/* 2010.06.23. Added by tynli. */
	if (!pattrib->qos_en) {
		/* ptxdesc->txdw4 |= cpu_to_le32(BIT(7)); */ /* Hw set sequence number */
		/* ptxdesc->txdw3 |= cpu_to_le32((8 <<28)); */ /* set bit3 to 1. Suugested by TimChen. 2009.12.29. */

		ptxdesc->txdw3 |= cpu_to_le32(EN_HWSEQ); /* Hw set sequence number */
		ptxdesc->txdw4 |= cpu_to_le32(HW_SSN);	/* Hw set sequence number */
	}

	/* offset 0 */
	if (bmcst)
		ptxdesc->txdw0 |= cpu_to_le32(BIT(24));
	ptxdesc->txdw0 |= cpu_to_le32(sz & 0x0000ffff);
	ptxdesc->txdw0 |= cpu_to_le32(FSG | LSG);
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE + OFFSET_SZ) << OFFSET_SHT) & 0x00ff0000);/* 32 bytes for TX Desc
 *	ptxdesc->txdw0 |= cpu_to_le32(OWN); */

	return 0;

}


static inline struct tx_desc *get_txdesc(struct rtw_tx_ring *ring)
{
	if (ring->qid == BCN_QUEUE_INX)
		return &ring->desc[0];

	if (ring->qlen == ring->entries)
		return NULL;

	return &ring->desc[(ring->idx + ring->qlen) % ring->entries];
}

#ifdef CONFIG_IOL_IOREG_CFG_DBG
	#include <rtw_iol.h>
#endif
s32 rtw_dump_xframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	s32 ret = _SUCCESS;
	s32 inner_ret = _SUCCESS;
	_irqL irqL;
	int	t, sz, w_sz, pull = 0;
	/* u8	*mem_addr; */
	u32	ff_hwaddr;
	struct xmit_buf		*pxmitbuf = pxmitframe->pxmitbuf;
	struct pkt_attrib		*pattrib = &pxmitframe->attrib;
	struct xmit_priv		*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv		*pdvobjpriv = adapter_to_dvobj(padapter);
	struct security_priv		*psecuritypriv = &padapter->securitypriv;
	struct tx_desc			*ptxdesc;
	struct rtw_tx_ring		*ptx_ring = GET_PRIMARY_ADAPTER(padapter)->xmitpriv.tx_ring;
	struct rtw_tx_ring		*ring;

	DBG_COUNTER(padapter->tx_logs.intf_tx_dump_xframe);

	if ((pxmitframe->frame_tag == DATA_FRAMETAG) &&
	    (pxmitframe->attrib.ether_type != 0x0806) &&
	    (pxmitframe->attrib.ether_type != 0x888e) &&
	    (pxmitframe->attrib.dhcp_pkt != 1))
		rtw_issue_addbareq_cmd(padapter, pxmitframe);

	/* mem_addr = pxmitframe->buf_addr; */


	for (t = 0; t < pattrib->nr_frags; t++) {
		if (inner_ret != _SUCCESS && ret == _SUCCESS)
			ret = _FAIL;

		if (t != (pattrib->nr_frags - 1)) {

			sz = pxmitpriv->frag_len;
			sz = sz - 4 - (psecuritypriv->sw_encrypt ? 0 : pattrib->icv_len);
		} else /* no frag */
			sz = pattrib->last_txcmdsz;

		ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);

		ring = &ptx_ring[ff_hwaddr];

		_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);

		ptxdesc = get_txdesc(ring);
		if (ptxdesc == NULL) {
			_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);
			rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_TX_DESC_NA);
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
			RTW_INFO("##### Tx desc unavailable !#####\n");
			DBG_COUNTER(padapter->tx_logs.intf_tx_dump_xframe_err_txdesc);
			break;
		}
		update_txdesc(pxmitframe, (u8 *)ptxdesc, sz);

		if (BCN_QUEUE_INX == ff_hwaddr) {
			/* beacon hw queue is special, use first ring entry always */
			/* RTW_INFO("BCN TX!\n"); */
		} else if (pxmitbuf->buf_tag == XMITBUF_CMD)
			RTW_INFO("%s: CMD shall be BCN_QUEUE_INX always!", __func__);
		else {
			pxmitbuf->desc = ptxdesc;
			rtw_list_delete(&pxmitbuf->list);
			rtw_list_insert_tail(&pxmitbuf->list, &ring->queue.queue);
			ring->qlen++;
		}

		pxmitbuf->len = sz;
		w_sz = sz;

		wmb();
		ptxdesc->txdw0 |= cpu_to_le32(OWN);

		_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);
#ifdef CONFIG_IOL_IOREG_CFG_DBG
		rtw_IOL_cmd_buf_dump(padapter, w_sz, pxmitframe->buf_addr);
#endif
		rtw_write16(padapter, REG_PCIE_CTRL_REG, ffaddr2dma(ff_hwaddr));
		inner_ret = rtw_write_port(padapter, ff_hwaddr, w_sz, (unsigned char *)pxmitbuf);
		if (inner_ret != _SUCCESS)
			DBG_COUNTER(padapter->tx_logs.intf_tx_dump_xframe_err_port);

		rtw_count_tx_stats(padapter, pxmitframe, sz);
		/* RTW_INFO("rtw_write_port, w_sz=%d, sz=%d, txdesc_sz=%d, tid=%d\n", w_sz, sz, w_sz-sz, pattrib->priority);       */

		/* mem_addr += w_sz; */

		/* mem_addr = (u8 *)RND4(((SIZE_PTR)(mem_addr))); */

	}

	rtw_free_xmitframe(pxmitpriv, pxmitframe);

	if (ret != _SUCCESS)
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_UNKNOWN);

	return ret;
}

#ifdef CONFIG_TX_AMSDU
static s32 xmitframe_amsdu_direct(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	struct xmit_buf *pxmitbuf = pxmitframe->pxmitbuf;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	s32 res = _SUCCESS;

	res = rtw_xmitframe_coalesce_amsdu(padapter, pxmitframe, NULL);

	if (res == _SUCCESS) {
#ifdef CONFIG_XMIT_THREAD_MODE
		enqueue_pending_xmitbuf(pxmitpriv, pxmitframe->pxmitbuf);
#else
		res = rtw_dump_xframe(padapter, pxmitframe);
#endif
	} else {
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
	}

	return res;
}
#endif

void rtl8188ee_xmitframe_resume(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame *pxmitframe = NULL;
	struct xmit_buf	*pxmitbuf = NULL;
	int res = _SUCCESS, xcnt = 0;

#ifdef CONFIG_TX_AMSDU
	struct mlme_priv *pmlmepriv =  &padapter->mlmepriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	int tx_amsdu = padapter->tx_amsdu;
	int tx_amsdu_rate = padapter->tx_amsdu_rate;
	int current_tx_rate = pdvobjpriv->traffic_stat.cur_tx_tp;

	struct pkt_attrib *pattrib = NULL;

	struct xmit_frame *pxmitframe_next = NULL;
	struct xmit_buf *pxmitbuf_next = NULL;
	struct pkt_attrib *pattrib_next = NULL;
	int num_frame = 0;

	u8 amsdu_timeout = 0;
#endif

	while (1) {
		if (RTW_CANNOT_RUN(padapter)) {
			RTW_INFO("rtl8188ee_xmitframe_resume => bDriverStopped or bSurpriseRemoved\n");
			break;
		}

		pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
		if (!pxmitbuf)
			break;

#ifdef CONFIG_TX_AMSDU
		if (tx_amsdu == 0)
			goto dump_pkt;

		if (!check_fwstate(pmlmepriv, WIFI_STATION_STATE))
			goto dump_pkt;

		pxmitframe = rtw_get_xframe(pxmitpriv, &num_frame);

		if (num_frame == 0 || pxmitframe == NULL || !check_amsdu(pxmitframe))
			goto dump_pkt;

		pattrib = &pxmitframe->attrib;

		if (tx_amsdu == 1) {
			pxmitframe =  rtw_dequeue_xframe(pxmitpriv, pxmitpriv->hwxmits,
						pxmitpriv->hwxmit_entry);
			if (pxmitframe) {
				pxmitframe->pxmitbuf = pxmitbuf;
				pxmitframe->buf_addr = pxmitbuf->pbuf;
				pxmitbuf->priv_data = pxmitframe;
				xmitframe_amsdu_direct(padapter, pxmitframe);
				pxmitpriv->amsdu_debug_coalesce_one++;
				continue;
			} else {
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				break;
			}
		} else if (tx_amsdu == 2 && ((tx_amsdu_rate == 0) || (current_tx_rate > tx_amsdu_rate))) {

			if (num_frame == 1) {
				amsdu_timeout = rtw_amsdu_get_timer_status(padapter, pattrib->priority);

				if (amsdu_timeout == RTW_AMSDU_TIMER_UNSET) {
					rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
					rtw_amsdu_set_timer_status(padapter,
						pattrib->priority, RTW_AMSDU_TIMER_SETTING);
					rtw_amsdu_set_timer(padapter, pattrib->priority);
					pxmitpriv->amsdu_debug_set_timer++;
					break;
				} else if (amsdu_timeout == RTW_AMSDU_TIMER_SETTING) {
					rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
					break;
				} else if (amsdu_timeout == RTW_AMSDU_TIMER_TIMEOUT) {
					rtw_amsdu_set_timer_status(padapter,
						pattrib->priority, RTW_AMSDU_TIMER_UNSET);
					pxmitpriv->amsdu_debug_timeout++;
					pxmitframe = rtw_dequeue_xframe(pxmitpriv,
						pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);
					if (pxmitframe) {
						pxmitframe->pxmitbuf = pxmitbuf;
						pxmitframe->buf_addr = pxmitbuf->pbuf;
						pxmitbuf->priv_data = pxmitframe;
						xmitframe_amsdu_direct(padapter, pxmitframe);
					} else {
						rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
					}
					break;
				}
			} else/* num_frame > 1*/{
				pxmitframe = rtw_dequeue_xframe(pxmitpriv,
					pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);

				if (!pxmitframe) {
					rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
					break;
				}

				pxmitframe->pxmitbuf = pxmitbuf;
				pxmitframe->buf_addr = pxmitbuf->pbuf;
				pxmitbuf->priv_data = pxmitframe;

				pxmitframe_next = rtw_get_xframe(pxmitpriv, &num_frame);

				if (num_frame == 0) {
					xmitframe_amsdu_direct(padapter, pxmitframe);
					pxmitpriv->amsdu_debug_coalesce_one++;
					break;
				}

				if (!check_amsdu(pxmitframe_next)) {
					xmitframe_amsdu_direct(padapter, pxmitframe);
					pxmitpriv->amsdu_debug_coalesce_one++;
					continue;
				} else {
					pxmitbuf_next = rtw_alloc_xmitbuf(pxmitpriv);
					if (!pxmitbuf_next) {
						xmitframe_amsdu_direct(padapter, pxmitframe);
						pxmitpriv->amsdu_debug_coalesce_one++;
						continue;
					}

					pxmitframe_next = rtw_dequeue_xframe(pxmitpriv,
						pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);
					if (!pxmitframe_next) {
						rtw_free_xmitbuf(pxmitpriv, pxmitbuf_next);
						xmitframe_amsdu_direct(padapter, pxmitframe);
						pxmitpriv->amsdu_debug_coalesce_one++;
						continue;
					}

					pxmitframe_next->pxmitbuf = pxmitbuf_next;
					pxmitframe_next->buf_addr = pxmitbuf_next->pbuf;
					pxmitbuf_next->priv_data = pxmitframe_next;

					rtw_xmitframe_coalesce_amsdu(padapter,
						pxmitframe_next, pxmitframe);
					rtw_free_xmitframe(pxmitpriv, pxmitframe);
					rtw_free_xmitbuf(pxmitpriv, pxmitbuf);

#ifdef CONFIG_XMIT_THREAD_MODE
					enqueue_pending_xmitbuf(pxmitpriv, pxmitframe_next->pxmitbuf);
#else
					rtw_dump_xframe(padapter, pxmitframe_next);
#endif
					pxmitpriv->amsdu_debug_coalesce_two++;

					continue;
				}

			}

		}
dump_pkt:
#endif /* CONFIG_TX_AMSDU */

		pxmitframe =  rtw_dequeue_xframe(pxmitpriv, pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);

		if (pxmitframe) {
			pxmitframe->pxmitbuf = pxmitbuf;

			pxmitframe->buf_addr = pxmitbuf->pbuf;

			pxmitbuf->priv_data = pxmitframe;

			res = _SUCCESS;

			if ((pxmitframe->frame_tag & 0x0f) == DATA_FRAMETAG) {
				if (pxmitframe->attrib.priority <= 15) /* TID0~15 */
					res = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);

				rtw_os_xmit_complete(padapter, pxmitframe);/* always return ndis_packet after rtw_xmitframe_coalesce			 */
			}

			DBG_COUNTER(padapter->tx_logs.intf_tx_dequeue);

			if (res == _SUCCESS)
				rtw_dump_xframe(padapter, pxmitframe);
			else {
				DBG_COUNTER(padapter->tx_logs.intf_tx_dequeue_err_coalesce);
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				rtw_free_xmitframe(pxmitpriv, pxmitframe);
			}

			xcnt++;
		} else {
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
			break;
		}
	}
}

static u8 check_nic_enough_desc(_adapter *padapter, struct pkt_attrib *pattrib)
{
	u32 prio;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct rtw_tx_ring	*ring;

	switch (pattrib->qsel) {
	case 0:
	case 3:
		prio = BE_QUEUE_INX;
		break;
	case 1:
	case 2:
		prio = BK_QUEUE_INX;
		break;
	case 4:
	case 5:
		prio = VI_QUEUE_INX;
		break;
	case 6:
	case 7:
		prio = VO_QUEUE_INX;
		break;
	default:
		prio = BE_QUEUE_INX;
		break;
	}

	ring = &pxmitpriv->tx_ring[prio];

	/* for now we reserve two free descriptor as a safety boundary */
	/* between the tail and the head */
	/*  */
	if ((ring->entries - ring->qlen) >= 2)
		return _TRUE;
	else {
		RTW_INFO("do not have enough desc for Tx\n");
		return _FALSE;
	}
}

static s32 xmitframe_direct(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	s32 res = _SUCCESS;

	DBG_COUNTER(padapter->tx_logs.intf_tx_direct);
	res = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
	if (res == _SUCCESS)
		rtw_dump_xframe(padapter, pxmitframe);
	else
		DBG_COUNTER(padapter->tx_logs.intf_tx_direct_err_coalesce);

	return res;
}

/*
 * Return
 *	_TRUE	dump packet directly
 *	_FALSE	enqueue packet
 */
static s32 pre_xmitframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	_irqL irqL;
	s32 res;
	struct xmit_buf *pxmitbuf = NULL;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

#ifdef CONFIG_TX_AMSDU
	int tx_amsdu = padapter->tx_amsdu;
	u8 amsdu_timeout = 0;
#endif

	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	if ((rtw_txframes_sta_ac_pending(padapter, pattrib) > 0) ||
	    (check_nic_enough_desc(padapter, pattrib) == _FALSE)) {
		DBG_COUNTER(padapter->tx_logs.intf_tx_pending_ac);
		goto enqueue;
	}

	if (rtw_xmit_ac_blocked(padapter) == _TRUE) {
		DBG_COUNTER(padapter->tx_logs.intf_tx_pending_fw_under_survey);
		goto enqueue;
	}

#ifdef CONFIG_TX_AMSDU
	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE) &&
		check_amsdu_tx_support(padapter)) {

		if (IS_AMSDU_AMPDU_VALID(pattrib))
			goto enqueue;
	}
#endif

	if (DEV_STA_LG_NUM(padapter->dvobj)) {
		DBG_COUNTER(padapter->tx_logs.intf_tx_pending_fw_under_linking);
		goto enqueue;
	}

	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	if (pxmitbuf == NULL) {
		DBG_COUNTER(padapter->tx_logs.intf_tx_pending_xmitbuf);
		goto enqueue;
	}

	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	pxmitframe->pxmitbuf = pxmitbuf;
	pxmitframe->buf_addr = pxmitbuf->pbuf;
	pxmitbuf->priv_data = pxmitframe;

	if (xmitframe_direct(padapter, pxmitframe) != _SUCCESS) {
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
	}

	return _TRUE;

enqueue:
	res = rtw_xmitframe_enqueue(padapter, pxmitframe);

#ifdef CONFIG_TX_AMSDU
	if (res == _SUCCESS && tx_amsdu == 2) {
		amsdu_timeout = rtw_amsdu_get_timer_status(padapter, pattrib->priority);
		if (amsdu_timeout == RTW_AMSDU_TIMER_SETTING) {
			rtw_amsdu_cancel_timer(padapter, pattrib->priority);
			rtw_amsdu_set_timer_status(padapter, pattrib->priority,
				RTW_AMSDU_TIMER_UNSET);
		}
	}
#endif

	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	if (res != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		pxmitpriv->tx_drop++;
		return _TRUE;
	}

#ifdef CONFIG_TX_AMSDU
	tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#endif
	return _FALSE;
}

s32 rtl8188ee_mgnt_xmit(_adapter *padapter, struct xmit_frame *pmgntframe)
{
	return rtw_dump_xframe(padapter, pmgntframe);
}

/*
 * Return
 *	_TRUE	dump packet directly ok
 *	_FALSE	temporary can't transmit packets to hardware
 */
s32 rtl8188ee_hal_xmit(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	DBG_COUNTER(padapter->tx_logs.intf_tx);
	return pre_xmitframe(padapter, pxmitframe);
}

s32	rtl8188ee_hal_xmitframe_enqueue(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	s32 err;

	DBG_COUNTER(padapter->tx_logs.intf_tx_enqueue);

	err = rtw_xmitframe_enqueue(padapter, pxmitframe);
	if (err != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		/* Trick, make the statistics correct */
		pxmitpriv->tx_pkts--;
		pxmitpriv->tx_drop++;
	} else {
#ifdef PLATFORM_LINUX
		tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#endif
	}

	return err;

}

#ifdef CONFIG_HOSTAPD_MLME

static void rtl8188ee_hostap_mgnt_xmit_cb(struct urb *urb)
{
#ifdef PLATFORM_LINUX
	struct sk_buff *skb = (struct sk_buff *)urb->context;

	/* RTW_INFO("%s\n", __FUNCTION__); */

	rtw_skb_free(skb);
#endif
}

s32 rtl8188ee_hostap_mgnt_xmit_entry(_adapter *padapter, _pkt *pkt)
{
#ifdef PLATFORM_LINUX
	u16 fc;
	int rc, len, pipe;
	unsigned int bmcst, tid, qsel;
	struct sk_buff *skb, *pxmit_skb;
	struct urb *urb;
	unsigned char *pxmitbuf;
	struct tx_desc *ptxdesc;
	struct rtw_ieee80211_hdr *tx_hdr;
	struct hostapd_priv *phostapdpriv = padapter->phostapdpriv;
	struct net_device *pnetdev = padapter->pnetdev;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv *pdvobj = adapter_to_dvobj(padapter);


	/* RTW_INFO("%s\n", __FUNCTION__); */

	skb = pkt;

	len = skb->len;
	tx_hdr = (struct rtw_ieee80211_hdr *)(skb->data);
	fc = le16_to_cpu(tx_hdr->frame_ctl);
	bmcst = IS_MCAST(tx_hdr->addr1);

	if ((fc & RTW_IEEE80211_FCTL_FTYPE) != RTW_IEEE80211_FTYPE_MGMT)
		goto _exit;

	pxmit_skb = rtw_skb_alloc(len + TXDESC_SIZE);

	if (!pxmit_skb)
		goto _exit;

	pxmitbuf = pxmit_skb->data;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		goto _exit;

	/* ----- fill tx desc -----	 */
	ptxdesc = (struct tx_desc *)pxmitbuf;
	_rtw_memset(ptxdesc, 0, sizeof(*ptxdesc));

	/* offset 0	 */
	ptxdesc->txdw0 |= cpu_to_le32(len & 0x0000ffff);
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE + OFFSET_SZ) << OFFSET_SHT) & 0x00ff0000); /* default = 32 bytes for TX Desc */
	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);

	if (bmcst)
		ptxdesc->txdw0 |= cpu_to_le32(BIT(24));

	/* offset 4	 */
	ptxdesc->txdw1 |= cpu_to_le32(0x00);/* MAC_ID */

	ptxdesc->txdw1 |= cpu_to_le32((0x12 << QSEL_SHT) & 0x00001f00);

	ptxdesc->txdw1 |= cpu_to_le32((0x06 << 16) & 0x000f0000); /* b mode */

	/* offset 8			 */

	/* offset 12		 */
	ptxdesc->txdw3 |= cpu_to_le32((le16_to_cpu(tx_hdr->seq_ctl) << 16) & 0xffff0000);

	/* offset 16		 */
	ptxdesc->txdw4 |= cpu_to_le32(BIT(8));/* driver uses rate */

	/* offset 20 */

	rtl8188e_cal_txdesc_chksum(ptxdesc);
	/* ----- end of fill tx desc ----- */

	/*  */
	skb_put(pxmit_skb, len + TXDESC_SIZE);
	pxmitbuf = pxmitbuf + TXDESC_SIZE;
	_rtw_memcpy(pxmitbuf, skb->data, len);

	/* RTW_INFO("mgnt_xmit, len=%x\n", pxmit_skb->len); */


	/* ----- prepare urb for submit ----- */

	/* translate DMA FIFO addr to pipehandle */
	/* pipe = ffaddr2pipehdl(pdvobj, MGT_QUEUE_INX); */
	pipe = usb_sndbulkpipe(pdvobj->pusbdev, pHalData->Queue2EPNum[(u8)MGT_QUEUE_INX] & 0x0f);

	usb_fill_bulk_urb(urb, pdvobj->pusbdev, pipe,
		pxmit_skb->data, pxmit_skb->len, rtl8188ee_hostap_mgnt_xmit_cb, pxmit_skb);

	urb->transfer_flags |= URB_ZERO_PACKET;
	usb_anchor_urb(urb, &phostapdpriv->anchored);
	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc < 0) {
		usb_unanchor_urb(urb);
		kfree_skb(skb);
	}
	usb_free_urb(urb);


_exit:

	rtw_skb_free(skb);

#endif

	return 0;

}
#endif
