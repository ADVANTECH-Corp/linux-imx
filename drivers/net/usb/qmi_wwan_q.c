/*
 * Copyright (c) 2012  Bjørn Mork <bjorn@mork.no>
 *
 * The probing code is heavily inspired by cdc_ether, which is:
 * Copyright (C) 2003-2005 by David Brownell
 * Copyright (C) 2006 by Ole Andre Vadla Ravnas (ActiveSync)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/usb/usbnet.h>
#include <linux/usb/cdc-wdm.h>

/* This driver supports wwan (3G/LTE/?) devices using a vendor
 * specific management protocol called Qualcomm MSM Interface (QMI) -
 * in addition to the more common AT commands over serial interface
 * management
 *
 * QMI is wrapped in CDC, using CDC encapsulated commands on the
 * control ("master") interface of a two-interface CDC Union
 * resembling standard CDC ECM.  The devices do not use the control
 * interface for any other CDC messages.  Most likely because the
 * management protocol is used in place of the standard CDC
 * notifications NOTIFY_NETWORK_CONNECTION and NOTIFY_SPEED_CHANGE
 *
 * Alternatively, control and data functions can be combined in a
 * single USB interface.
 *
 * Handling a protocol like QMI is out of the scope for any driver.
 * It is exported as a character device using the cdc-wdm driver as
 * a subdriver, enabling userspace applications ("modem managers") to
 * handle it.
 *
 * These devices may alternatively/additionally be configured using AT
 * commands on a serial interface
 */

/* driver specific data */
struct qmi_wwan_state {
	struct usb_driver *subdriver;
	atomic_t pmcount;
	unsigned long unused;
	struct usb_interface *control;
	struct usb_interface *data;
};

/* default ethernet address used by the modem */
static const u8 default_modem_addr[ETH_ALEN] = {0x02, 0x50, 0xf3};

#if 1 //Added by Quectel
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/ipv6.h>

/*
    Quectel_WCDMA&LTE_Linux_USB_Driver_User_Guide_V1.9.pdf
    5.6.	Test QMAP on GobiNet or QMI WWAN
    0 - no QMAP
    1 - QMAP (Aggregation protocol)
    X - QMAP (Multiplexing and Aggregation protocol)
*/
#define QUECTEL_WWAN_QMAP 0
#ifdef CONFIG_BRIDGE
//#define QUECTEL_BRIDGE_MODE
#endif

#ifdef QUECTEL_BRIDGE_MODE
static int __read_mostly bridge_mode = 0;
module_param( bridge_mode, int, S_IRUGO );
static uint __read_mostly bridge_ipv4 = 0;
module_param( bridge_ipv4, int, S_IRUGO | S_IWUSR );
static unsigned char bridge_mac[ETH_ALEN];

static int bridge_arp_reply(struct usbnet *dev, struct sk_buff *skb) {
    struct net_device *net = dev->net;
    struct arphdr *parp;
    u8 *arpptr, *sha;
    u8  sip[4], tip[4], ipv4[4];
    struct sk_buff *reply = NULL;

    ipv4[0]  = (bridge_ipv4 >> 24) & 0xFF;
    ipv4[1]  = (bridge_ipv4 >> 16) & 0xFF;
    ipv4[2]  = (bridge_ipv4 >> 8) & 0xFF;
    ipv4[3]  = (bridge_ipv4 >> 0) & 0xFF;
        
    parp = arp_hdr(skb);

    if (parp->ar_hrd == htons(ARPHRD_ETHER)  && parp->ar_pro == htons(ETH_P_IP)
        && parp->ar_op == htons(ARPOP_REQUEST) && parp->ar_hln == 6 && parp->ar_pln == 4) {
        arpptr = (u8 *)parp + sizeof(struct arphdr);
        sha = arpptr;
        arpptr += net->addr_len;	/* sha */
        memcpy(sip, arpptr, sizeof(sip));
        arpptr += sizeof(sip);
        arpptr += net->addr_len;	/* tha */
        memcpy(tip, arpptr, sizeof(tip));

        dev_info(&net->dev, "sip = %d.%d.%d.%d, tip=%d.%d.%d.%d, ipv4=%d.%d.%d.%d\n",
            sip[0], sip[1], sip[2], sip[3], tip[0], tip[1], tip[2], tip[3], ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
        if (tip[0] == ipv4[0] && tip[1] == ipv4[1] && tip[2] == ipv4[2] && tip[3] != ipv4[3])
            reply = arp_create(ARPOP_REPLY, ETH_P_ARP, *((__be32 *)sip), net, *((__be32 *)tip), sha, default_modem_addr, sha);

        if (reply) {
            skb_reset_mac_header(reply);
            __skb_pull(reply, skb_network_offset(reply));
            reply->ip_summed = CHECKSUM_UNNECESSARY;
            reply->pkt_type = PACKET_HOST;

            netif_rx_ni(reply);
        }
        return 1;
    }

    return 0;
}
#endif

#if defined(QUECTEL_WWAN_QMAP)
#define QUECTEL_QMAP_MUX_ID 0x81
static uint __read_mostly qmap_mode = QUECTEL_WWAN_QMAP;
module_param( qmap_mode, uint, S_IRUGO);
module_param_named( rx_qmap, qmap_mode, uint, S_IRUGO );

typedef struct sQmiWwanQmap
{
	struct usbnet *mpNetDev;
	atomic_t refcount;
	struct net_device *mpQmapNetDev[QUECTEL_WWAN_QMAP];
	int qmap_mode;
#ifdef QUECTEL_BRIDGE_MODE
	int bridge_mode;
	uint bridge_ipv4;
	unsigned char bridge_mac[ETH_ALEN];
#endif
} sQmiWwanQmap;

struct qmap_hdr {
    u8 cd_rsvd_pad;
    u8 mux_id;
    u16 pkt_len;
} __packed;

struct qmap_priv {
	struct net_device *real_dev;
	u8 offset_id;
};

static int qmap_open(struct net_device *dev)
{
	struct qmap_priv *priv = netdev_priv(dev);
	struct net_device *real_dev = priv->real_dev;

	if (!(priv->real_dev->flags & IFF_UP))
		return -ENETDOWN;

	if (netif_carrier_ok(real_dev))
		netif_carrier_on(dev);
	return 0;
}

static int qmap_stop(struct net_device *pNet)
{
	netif_carrier_off(pNet);
	return 0;
}

static int qmap_start_xmit(struct sk_buff *skb, struct net_device *pNet)
{
	int err;
	struct qmap_priv *priv = netdev_priv(pNet);
	unsigned int len;
	struct qmap_hdr *hdr;

	skb_reset_mac_header(skb);
	if (skb_pull(skb, ETH_HLEN) == NULL) {
	      dev_kfree_skb_any (skb);
	      return NETDEV_TX_OK;
   	}
   
	len = skb->len;
	hdr = (struct qmap_hdr *)skb_push(skb, sizeof(struct qmap_hdr));
	hdr->cd_rsvd_pad = 0;
	hdr->mux_id = QUECTEL_QMAP_MUX_ID + priv->offset_id;
	hdr->pkt_len = cpu_to_be16(len);

	skb->dev = priv->real_dev;
	err = dev_queue_xmit(skb);
	if (err == NET_XMIT_SUCCESS) {
		pNet->stats.tx_packets++;
		pNet->stats.tx_bytes += skb->len;
	} else {
		pNet->stats.tx_errors++;
	}

	return err;
}

static const struct net_device_ops qmap_netdev_ops = {
	.ndo_open       = qmap_open,
	.ndo_stop       = qmap_stop,
	.ndo_start_xmit = qmap_start_xmit,
};

static int qmap_register_device(sQmiWwanQmap * pDev, u8 offset_id)
{
    struct net_device *real_dev = pDev->mpNetDev->net;
    struct net_device *qmap_net;
    struct qmap_priv *priv;
    int err;

    qmap_net = alloc_etherdev(sizeof(*priv));
    if (!qmap_net)
        return -ENOBUFS;

    SET_NETDEV_DEV(qmap_net, &real_dev->dev);
    priv = netdev_priv(qmap_net);
    priv->offset_id = offset_id;
    priv->real_dev = real_dev;
    sprintf(qmap_net->name, "%s.%d", real_dev->name, offset_id + 1);
    qmap_net->netdev_ops = &qmap_netdev_ops;
    memcpy (qmap_net->dev_addr, real_dev->dev_addr, ETH_ALEN);

    err = register_netdev(qmap_net);
    if (err < 0)
        goto out_free_newdev;
    netif_device_attach (qmap_net);

    pDev->mpQmapNetDev[offset_id] = qmap_net;
    qmap_net->flags |= IFF_NOARP;

    dev_info(&real_dev->dev, "%s %s\n", __func__, qmap_net->name);

    return 0;

out_free_newdev:
    free_netdev(qmap_net);
    return err;
}

static void qmap_unregister_device(sQmiWwanQmap * pDev, u8 offset_id) {
    struct net_device *net = pDev->mpQmapNetDev[offset_id];
    if (net != NULL) {
        netif_carrier_off( net );
        unregister_netdev (net);
        free_netdev(net);
    }
}
#endif

static struct sk_buff *qmi_wwan_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags) {
	//MDM9x07,MDM9628,MDM9x40,SDX20,SDX24 only work on RAW IP mode
	if ((dev->driver_info->flags & FLAG_NOARP) == 0)
		return skb;

	// Skip Ethernet header from message
	if (dev->net->hard_header_len == 0)
		return skb;
	else
		skb_reset_mac_header(skb);

#ifdef QUECTEL_BRIDGE_MODE
	if (bridge_mode) {
		struct ethhdr *ehdr;
		const struct iphdr *iph;

		ehdr = eth_hdr(skb);

		if (ehdr->h_proto == htons(ETH_P_ARP)) {
			bridge_arp_reply(dev, skb);
			dev_kfree_skb_any(skb);
			return NULL;
		}

		iph = ip_hdr(skb);
		//DBG("iphdr: ");
		//PrintHex((void *)iph, sizeof(struct iphdr));

// 1	0.000000000	0.0.0.0	255.255.255.255	DHCP	362	DHCP Request  - Transaction ID 0xe7643ad7        
		if (ehdr->h_proto == htons(ETH_P_IP) && iph->protocol == IPPROTO_UDP && iph->saddr == 0x00000000 && iph->daddr == 0xFFFFFFFF) {
			//if (udp_hdr(skb)->dest == htons(67)) //DHCP Request
			{
				memcpy(bridge_mac, ehdr->h_source, ETH_ALEN);
				dev_info(&dev->net->dev, "PC Mac Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
					bridge_mac[0], bridge_mac[1], bridge_mac[2], bridge_mac[3], bridge_mac[4], bridge_mac[5]);
			}
		}
        
		if (memcmp(ehdr->h_source, bridge_mac, ETH_ALEN)) {
			dev_kfree_skb_any(skb);
			return NULL;
		}
	}  
#endif

	if (skb_pull(skb, ETH_HLEN)) {
		return skb;
	} else {
		dev_err(&dev->intf->dev,  "Packet Dropped ");
	}

	// Filter the packet out, release it
	dev_kfree_skb_any(skb);
	return NULL;
}
#endif

/* Make up an ethernet header if the packet doesn't have one.
 *
 * A firmware bug common among several devices cause them to send raw
 * IP packets under some circumstances.  There is no way for the
 * driver/host to know when this will happen.  And even when the bug
 * hits, some packets will still arrive with an intact header.
 *
 * The supported devices are only capably of sending IPv4, IPv6 and
 * ARP packets on a point-to-point link. Any packet with an ethernet
 * header will have either our address or a broadcast/multicast
 * address as destination.  ARP packets will always have a header.
 *
 * This means that this function will reliably add the appropriate
 * header iff necessary, provided our hardware address does not start
 * with 4 or 6.
 *
 * Another common firmware bug results in all packets being addressed
 * to 00:a0:c6:00:00:00 despite the host address being different.
 * This function will also fixup such packets.
 */
static int qmi_wwan_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	__be16 proto;

	/* This check is no longer done by usbnet */
	if (skb->len < dev->net->hard_header_len)
		return 0;

	switch (skb->data[0] & 0xf0) {
	case 0x40:
		proto = htons(ETH_P_IP);
		break;
	case 0x60:
		proto = htons(ETH_P_IPV6);
		break;
	case 0x00:
		if (is_multicast_ether_addr(skb->data))
			return 1;
		/* possibly bogus destination - rewrite just in case */
		skb_reset_mac_header(skb);
		goto fix_dest;
	default:
		/* pass along other packets without modifications */
		return 1;
	}
	if (skb_headroom(skb) < ETH_HLEN)
		return 0;
	skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	eth_hdr(skb)->h_proto = proto;
	memset(eth_hdr(skb)->h_source, 0, ETH_ALEN);
#if 1 //Added by Quectel
	//some kernel will drop ethernet packet which's souce mac is all zero
	memcpy(eth_hdr(skb)->h_source, default_modem_addr, ETH_ALEN);
#endif
#ifdef QUECTEL_BRIDGE_MODE
	if (bridge_mode) {
		memcpy(eth_hdr(skb)->h_dest, bridge_mac, ETH_ALEN);
	}
#endif

fix_dest:
	memcpy(eth_hdr(skb)->h_dest, dev->net->dev_addr, ETH_ALEN);
	return 1;
}

#if defined(QUECTEL_WWAN_QMAP)
static struct sk_buff *qmap_qmi_wwan_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags) {
	struct qmi_wwan_state *info = (void *)&dev->data;
	sQmiWwanQmap *pQmapDev = (sQmiWwanQmap *)info->unused;
	struct qmap_hdr *qhdr;

	if (pQmapDev->qmap_mode == 0) {
		return qmi_wwan_tx_fixup(dev, skb, flags);
	}
	else if (pQmapDev->qmap_mode > 1) {
		qhdr = (struct qmap_hdr *)skb->data;
		if (qhdr->cd_rsvd_pad != 0) {
			goto drop_skb;
		}
		if ((qhdr->mux_id&0xF0) != 0x80) {
			goto drop_skb;
		}
		return skb;
	}
	else {
		if (qmi_wwan_tx_fixup(dev, skb, flags)) {
			qhdr = (struct qmap_hdr *)skb_push(skb, sizeof(struct qmap_hdr));
			qhdr->cd_rsvd_pad = 0;
			qhdr->mux_id = QUECTEL_QMAP_MUX_ID;
			qhdr->pkt_len = cpu_to_be16(skb->len - sizeof(struct qmap_hdr));

                    return skb;
		}
	}

drop_skb:
	dev_kfree_skb_any (skb);
	return NULL;
}

static int qmap_qmi_wwan_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	struct qmi_wwan_state *info = (void *)&dev->data;
	sQmiWwanQmap *pQmapDev = (sQmiWwanQmap *)info->unused;
	static int debug_len = 0;
	int debug_pkts = 0;
	int update_len = skb->len;

	if (pQmapDev->qmap_mode == 0)
		return qmi_wwan_rx_fixup(dev, skb);

	while (skb->len > sizeof(struct qmap_hdr)) {
		struct qmap_hdr *qhdr = (struct qmap_hdr *)skb->data;
		struct net_device *qmap_net;
		struct sk_buff *qmap_skb;
		__be16 proto;
		int pkt_len;
		u8 offset_id = 0;
		int err;
		unsigned len = (be16_to_cpu(qhdr->pkt_len) + sizeof(struct qmap_hdr));

		if (skb->len < (be16_to_cpu(qhdr->pkt_len) + sizeof(struct qmap_hdr))) {
			dev_info(&dev->net->dev, "drop qmap unknow pkt, len=%d, pkt_len=%d\n", skb->len, be16_to_cpu(qhdr->pkt_len));
			goto out;
		}

		debug_pkts++;

		if (qhdr->cd_rsvd_pad & 0x80) {
			dev_info(&dev->net->dev, "drop qmap command packet %x\n", qhdr->cd_rsvd_pad);
			goto skip_pkt;;
		}

		offset_id = qhdr->mux_id - QUECTEL_QMAP_MUX_ID;
		if (offset_id >= pQmapDev->qmap_mode) {
			dev_info(&dev->net->dev, "drop qmap unknow mux_id %x\n", qhdr->mux_id);
			goto skip_pkt;
		}

		qmap_net = pQmapDev->mpQmapNetDev[offset_id];

		if (qmap_net == NULL) {
			dev_info(&dev->net->dev, "drop qmap unknow mux_id %x\n", qhdr->mux_id);
			goto skip_pkt;
		}

		switch (skb->data[sizeof(struct qmap_hdr)] & 0xf0) {
			case 0x40:
				proto = htons(ETH_P_IP);
			break;
			case 0x60:
				proto = htons(ETH_P_IPV6);
			break;
			default:
				goto skip_pkt;
		}

		pkt_len = be16_to_cpu(qhdr->pkt_len) - (qhdr->cd_rsvd_pad&0x3F);
		qmap_skb = netdev_alloc_skb(qmap_net, ETH_HLEN + pkt_len);
		if (qmap_skb == NULL) {
			dev_info(&dev->net->dev, "alloc skb fail! pkt_len=%d\n", pkt_len);
			return 0;
		}
		skb_reset_mac_header(qmap_skb);
		memcpy(eth_hdr(qmap_skb)->h_source, default_modem_addr, ETH_ALEN);
		memcpy(eth_hdr(qmap_skb)->h_dest, qmap_net->dev_addr, ETH_ALEN);
		eth_hdr(qmap_skb)->h_proto = proto;        
		memcpy(skb_put(qmap_skb, ETH_HLEN + pkt_len) + ETH_HLEN, skb->data + sizeof(struct qmap_hdr), pkt_len);

		if (pQmapDev->qmap_mode > 1) {
			qmap_skb->protocol = eth_type_trans (qmap_skb, qmap_net);
			memset(qmap_skb->cb, 0, sizeof(struct skb_data));
			err = netif_rx(qmap_skb);
			if (err == NET_RX_SUCCESS) {
				qmap_net->stats.rx_packets++;
				qmap_net->stats.rx_bytes += qmap_skb->len;
			} else {
				qmap_net->stats.rx_errors++;
			}
		}
		else {
			usbnet_skb_return(dev, qmap_skb);
		}

		skip_pkt:
		skb_pull(skb, len);
	}

out:
	if (update_len > debug_len) {
		debug_len = update_len;
		dev_info(&dev->net->dev, "rx_pkts=%d, rx_len=%d\n", debug_pkts, debug_len);
	}
	
    return 0;
}
#endif

/* very simplistic detection of IPv4 or IPv6 headers */
static bool possibly_iphdr(const char *data)
{
	return (data[0] & 0xd0) == 0x40;
}

/* disallow addresses which may be confused with IP headers */
static int qmi_wwan_mac_addr(struct net_device *dev, void *p)
{
	int ret;
	struct sockaddr *addr = p;

	ret = eth_prepare_mac_addr_change(dev, p);
	if (ret < 0)
		return ret;
	if (possibly_iphdr(addr->sa_data))
		return -EADDRNOTAVAIL;
	eth_commit_mac_addr_change(dev, p);
	return 0;
}

static const struct net_device_ops qmi_wwan_netdev_ops = {
	.ndo_open		= usbnet_open,
	.ndo_stop		= usbnet_stop,
	.ndo_start_xmit		= usbnet_start_xmit,
	.ndo_tx_timeout		= usbnet_tx_timeout,
	.ndo_change_mtu		= usbnet_change_mtu,
	.ndo_set_mac_address	= qmi_wwan_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

/* using a counter to merge subdriver requests with our own into a
 * combined state
 */
static int qmi_wwan_manage_power(struct usbnet *dev, int on)
{
	struct qmi_wwan_state *info = (void *)&dev->data;
	int rv;

	dev_dbg(&dev->intf->dev, "%s() pmcount=%d, on=%d\n", __func__,
		atomic_read(&info->pmcount), on);

	if ((on && atomic_add_return(1, &info->pmcount) == 1) ||
	    (!on && atomic_dec_and_test(&info->pmcount))) {
		/* need autopm_get/put here to ensure the usbcore sees
		 * the new value
		 */
		rv = usb_autopm_get_interface(dev->intf);
		dev->intf->needs_remote_wakeup = on;
		if (!rv)
			usb_autopm_put_interface(dev->intf);
	}
	return 0;
}

static int qmi_wwan_cdc_wdm_manage_power(struct usb_interface *intf, int on)
{
	struct usbnet *dev = usb_get_intfdata(intf);

	/* can be called while disconnecting */
	if (!dev)
		return 0;
	return qmi_wwan_manage_power(dev, on);
}

/* collect all three endpoints and register subdriver */
static int qmi_wwan_register_subdriver(struct usbnet *dev)
{
	int rv;
	struct usb_driver *subdriver = NULL;
	struct qmi_wwan_state *info = (void *)&dev->data;

	/* collect bulk endpoints */
	rv = usbnet_get_endpoints(dev, info->data);
	if (rv < 0)
		goto err;

	/* update status endpoint if separate control interface */
	if (info->control != info->data)
		dev->status = &info->control->cur_altsetting->endpoint[0];

	/* require interrupt endpoint for subdriver */
	if (!dev->status) {
		rv = -EINVAL;
		goto err;
	}

	/* for subdriver power management */
	atomic_set(&info->pmcount, 0);

	/* register subdriver */
	subdriver = usb_cdc_wdm_register(info->control, &dev->status->desc,
					 4096, &qmi_wwan_cdc_wdm_manage_power);
	if (IS_ERR(subdriver)) {
		dev_err(&info->control->dev, "subdriver registration failed\n");
		rv = PTR_ERR(subdriver);
		goto err;
	}

	/* prevent usbnet from using status endpoint */
	dev->status = NULL;

	/* save subdriver struct for suspend/resume wrappers */
	info->subdriver = subdriver;

err:
	return rv;
}

static int qmi_wwan_bind(struct usbnet *dev, struct usb_interface *intf)
{
	int status = -1;
	u8 *buf = intf->cur_altsetting->extra;
	int len = intf->cur_altsetting->extralen;
	struct usb_interface_descriptor *desc = &intf->cur_altsetting->desc;
	struct usb_cdc_union_desc *cdc_union = NULL;
	struct usb_cdc_ether_desc *cdc_ether = NULL;
	u32 found = 0;
	struct usb_driver *driver = driver_of(intf);
	struct qmi_wwan_state *info = (void *)&dev->data;

	BUILD_BUG_ON((sizeof(((struct usbnet *)0)->data) <
		      sizeof(struct qmi_wwan_state)));

	/* set up initial state */
	info->control = intf;
	info->data = intf;

	/* and a number of CDC descriptors */
	while (len > 3) {
		struct usb_descriptor_header *h = (void *)buf;

		/* ignore any misplaced descriptors */
		if (h->bDescriptorType != USB_DT_CS_INTERFACE)
			goto next_desc;

		/* buf[2] is CDC descriptor subtype */
		switch (buf[2]) {
		case USB_CDC_HEADER_TYPE:
			if (found & 1 << USB_CDC_HEADER_TYPE) {
				dev_dbg(&intf->dev, "extra CDC header\n");
				goto err;
			}
			if (h->bLength != sizeof(struct usb_cdc_header_desc)) {
				dev_dbg(&intf->dev, "CDC header len %u\n",
					h->bLength);
				goto err;
			}
			break;
		case USB_CDC_UNION_TYPE:
			if (found & 1 << USB_CDC_UNION_TYPE) {
				dev_dbg(&intf->dev, "extra CDC union\n");
				goto err;
			}
			if (h->bLength != sizeof(struct usb_cdc_union_desc)) {
				dev_dbg(&intf->dev, "CDC union len %u\n",
					h->bLength);
				goto err;
			}
			cdc_union = (struct usb_cdc_union_desc *)buf;
			break;
		case USB_CDC_ETHERNET_TYPE:
			if (found & 1 << USB_CDC_ETHERNET_TYPE) {
				dev_dbg(&intf->dev, "extra CDC ether\n");
				goto err;
			}
			if (h->bLength != sizeof(struct usb_cdc_ether_desc)) {
				dev_dbg(&intf->dev, "CDC ether len %u\n",
					h->bLength);
				goto err;
			}
			cdc_ether = (struct usb_cdc_ether_desc *)buf;
			break;
		}

		/* Remember which CDC functional descriptors we've seen.  Works
		 * for all types we care about, of which USB_CDC_ETHERNET_TYPE
		 * (0x0f) is the highest numbered
		 */
		if (buf[2] < 32)
			found |= 1 << buf[2];

next_desc:
		len -= h->bLength;
		buf += h->bLength;
	}

	/* Use separate control and data interfaces if we found a CDC Union */
	if (cdc_union) {
		info->data = usb_ifnum_to_if(dev->udev,
					     cdc_union->bSlaveInterface0);
		if (desc->bInterfaceNumber != cdc_union->bMasterInterface0 ||
		    !info->data) {
			dev_err(&intf->dev,
				"bogus CDC Union: master=%u, slave=%u\n",
				cdc_union->bMasterInterface0,
				cdc_union->bSlaveInterface0);
			goto err;
		}
	}

	/* errors aren't fatal - we can live with the dynamic address */
	if (cdc_ether) {
		dev->hard_mtu = le16_to_cpu(cdc_ether->wMaxSegmentSize);
		usbnet_get_ethernet_addr(dev, cdc_ether->iMACAddress);
	}

	/* claim data interface and set it up */
	if (info->control != info->data) {
		status = usb_driver_claim_interface(driver, info->data, dev);
		if (status < 0)
			goto err;
	}

	status = qmi_wwan_register_subdriver(dev);
	if (status < 0 && info->control != info->data) {
		usb_set_intfdata(info->data, NULL);
		usb_driver_release_interface(driver, info->data);
	}

	/* Never use the same address on both ends of the link, even
	 * if the buggy firmware told us to.
	 */
	if (ether_addr_equal(dev->net->dev_addr, default_modem_addr))
		eth_hw_addr_random(dev->net);

	/* make MAC addr easily distinguishable from an IP header */
	if (possibly_iphdr(dev->net->dev_addr)) {
		dev->net->dev_addr[0] |= 0x02;	/* set local assignment bit */
		dev->net->dev_addr[0] &= 0xbf;	/* clear "IP" bit */
	}
	dev->net->netdev_ops = &qmi_wwan_netdev_ops;

#if 1 //Added by Quectel
	if (dev->driver_info->flags & FLAG_NOARP) {		
		dev_info(&intf->dev, "Quectel EC25&EC21&EG91&EG95&EG06&EP06&EM06&EG12&EP12&EM12&EG16&EG18&BG96&AG35 work on RawIP mode\n");
		dev->net->flags |= IFF_NOARP;
		usb_control_msg(
			interface_to_usbdev(intf),
			usb_sndctrlpipe(interface_to_usbdev(intf), 0),
			0x22, //USB_CDC_REQ_SET_CONTROL_LINE_STATE
			0x21, //USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE
			1, //active CDC DTR
			intf->cur_altsetting->desc.bInterfaceNumber,
			NULL, 0, 100);
	}

	//to advoid module report mtu 1460, but rx 1500 bytes IP packets, and cause the customer's system crash
	//next setting can make usbnet.c:usbnet_change_mtu() do not modify rx_urb_size according to hard mtu
	dev->rx_urb_size = ETH_DATA_LEN + ETH_HLEN + 6;
#endif

err:
	return status;
}

static void qmi_wwan_unbind(struct usbnet *dev, struct usb_interface *intf)
{
	struct qmi_wwan_state *info = (void *)&dev->data;
	struct usb_driver *driver = driver_of(intf);
	struct usb_interface *other;

	if (info->subdriver && info->subdriver->disconnect)
		info->subdriver->disconnect(info->control);

	/* allow user to unbind using either control or data */
	if (intf == info->control)
		other = info->data;
	else
		other = info->control;

	/* only if not shared */
	if (other && intf != other) {
		usb_set_intfdata(other, NULL);
		usb_driver_release_interface(driver, other);
	}

	info->subdriver = NULL;
	info->data = NULL;
	info->control = NULL;
}

/* suspend/resume wrappers calling both usbnet and the cdc-wdm
 * subdriver if present.
 *
 * NOTE: cdc-wdm also supports pre/post_reset, but we cannot provide
 * wrappers for those without adding usbnet reset support first.
 */
static int qmi_wwan_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	struct qmi_wwan_state *info = (void *)&dev->data;
	int ret;

	/* Both usbnet_suspend() and subdriver->suspend() MUST return 0
	 * in system sleep context, otherwise, the resume callback has
	 * to recover device from previous suspend failure.
	 */
	ret = usbnet_suspend(intf, message);
	if (ret < 0)
		goto err;

	if (intf == info->control && info->subdriver &&
	    info->subdriver->suspend)
		ret = info->subdriver->suspend(intf, message);
	if (ret < 0)
		usbnet_resume(intf);
err:
	return ret;
}

static int qmi_wwan_resume(struct usb_interface *intf)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	struct qmi_wwan_state *info = (void *)&dev->data;
	int ret = 0;
	bool callsub = (intf == info->control && info->subdriver &&
			info->subdriver->resume);

	if (callsub)
		ret = info->subdriver->resume(intf);
	if (ret < 0)
		goto err;
	ret = usbnet_resume(intf);
	if (ret < 0 && callsub)
		info->subdriver->suspend(intf, PMSG_SUSPEND);
err:
	return ret;
}

static const struct driver_info	qmi_wwan_info = {
	.description	= "WWAN/QMI device",
	.flags		= FLAG_WWAN,
	.bind		= qmi_wwan_bind,
	.unbind		= qmi_wwan_unbind,
	.manage_power	= qmi_wwan_manage_power,
	.rx_fixup       = qmi_wwan_rx_fixup,
};

static const struct driver_info qmi_wwan_raw_ip_info = {
	.description	= "WWAN/QMI device",
	.flags		= FLAG_WWAN | FLAG_RX_ASSEMBLE | FLAG_NOARP | FLAG_SEND_ZLP,
	.bind		= qmi_wwan_bind,
	.unbind		= qmi_wwan_unbind,
	.manage_power	= qmi_wwan_manage_power,
#if defined(QUECTEL_WWAN_QMAP)
	.tx_fixup       = qmap_qmi_wwan_tx_fixup,
	.rx_fixup       = qmap_qmi_wwan_rx_fixup,
#else
	.tx_fixup       = qmi_wwan_tx_fixup,
	.rx_fixup       = qmi_wwan_rx_fixup,
#endif
};


/* map QMI/wwan function by a fixed interface number */
#define QMI_FIXED_INTF(vend, prod, num) \
	USB_DEVICE_INTERFACE_NUMBER(vend, prod, num), \
	.driver_info = (unsigned long)&qmi_wwan_info

#define QMI_FIXED_RAWIP_INTF(vend, prod, num) \
	USB_DEVICE_INTERFACE_NUMBER(vend, prod, num), \
	.driver_info = (unsigned long)&qmi_wwan_raw_ip_info

static const struct usb_device_id products[] = {
#if 1 //Added by Quectel
	{ QMI_FIXED_INTF(0x05C6, 0x9003, 4) },  /* Quectel UC20 */
	{ QMI_FIXED_INTF(0x05C6, 0x9215, 4) },  /* Quectel EC20 (MDM9215) */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0125, 4) },  /* Quectel EC25 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0121, 4) },  /* Quectel EC21 */
	{ QMI_FIXED_RAWIP_INTF(0x05C6, 0x9215, 4) },  /* Quectel EC20 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0191, 4) },  /* Quectel EG91 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0195, 4) },  /* Quectel EG95 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0306, 4) },  /* Quectel EG06/EP06/EM06 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0512, 4) },  /* Quectel EG12/EP12/EM12/EG16/EG18 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0296, 4) },  /* Quectel BG96 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0435, 4) },  /* Quectel AG35 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0620, 4) },  /* Quectel EG20 */
	{ QMI_FIXED_RAWIP_INTF(0x2C7C, 0x0800, 4) },  /* Quectel RG500 */
#endif
	{ }					/* END */
};
MODULE_DEVICE_TABLE(usb, products);

static int qmi_wwan_probe(struct usb_interface *intf,
			  const struct usb_device_id *prod)
{
	struct usb_device_id *id = (struct usb_device_id *)prod;

	/* Workaround to enable dynamic IDs.  This disables usbnet
	 * blacklisting functionality.  Which, if required, can be
	 * reimplemented here by using a magic "blacklist" value
	 * instead of 0 in the static device id table
	 */
	if (!id->driver_info) {
		dev_dbg(&intf->dev, "setting defaults for dynamic device id\n");
		id->driver_info = (unsigned long)&qmi_wwan_info;
	}

	if (intf->cur_altsetting->desc.bInterfaceClass != 0xff) {
		dev_info(&intf->dev,  "Quectel module not qmi_wwan mode! please check 'at+qcfg=\"usbnet\"'\n");
		return -ENODEV;
	}

	return usbnet_probe(intf, id);
}

#if defined(QUECTEL_WWAN_QMAP)
static int qmap_qmi_wwan_probe(struct usb_interface *intf,
			  const struct usb_device_id *prod)
{
	int status = qmi_wwan_probe(intf, prod);
	
	if (!status) {
		struct usbnet *dev = usb_get_intfdata(intf);
		struct qmi_wwan_state *info = (void *)&dev->data;		
		sQmiWwanQmap *pQmapDev = (sQmiWwanQmap *)kzalloc(sizeof(sQmiWwanQmap), GFP_KERNEL);

		pQmapDev->mpNetDev = dev;
		if (dev->driver_info->flags & FLAG_NOARP) //only RAWIP support QMAP, Eth mode do not support
			pQmapDev->qmap_mode = qmap_mode;
		info->unused = (unsigned long)pQmapDev;		
		
		if (pQmapDev->qmap_mode == 1) {
			pQmapDev->mpQmapNetDev[0] = dev->net;
		}
		else if (pQmapDev->qmap_mode > 1) {
			unsigned i;

			for (i = 0; i < pQmapDev->qmap_mode; i++) {
				qmap_register_device(pQmapDev, i);
			}
		}

		if (pQmapDev->qmap_mode) {
			int idProduct = le16_to_cpu(dev->udev->descriptor.idProduct);
			
			if (idProduct == 0x0121 || idProduct == 0x0125 || idProduct == 0x0435) //MDM9x07
				dev->rx_urb_size = 4*1024;
			else if (idProduct == 0x0306) //MDM9x40
				dev->rx_urb_size = 16*1024;
			else if (idProduct == 0x0512) //SDX20
				dev->rx_urb_size = 32*1024;			
			else if (idProduct == 0x0620) //SDX24
				dev->rx_urb_size = 32*1024;
			else if (idProduct == 0x0800) //SDX55
				dev->rx_urb_size = 32*1024;
			else
				dev->rx_urb_size = 32*1024;
		}

		dev_info(&intf->dev, "rx_urb_size = %zd\n", dev->rx_urb_size);
	}

	return status;
}

static void qmap_qmi_wwan_disconnect(struct usb_interface *intf)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	struct qmi_wwan_state *info = (void *)&dev->data;
	sQmiWwanQmap *pQmapDev = (sQmiWwanQmap *)info->unused;

	if (pQmapDev) {
		if (pQmapDev->qmap_mode > 1) {
			unsigned i;

			for (i = 0; i < pQmapDev->qmap_mode; i++) {
				qmap_unregister_device(pQmapDev, i);
			}	
		}
		
		kfree(pQmapDev);
	}

	usbnet_disconnect(intf);
}
#endif

static struct usb_driver qmi_wwan_driver = {
	.name		      = "qmi_wwan_q",
	.id_table	      = products,
	.probe		      = qmi_wwan_probe,
#if defined(QUECTEL_WWAN_QMAP)
	.probe		      = qmap_qmi_wwan_probe,
	.disconnect	      = qmap_qmi_wwan_disconnect,
#else
	.probe		      = qmi_wwan_probe,
	.disconnect	      = usbnet_disconnect,
#endif
	.suspend	      = qmi_wwan_suspend,
	.resume		      =	qmi_wwan_resume,
	.reset_resume         = qmi_wwan_resume,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(qmi_wwan_driver);

MODULE_AUTHOR("Bjørn Mork <bjorn@mork.no>");
MODULE_DESCRIPTION("Qualcomm MSM Interface (QMI) WWAN driver");
MODULE_LICENSE("GPL");
