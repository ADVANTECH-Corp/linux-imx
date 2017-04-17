/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_HAL_INIT_C_

#include <drv_types.h>

#include <rtl8188e_hal.h>
#include <rtl8188e_led.h>

#if defined (PLATFORM_LINUX) && defined (PLATFORM_WINDOWS)

#error "Shall be Linux or Windows, but not both!\n"

#endif

#ifndef CONFIG_PCI_HCI

#error "CONFIG_PCI_HCI shall be on!\n"

#endif


// For Two MAC FPGA verify we must disable all MAC/BB/RF setting
#define FPGA_UNKNOWN		0
#define FPGA_2MAC			1
#define FPGA_PHY			2
#define ASIC					3
#define BOARD_TYPE			ASIC

#if BOARD_TYPE == FPGA_2MAC
#else // FPGA_PHY and ASIC
#define FPGA_RF_UNKOWN	0
#define FPGA_RF_8225		1
#define FPGA_RF_0222D		2
#define FPGA_RF				FPGA_RF_0222D
#endif

static u8 getChnlGroup(u8 chnl)
{
	u8	group=0;

	if (chnl < 3)			// Cjanel 1-3
		group = 0;
	else if (chnl < 9)		// Channel 4-9
		group = 1;
	else					// Channel 10-14
		group = 2;
	
	return group;
}


static BOOLEAN
Check11nProductID(
	IN	PADAPTER	Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if(((pHalData->EEPROMDID==0x8191)) ||
		(pHalData->EEPROMDID==0x8193)||
		((pHalData->EEPROMDID==0x8176) && (
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8176) || // <= Start of 88CE Solo
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8175) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8181) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8182) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8184) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8185) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8219) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8207) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8208) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8209) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8210) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8220) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8211) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8212) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8213) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8214) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8221) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8215) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8216) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8217) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8218) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8222) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x8186) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x8187) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8191) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8192) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8193) ||
			(pHalData->EEPROMSVID == 0x1A3B && pHalData->EEPROMSMID == 0x1139) ||
			(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x8194) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8197) ||
			(pHalData->EEPROMSVID == 0x1462 && pHalData->EEPROMSMID == 0x3824) ||
			(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x8198) ||
			(pHalData->EEPROMSVID == 0x185F && pHalData->EEPROMSMID == 0x8176) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8206) ||
			(pHalData->EEPROMSVID == 0x1A32 && pHalData->EEPROMSMID == 0x0315) ||
			(pHalData->EEPROMSVID == 0x144F && pHalData->EEPROMSMID == 0x7185) ||
			(pHalData->EEPROMSVID == 0x103C && pHalData->EEPROMSMID == 0x1629) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8199) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8203) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8204) ||
			(pHalData->EEPROMSVID == 0x1043 && pHalData->EEPROMSMID == 0x84B5) ||
			(pHalData->EEPROMSVID == 0x1A32 && pHalData->EEPROMSMID == 0x2315) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7611) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8200) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8201) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8202) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8205) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8195) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8150) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9176) || // <= Start of 88CE Combo
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9181) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9182) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9184) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9185) ||
			(pHalData->EEPROMSVID == 0x103C && pHalData->EEPROMSMID == 0x169F) ||
			(pHalData->EEPROMSVID == 0x103C && pHalData->EEPROMSMID == 0x1786) ||
			(pHalData->EEPROMSVID == 0x10CF && pHalData->EEPROMSMID == 0x16B3) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x9186) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x9187) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9191) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9192) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9193) ||
			(pHalData->EEPROMSVID == 0x1A3B && pHalData->EEPROMSMID == 0x2057) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9194) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9196) ||
			(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x9197) ||
			(pHalData->EEPROMSVID == 0x1462 && pHalData->EEPROMSMID == 0x3874) ||
			(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x9198) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9201) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9202) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9203) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9204) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9195) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9199) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9200) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9150)))	||
		((pHalData->EEPROMDID==0x8177)) ||
		((pHalData->EEPROMDID==0x8178) && (
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8178) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x8179) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x8180) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8186) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8191) ||
			(pHalData->EEPROMSVID == 0x1A3B && pHalData->EEPROMSMID == 0x1178) ||
			(pHalData->EEPROMSVID == 0x1043 && pHalData->EEPROMSMID == 0x84B6) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8189) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7622) ||
			(pHalData->EEPROMSVID == 0x1B9A && pHalData->EEPROMSMID == 0x1400) ||
			(pHalData->EEPROMSVID == 0x1B9A && pHalData->EEPROMSMID == 0x1401) ||
			(pHalData->EEPROMSVID == 0x1B9A && pHalData->EEPROMSMID == 0x1402) ||
			(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9178) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x9179) ||
			(pHalData->EEPROMSVID == 0x1025 && pHalData->EEPROMSMID == 0x9180)))	||
		((pHalData->EEPROMDID == 0x8723)))
	{
		return _TRUE;
	}
	else
	{
		return _FALSE;
	}

}

static VOID
hal_CustomizedBehavior_8188EE(
	PADAPTER			Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*pledpriv = &(Adapter->ledpriv);

	pledpriv->LedStrategy = SW_LED_MODE7; //Default LED strategy.
	pHalData->bLedOpenDrain = _TRUE;// Support Open-drain arrangement for controlling the LED. Added by Roger, 2009.10.16.
	
	switch(pHalData->CustomerID)
	{

		case RT_CID_DEFAULT:
			break;

		case RT_CID_819x_SAMSUNG:
			//pMgntInfo->bAutoConnectEnable = FALSE;
			//pMgntInfo->bForcedShowRateStill = TRUE;
			break;

		case RT_CID_TOSHIBA:
			pHalData->CurrentChannel = 10;
			//pHalData->EEPROMRegulatory = 1;
			break;

		case RT_CID_CCX:
			//pMgntInfo->IndicateByDeauth = _TRUE;
			break;

		case RT_CID_819x_Lenovo:
			// Customize Led mode	
			pledpriv->LedStrategy = SW_LED_MODE7;
			// Customize  Link any for auto connect
			// This Value should be set after InitializeMgntVariables
			//pMgntInfo->bAutoConnectEnable = FALSE;
			//pMgntInfo->pHTInfo->RxReorderPendingTime = 50;
			DBG_8192C("RT_CID_819x_Lenovo \n");

			if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8200) ||
				(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9199) )
			{
				pledpriv->LedStrategy = SW_LED_MODE9;
			}
			else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8201) ||
					(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8202) ||
					(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9200))
			{
				pledpriv->LedStrategy = SW_LED_MODE7;
			}
			else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8205)||
					(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9204))
			{
				pledpriv->LedStrategy = SW_LED_MODE7;
				pHalData->bLedOpenDrain = _FALSE;
			}
			else
			{
				pledpriv->LedStrategy = SW_LED_MODE7;
			}
			break;

		case RT_CID_819x_QMI:
			pledpriv->LedStrategy = SW_LED_MODE8; // Customize Led mode	
			break;			

		case RT_CID_819x_HP:
			pledpriv->LedStrategy = SW_LED_MODE7; // Customize Led mode	
			break;

		case RT_CID_819x_Acer:
			break;			

		case RT_CID_819x_Edimax_ASUS:
			pledpriv->LedStrategy = SW_LED_MODE10; // Customize Led mode	
			break;				

		case RT_CID_WHQL:
			//Adapter->bInHctTest = TRUE;
			break;

		case RT_CID_819x_PRONETS:
			pledpriv->LedStrategy = SW_LED_MODE9; // Customize Led mode	
			break;			
	
		default:
			MSG_8192C("Unkown hardware Type \n");
			break;
	}
	MSG_8192C("hal_CustomizedBehavior_8188EE(): RT Customized ID: 0x%02X\n", pHalData->CustomerID);

	if((pHalData->bautoload_fail_flag) || (!Check11nProductID(Adapter)))
	{
		if(pHalData->CurrentWirelessMode != WIRELESS_MODE_B)
			pHalData->CurrentWirelessMode = WIRELESS_MODE_G;
	}
}

static VOID
hal_CustomizeByCustomerID_88EE(
	IN	PADAPTER		pAdapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	
	// If the customer ID had been changed by registry, do not cover up by EEPROM.
	if(pHalData->CustomerID == RT_CID_DEFAULT)
	{
		switch(pHalData->EEPROMCustomerID)
		{	
			case EEPROM_CID_DEFAULT:
				if(pHalData->EEPROMDID==0x8176)
				{
					if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6151) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6152) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6154) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6155) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6177) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6178) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6179) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6180) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7151) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7152) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7154) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7155) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7177) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7178) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7179) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7180) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8151) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8152) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8154) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8155) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8181) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8182) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8184) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8185) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8219) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8207) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8208) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8209) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8210) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8220) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8211) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8212) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8213) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8214) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8221) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8215) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8216) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8217) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8218) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8222) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9151) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9152) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9154) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9155) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9181) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9182) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9184) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9185) )
							pHalData->CustomerID = RT_CID_TOSHIBA;
					else if(pHalData->EEPROMSVID == 0x1025)
							pHalData->CustomerID = RT_CID_819x_Acer;	
					else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6191) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6192) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6193) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7191) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7192) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7193) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8191) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8192) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8193) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9191) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9192) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9193) )
							pHalData->CustomerID = RT_CID_819x_SAMSUNG;
					else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8195) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9195) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7194) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8200) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8201) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8202) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8205) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9199) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9200) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9204))
							pHalData->CustomerID = RT_CID_819x_Lenovo;
					else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8197) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9196) )
							pHalData->CustomerID = RT_CID_819x_CLEVO;
					else if((pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x8194) ||
							(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x8198) ||
							(pHalData->EEPROMSVID == 0x185F && pHalData->EEPROMSMID == 0x8176) ||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8206) ||
							(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x9197) ||
							(pHalData->EEPROMSVID == 0x1028 && pHalData->EEPROMSMID == 0x9198))
							pHalData->CustomerID = RT_CID_819x_DELL;
					else if((pHalData->EEPROMSVID == 0x103C && pHalData->EEPROMSMID == 0x1629) ||// HP LiteOn
							(pHalData->EEPROMSVID == 0x103C && pHalData->EEPROMSMID == 0x1786) )
			 				pHalData->CustomerID = RT_CID_819x_HP;
					else if((pHalData->EEPROMSVID == 0x1A32 && pHalData->EEPROMSMID == 0x2315))// QMI
							pHalData->CustomerID = RT_CID_819x_QMI;	
					else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8203))// QMI				
							pHalData->CustomerID = RT_CID_819x_PRONETS;
					else if((pHalData->EEPROMSVID == 0x1043 && pHalData->EEPROMSMID == 0x84B5)||
							(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7611))// ASUS
						pHalData->CustomerID = RT_CID_819x_Edimax_ASUS;					
					else
						pHalData->CustomerID = RT_CID_DEFAULT;
				}
				else if(pHalData->EEPROMDID==0x8178)
				{
					if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6181) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6182) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6184) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x6185) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7181) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7182) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7184) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7185) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8181) ||						
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8182) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8184) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8185) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9181) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9182) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9184) ||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x9185) )
							pHalData->CustomerID = RT_CID_TOSHIBA;
					else if(pHalData->EEPROMSVID == 0x1025)
						pHalData->CustomerID = RT_CID_819x_Acer;
					else if((pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x8186))// PRONETS					
						pHalData->CustomerID = RT_CID_819x_PRONETS;
					else if((pHalData->EEPROMSVID == 0x1043 && pHalData->EEPROMSMID == 0x84B6)||
						(pHalData->EEPROMSVID == 0x10EC && pHalData->EEPROMSMID == 0x7622))// ASUS				
						pHalData->CustomerID = RT_CID_819x_Edimax_ASUS;						
					else
						pHalData->CustomerID = RT_CID_DEFAULT;
				}
				else
				{
					pHalData->CustomerID = RT_CID_DEFAULT;
				}
				break;
				
			case EEPROM_CID_TOSHIBA:       
				pHalData->CustomerID = RT_CID_TOSHIBA;
				break;

			case EEPROM_CID_CCX:
				pHalData->CustomerID = RT_CID_CCX;
				break;

			case EEPROM_CID_QMI:
				pHalData->CustomerID = RT_CID_819x_QMI;
				break;
				
			case EEPROM_CID_WHQL:
				/*pAdapter->bInHctTest = TRUE;
				
				pMgntInfo->bSupportTurboMode = FALSE;
				pMgntInfo->bAutoTurboBy8186 = FALSE;
				pMgntInfo->PowerSaveControl.bInactivePs = FALSE;
				pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE;
				pMgntInfo->PowerSaveControl.bLeisurePs = FALSE;
				pMgntInfo->PowerSaveControl.bLeisurePsModeBackup = FALSE;
				pMgntInfo->keepAliveLevel = 0;	
				pAdapter->bUnloadDriverwhenS3S4 = FALSE;*/
				break;
					
			default:
				pHalData->CustomerID = RT_CID_DEFAULT;
				break;
					
		}
	}
	//MSG_8192C("MGNT Customer ID: 0x%2x\n", pHalData->CustomerID);
	
	hal_CustomizedBehavior_8188EE(pAdapter);
}


//
//	Description:
//		Config HW adapter information into initial value.
//
//	Assumption:
//		1. After Auto load fail(i.e, check CR9346 fail)
//
//	Created by Roger, 2008.10.21.
//
static	VOID
ConfigAdapterInfo8188EForAutoLoadFail(
	IN PADAPTER			Adapter
)
{ 
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	u16	i;
	u8	sMacAddr[6] = {0x00, 0xE0, 0x4C, 0x81, 0x92, 0x00};
	u8	*hwinfo = 0;

	DBG_8192C("====> ConfigAdapterInfo8188EForAutoLoadFail\n");

	// Initialize IC Version && Channel Plan
	pHalData->EEPROMVID = 0;
	pHalData->EEPROMDID = 0;
	DBG_8192C("EEPROM VID = 0x%4x\n", pHalData->EEPROMVID);
	DBG_8192C("EEPROM DID = 0x%4x\n", pHalData->EEPROMDID);	

	pHalData->EEPROMCustomerID = 0;
	DBG_8192C("EEPROM Customer ID: 0x%2x\n", pHalData->EEPROMCustomerID);
	hal_CustomizeByCustomerID_88EE(Adapter);
	
	//
	// Read tx power index from efuse or eeprom
	//
//	readTxPowerInfo_8188E(Adapter, pEEPROM->bautoload_fail_flag, hwinfo);
	Hal_ReadTxPowerInfo88E(Adapter, hwinfo, pHalData->bautoload_fail_flag);
	
	//
	// Read Bluetooth co-exist and initialize
	//
#ifdef CONFIG_BT_COEXIST
	//ReadBluetoothCoexistInfoFromHWPG(Adapter, pHalData->bautoload_fail_flag, hwinfo);
//	rtl8188e_ReadBluetoothCoexistInfo(Adapter, hwinfo, pHalData->bautoload_fail_flag);
#endif

	pHalData->EEPROMVersion = 1;		// Default version is 1
	pHalData->bTXPowerDataReadFromEEPORM = _FALSE;

	DBG_8192C("<==== ConfigAdapterInfo8188EForAutoLoadFail\n");
}




static void
Hal_EfuseParsePIDVID_8188EE(
	IN	PADAPTER		pAdapter,
	IN	u8*				hwinfo,
	IN	BOOLEAN			AutoLoadFail
	)
{

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	if( !AutoLoadFail )
	{
		// VID, PID 
		pHalData->EEPROMVID = EF2Byte( *(u16 *)&hwinfo[EEPROM_VID_88EE] );
		pHalData->EEPROMDID = EF2Byte( *(u16 *)&hwinfo[EEPROM_DID_88EE] );
		
		// Customer ID, 0x00 and 0xff are reserved for Realtek. 		
		pHalData->EEPROMCustomerID = *(u8 *)&hwinfo[EEPROM_CustomID_88E];
		if(pHalData->EEPROMCustomerID == 0xFF)
			pHalData->EEPROMCustomerID = EEPROM_Default_CustomerID_8188E;
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;

	}
	else
	{
		pHalData->EEPROMVID 			= EEPROM_Default_VID;
		pHalData->EEPROMDID 			= EEPROM_Default_PID;

		// Customer ID, 0x00 and 0xff are reserved for Realtek. 		
		pHalData->EEPROMCustomerID		= EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID	= EEPROM_Default_SubCustomerID;

	}

	DBG_871X("VID = 0x%04X, DID = 0x%04X\n", pHalData->EEPROMVID, pHalData->EEPROMDID);
	DBG_871X("Customer ID: 0x%02X, SubCustomer ID: 0x%02X\n", pHalData->EEPROMCustomerID, pHalData->EEPROMSubCustomerID);
}

static void
Hal_CustomizeByCustomerID_8188EE(
	IN	PADAPTER		padapter
	)
{
#if 0	
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	// For customized behavior.
	if((pHalData->EEPROMVID == 0x103C) && (pHalData->EEPROMVID == 0x1629))// HP Lite-On for RTL8188CUS Slim Combo.
		pHalData->CustomerID = RT_CID_819x_HP;

	// Decide CustomerID according to VID/DID or EEPROM
	switch(pHalData->EEPROMCustomerID)
	{
		case EEPROM_CID_DEFAULT:
			if((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3308))
				pHalData->CustomerID = RT_CID_DLINK;
			else if((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3309))
				pHalData->CustomerID = RT_CID_DLINK;
			else if((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x330a))
				pHalData->CustomerID = RT_CID_DLINK;
			break;
		case EEPROM_CID_WHQL:
			padapter->bInHctTest = TRUE;

			pMgntInfo->bSupportTurboMode = FALSE;
			pMgntInfo->bAutoTurboBy8186 = FALSE;

			pMgntInfo->PowerSaveControl.bInactivePs = FALSE;
			pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE;
			pMgntInfo->PowerSaveControl.bLeisurePs = FALSE;
			pMgntInfo->PowerSaveControl.bLeisurePsModeBackup =FALSE;
			pMgntInfo->keepAliveLevel = 0;

			padapter->bUnloadDriverwhenS3S4 = FALSE;
			break;
		default:
			pHalData->CustomerID = RT_CID_DEFAULT;
			break;

	}

	RT_TRACE(_module_hci_hal_init_c_, _drv_info_, ("Mgnt Customer ID: 0x%02x\n", pHalData->CustomerID));

	hal_CustomizedBehavior_8723U(padapter);
#endif
}


//==============================================
static VOID
readAdapterInfo_8188EE(
	IN	PADAPTER	padapter
	)
{
#if 1
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

	/* parse the eeprom/efuse content */
	Hal_EfuseParseIDCode88E(padapter, pHalData->efuse_eeprom_data);
	Hal_EfuseParsePIDVID_8188EE(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_config_macaddr(padapter, pHalData->bautoload_fail_flag);
	Hal_ReadPowerSavingMode88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);	
	Hal_ReadTxPowerInfo88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);		
	Hal_EfuseParseEEPROMVer88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	rtl8188e_EfuseParseChnlPlan(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseXtal_8188E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseCustomerID88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadAntennaDiversity88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);	
	Hal_EfuseParseBoardType88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);	
	Hal_ReadThermalMeter_88E(padapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	//
	// The following part initialize some vars by PG info.
	//
	Hal_CustomizeByCustomerID_8188EE(padapter);
#else

#ifdef CONFIG_INTEL_PROXIM	
		/* for intel proximity */
	if (pHalData->rf_type== RF_1T1R) {
		Adapter->proximity.proxim_support = _TRUE;
	} else if (pHalData->rf_type== RF_2T2R) {
		if ((pHalData->EEPROMPID == 0x8186) &&
			(pHalData->EEPROMVID== 0x0bda))
		Adapter->proximity.proxim_support = _TRUE;
	} else {
		Adapter->proximity.proxim_support = _FALSE;
	}
#endif //CONFIG_INTEL_PROXIM		
#endif
}

static void _ReadPROMContent(
	IN PADAPTER 		Adapter
	)
{	
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
	u8			eeValue;

	/* check system boot selection */
	eeValue = rtw_read8(Adapter, REG_9346CR);
	pHalData->EepromOrEfuse		= (eeValue & BOOT_FROM_EEPROM) ? _TRUE : _FALSE;
	pHalData->bautoload_fail_flag	= (eeValue & EEPROM_EN) ? _FALSE : _TRUE;


	DBG_8192C("Boot from %s, Autoload %s !\n", (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
				(pHalData->bautoload_fail_flag ? "Fail" : "OK") );

	//pHalData->EEType = IS_BOOT_FROM_EEPROM(Adapter) ? EEPROM_93C46 : EEPROM_BOOT_EFUSE;

	Hal_InitPGData88E(Adapter);
	readAdapterInfo_8188EE(Adapter);
}


static VOID
_InitOtherVariable(
	IN PADAPTER 		Adapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);	


	//if(Adapter->bInHctTest){
	//	pMgntInfo->PowerSaveControl.bInactivePs = FALSE;
	//	pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE;
	//	pMgntInfo->PowerSaveControl.bLeisurePs = FALSE;
	//	pMgntInfo->keepAliveLevel = 0;
	//}
}

static VOID
_ReadRFType(
	IN	PADAPTER	Adapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

#if DISABLE_BB_RF
	pHalData->rf_chip = RF_PSEUDO_11N;
#else
	pHalData->rf_chip = RF_6052;
#endif
}

void _ReadSilmComboMode(PADAPTER Adapter)
{
}


#if 0
static int _ReadAdapterInfo8188EE(PADAPTER	Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32 start=rtw_get_current_time();
	
	MSG_8192C("====> %s\n", __FUNCTION__);



	_ReadRFType(Adapter);//rf_chip -> _InitRFType()
	_ReadPROMContent(Adapter);

	// 2010/10/25 MH THe function must be called after borad_type & IC-Version recognize.
	_ReadSilmComboMode(Adapter);

	_InitOtherVariable(Adapter);

	//MSG_8192C("%s()(done), rf_chip=0x%x, rf_type=0x%x\n",  __FUNCTION__, pHalData->rf_chip, pHalData->rf_type);

	MSG_8192C("<==== %s in %d ms\n", __FUNCTION__, rtw_get_passing_time_ms(start));

	return _SUCCESS;
}

#endif


//==============================================

static int _ReadAdapterInfo8188E(PADAPTER Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32 start=rtw_get_current_time();
	
	MSG_8192C("====> %s\n", __FUNCTION__);

	//Efuse_InitSomeVar(Adapter);

	_ReadRFType(Adapter);//rf_chip -> _InitRFType()
	_ReadPROMContent(Adapter);

	// 2010/10/25 MH THe function must be called after borad_type & IC-Version recognize.
	_ReadSilmComboMode(Adapter);

	_InitOtherVariable(Adapter);

	//MSG_8192C("%s()(done), rf_chip=0x%x, rf_type=0x%x\n",  __FUNCTION__, pHalData->rf_chip, pHalData->rf_type);

	MSG_8192C("<==== %s in %d ms\n", __FUNCTION__, rtw_get_passing_time_ms(start));

	return _SUCCESS;
}

static void ReadAdapterInfo8188EE(PADAPTER Adapter)
{
	// Read EEPROM size before call any EEPROM function
	//Adapter->EepromAddressSize=Adapter->HalFunc.GetEEPROMSizeHandler(Adapter);
	Adapter->EepromAddressSize = GetEEPROMSize8188E(Adapter);
	
	_ReadAdapterInfo8188E(Adapter);
}


void rtl8188ee_interface_configure(_adapter *padapter)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pwrctrl_priv	*pwrpriv = adapter_to_pwrctl(padapter);

_func_enter_;

	////close ASPM for AMD defaultly
	pdvobjpriv->const_amdpci_aspm = 0;

	//// ASPM PS mode.
	//// 0 - Disable ASPM, 1 - Enable ASPM without Clock Req, 
	//// 2 - Enable ASPM with Clock Req, 3- Alwyas Enable ASPM with Clock Req,
	//// 4-  Always Enable ASPM without Clock Req.
	//// set defult to rtl8188ee:3 RTL8192E:2
	pdvobjpriv->const_pci_aspm = 0;

	//// Setting for PCI-E device */
	pdvobjpriv->const_devicepci_aspm_setting = 0x03;

	//// Setting for PCI-E bridge */
	pdvobjpriv->const_hostpci_aspm_setting = 0x03;

	//// In Hw/Sw Radio Off situation.
	//// 0 - Default, 1 - From ASPM setting without low Mac Pwr, 
	//// 2 - From ASPM setting with low Mac Pwr, 3 - Bus D3
	//// set default to RTL8192CE:0 RTL8192SE:2
	pdvobjpriv->const_hwsw_rfoff_d3 = 0;

	//// This setting works for those device with backdoor ASPM setting such as EPHY setting.
	//// 0: Not support ASPM, 1: Support ASPM, 2: According to chipset.
	pdvobjpriv->const_support_pciaspm = 1;

	pwrpriv->reg_rfoff = 0;
	pwrpriv->rfoff_reason = 0;

_func_exit_;
}

VOID
DisableInterrupt8188EE (
	IN PADAPTER			Adapter
	)
{
	struct dvobj_priv	*pdvobjpriv= adapter_to_dvobj(Adapter);

	// Because 92SE now contain two DW IMR register range.
	rtw_write32(Adapter, REG_HIMR_88E, IMR_DISABLED_88E);
	rtw_write32(Adapter, REG_HIMRE_88E, IMR_DISABLED_88E);	// by tynli

	pdvobjpriv->irq_enabled = 0;

}

VOID
ClearInterrupt8188EE(
	IN PADAPTER			Adapter
	)
{
	u32	tmp = 0;
	
	tmp = rtw_read32(Adapter, REG_HISR_88E);	
	rtw_write32(Adapter, REG_HISR_88E, tmp);	

	tmp = 0;
	tmp = rtw_read32(Adapter, REG_HISRE_88E);	
	rtw_write32(Adapter, REG_HISRE_88E, tmp);	
}


VOID
EnableInterrupt8188EE(
	IN PADAPTER			Adapter
	)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);

	pdvobjpriv->irq_enabled = 1;

	pHalData->IntrMask[0] = pHalData->IntrMaskToSet[0];
	pHalData->IntrMask[1] = pHalData->IntrMaskToSet[1];

	rtw_write32(Adapter, REG_HIMR_88E, pHalData->IntrMask[0]&0xFFFFFFFF);

	rtw_write32(Adapter, REG_HIMRE_88E, pHalData->IntrMask[1]&0xFFFFFFFF);

}

void
InterruptRecognized8188EE(
	IN	PADAPTER			Adapter,
	OUT	PRT_ISR_CONTENT	pIsrContent
	)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);

	// 2013.11.18 Glayrainx suggests that turn off IMR and
	// restore after cleaning ISR.
	DisableInterrupt8188EE(Adapter);

	pIsrContent->IntArray[0] = rtw_read32(Adapter, REG_HISR_88E);	
	pIsrContent->IntArray[0] &= pHalData->IntrMask[0];
	rtw_write32(Adapter, REG_HISR_88E, pIsrContent->IntArray[0]);

	//For HISR extension. Added by tynli. 2009.10.07.
	pIsrContent->IntArray[1] = rtw_read32(Adapter, REG_HISRE_88E);	
	pIsrContent->IntArray[1] &= pHalData->IntrMask[1];
	rtw_write32(Adapter, REG_HISRE_88E, pIsrContent->IntArray[1]);

	EnableInterrupt8188EE(Adapter);
}

VOID
UpdateInterruptMask8188EE(
	IN	PADAPTER		Adapter,
	IN	u32		AddMSR, 	u32		AddMSR1,
	IN	u32		RemoveMSR, u32		RemoveMSR1
	)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);

	if( AddMSR )
	{
		pHalData->IntrMaskToSet[0] |= AddMSR;
	}
	if( AddMSR1 )
	{
		pHalData->IntrMaskToSet[1] |= AddMSR1;
	}

	if( RemoveMSR )
	{
		pHalData->IntrMaskToSet[0] &= (~RemoveMSR);
	}

	if( RemoveMSR1 )
	{
		pHalData->IntrMaskToSet[1] &= (~RemoveMSR1);
	}
	
	DisableInterrupt8188EE( Adapter );
	EnableInterrupt8188EE( Adapter );
}

 static VOID
_InitBeaconParameters(
	IN  PADAPTER Adapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	rtw_write16(Adapter, REG_BCN_CTRL, 0x1010);	// For 2 PORT TSF SYNC

	// TODO: Remove these magic number
	rtw_write16(Adapter, REG_TBTT_PROHIBIT,0x6404);// ms

	rtw_write8(Adapter, REG_DRVERLYINT, DRIVER_EARLY_INT_TIME_8188E);// 5ms
	rtw_write8(Adapter, REG_BCNDMATIM, BCN_DMA_ATIME_INT_TIME_8188E); // 2ms

	// Suggested by designer timchen. Change beacon AIFS to the largest number
	// beacause test chip does not contension before sending beacon. by tynli. 2009.11.03
	rtw_write16(Adapter, REG_BCNTCFG, 0x660F);
}

static VOID
HwConfigureRTL8188EE(
		IN	PADAPTER			Adapter
		)
{

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8	regBwOpMode = 0;
	u32	regRATR = 0, regRRSR = 0;


	//1 This part need to modified according to the rate set we filtered!!
	//
	// Set RRSR, RATR, and BW_OPMODE registers
	//
	switch(pHalData->CurrentWirelessMode)
	{
	case WIRELESS_MODE_B:
		regBwOpMode = BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_CCK;
		regRRSR = RATE_ALL_CCK;
		break;
	case WIRELESS_MODE_A:
		regBwOpMode = BW_OPMODE_5G |BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_OFDM_AG;
		regRRSR = RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_G:
		regBwOpMode = BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_UNKNOWN:
	case WIRELESS_MODE_AUTO:
	case WIRELESS_MODE_N_24G:
		// It support CCK rate by default.
		// CCK rate will be filtered out only when associated AP does not support it.
		regBwOpMode = BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_CCK | RATE_ALL_OFDM_AG | RATE_ALL_OFDM_1SS | RATE_ALL_OFDM_2SS;
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_N_5G:
		regBwOpMode = BW_OPMODE_5G;
		regRATR = RATE_ALL_OFDM_AG | RATE_ALL_OFDM_1SS | RATE_ALL_OFDM_2SS;
		regRRSR = RATE_ALL_OFDM_AG;
		break;
	default:
		break;
	}

	rtw_write8(Adapter, REG_INIRTS_RATE_SEL, 0x8);

	// 2007/02/07 Mark by Emily becasue we have not verify whether this register works	
	//For 92C,which reg?
	rtw_write8(Adapter, REG_BWOPMODE, regBwOpMode);


	// Init value for RRSR.
	rtw_write32(Adapter, REG_RRSR, regRRSR);

	// Set SLOT time
	rtw_write8(Adapter,REG_SLOT, 0x09);
	
	// CF-End setting.
	rtw_write16(Adapter,REG_FWHW_TXQ_CTRL, 0x1F80);

	// Set retry limit
	rtw_write16(Adapter,REG_RL, 0x0707);

	// BAR settings
	rtw_write32(Adapter, REG_BAR_MODE_CTRL, 0x0201ffff);

	// HW SEQ CTRL
	rtw_write8(Adapter,REG_HWSEQ_CTRL, 0xFF); //set 0x0 to 0xFF by tynli. Default enable HW SEQ NUM.

	// Set Data / Response auto rate fallack retry count
	rtw_write32(Adapter, REG_DARFRC, 0x01000000);
	rtw_write32(Adapter, REG_DARFRC+4, 0x07060504);
	rtw_write32(Adapter, REG_RARFRC, 0x01000000);
	rtw_write32(Adapter, REG_RARFRC+4, 0x07060504);

#if 0//cosa, for 92s
	if(	(pHalData->bt_coexist.BluetoothCoexist) &&
		(pHalData->bt_coexist.BT_CoexistType == BT_CSR_BC4) )
	{
		PlatformEFIOWrite4Byte(Adapter, REG_AGGLEN_LMT, 0x97427431);
		RTPRINT(FBT, BT_TRACE, ("BT write 0x%x = 0x97427431\n", REG_AGGLEN_LMT));
	}
	else
#endif
	{
		// Aggregation threshold
		rtw_write32(Adapter, REG_AGGLEN_LMT, 0xb972a841);
	}
	
	// Beacon related, for rate adaptive
	rtw_write8(Adapter, REG_ATIMWND, 0x2);

	// For client mode and ad hoc mode TSF setting
	rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xff);

	// 20100211 Joseph: Change original setting of BCN_CTRL(0x550) from 
	// 0x1e(0x2c for test chip) ro 0x1f(0x2d for test chip). Set BIT0 of this register disable ATIM
	// function. Since we do not use HIGH_QUEUE anymore, ATIM function is no longer used.
	// Also, enable ATIM function may invoke HW Tx stop operation. This may cause ping failed
	// sometimes in long run test. So just disable it now.

	//PlatformAtomicExchange((pu4Byte)(&pHalData->RegBcnCtrlVal), 0x1d);
	pHalData->RegBcnCtrlVal = 0x1d;

#ifdef CONFIG_CONCURRENT_MODE
	rtw_write16(Adapter, REG_BCN_CTRL, 0x1010);	// For 2 PORT TSF SYNC
#else
	rtw_write8(Adapter, REG_BCN_CTRL, (u8)(pHalData->RegBcnCtrlVal));
#endif
	// Marked out by Bruce, 2010-09-09.
	// This register is configured for the 2nd Beacon (multiple BSSID). 
	// We shall disable this register if we only support 1 BSSID.
	rtw_write8(Adapter, REG_BCN_CTRL_1, 0);

	// TBTT prohibit hold time. Suggested by designer TimChen.
	rtw_write8(Adapter, REG_TBTT_PROHIBIT+1,0xff); // 8 ms

	// 20091211 Joseph: Do not set 0x551[1] suggested by Scott.
	// 
	// Disable BCNQ SUB1 0x551[1]. Suggested by TimChen. 2009.12.04. by tynli.
	// For protecting HW to decrease the TSF value when temporarily the real TSF value 
	// is smaller than the TSF counter.
	//regTmp = rtw_read8(Adapter, REG_USTIME_TSF);
	//rtw_write8(Adapter, REG_USTIME_TSF, (regTmp|BIT1)); // 8 ms

	rtw_write8(Adapter, REG_PIFS, 0x1C);
	rtw_write8(Adapter, REG_AGGR_BREAK_TIME, 0x16);

#if 0//cosa, for 92s
	if(	(pHalData->bt_coexist.BluetoothCoexist) &&
		(pHalData->bt_coexist.BT_CoexistType == BT_CSR_BC4) )
	{
		PlatformEFIOWrite2Byte(Adapter, REG_NAV_PROT_LEN, 0x0020);
		RTPRINT(FBT, BT_TRACE, ("BT write 0x%x = 0x0020\n", REG_NAV_PROT_LEN));
		PlatformEFIOWrite2Byte(Adapter, REG_PROT_MODE_CTRL, 0x0402);
		RTPRINT(FBT, BT_TRACE, ("BT write 0x%x = 0x0402\n", REG_PROT_MODE_CTRL));
	}
	else
#endif
	{
		rtw_write16(Adapter, REG_NAV_PROT_LEN, 0x0040);
		rtw_write16(Adapter, REG_PROT_MODE_CTRL, 0x08ff);
	}

	if(!Adapter->registrypriv.wifi_spec)
	{
		//For Rx TP. Suggested by SD1 Richard. Added by tynli. 2010.04.12.
		rtw_write32(Adapter, REG_FAST_EDCA_CTRL, 0x03086666);
	}
	else
	{
		//For WiFi WMM. suggested by timchen. Added by tynli.	
		rtw_write16(Adapter, REG_FAST_EDCA_CTRL, 0x0);
		//Nav limit , suggest by scott
		rtw_write8(Adapter, REG_NAV_UPPER, 0x0);
	}

#if (BOARD_TYPE == FPGA_2MAC)
	// ACKTO for IOT issue.
	rtw_write8(Adapter, REG_ACKTO, 0x40);

	// Set Spec SIFS (used in NAV)
	rtw_write16(Adapter,REG_SPEC_SIFS, 0x1010);
	rtw_write16(Adapter,REG_MAC_SPEC_SIFS, 0x1010);

	// Set SIFS for CCK
	rtw_write16(Adapter,REG_SIFS_CTX, 0x1010);	

	// Set SIFS for OFDM
	rtw_write16(Adapter,REG_SIFS_TRX, 0x1010);

#else
	// ACKTO for IOT issue.
	rtw_write8(Adapter, REG_ACKTO, 0x40);

	// Set Spec SIFS (used in NAV)
	rtw_write16(Adapter,REG_SPEC_SIFS, 0x100a);
	rtw_write16(Adapter,REG_MAC_SPEC_SIFS, 0x100a);

	// Set SIFS for CCK
	rtw_write16(Adapter,REG_SIFS_CTX, 0x100a);	

	// Set SIFS for OFDM
	rtw_write16(Adapter,REG_SIFS_TRX, 0x100a);
#endif

	// Set Multicast Address. 2009.01.07. by tynli.
	rtw_write32(Adapter, REG_MAR, 0xffffffff);
	rtw_write32(Adapter, REG_MAR+4, 0xffffffff);

	//Reject all control frame - default value is 0
	rtw_write16(Adapter,REG_RXFLTMAP1,0x0);

	//For 92C,how to??
	//rtw_write8(Adapter, MLT, 0x8f);

	// Set Contention Window here		

	// Set Tx AGC

	// Set Tx Antenna including Feedback control
		
	// Set Auto Rate fallback control
				
	
}

static u8
_LLTWrite(
	IN  PADAPTER	Adapter,
	IN	u32		address,
	IN	u32		data
	)
{
	u8	status = _SUCCESS;
	s8	count = POLLING_LLT_THRESHOLD;
	u32	value = _LLT_INIT_ADDR(address) | _LLT_INIT_DATA(data) | _LLT_OP(_LLT_WRITE_ACCESS);

	rtw_write32(Adapter, REG_LLT_INIT, value);

	//polling
	do{
		value = rtw_read32(Adapter, REG_LLT_INIT);
		if(_LLT_NO_ACTIVE == _LLT_OP_VALUE(value)){
			break;
		}
	}while(--count);
	
	if(count<=0){
		DBG_8192C("Failed to polling write LLT done at address %x!\n", address);
		status = _FAIL;	
	}

	return status;

}
static u32 _InitPowerOn_8188EE(PADAPTER Adapter)
{
	u8	bytetmp,bMacPwrCtrlOn=_FALSE;
	u32	status = _SUCCESS;
	// Independent some steps for RTL8188EE power on sequence.
	// Create RTL8723e power on sequence v01 which suggested by Scott.
	// First modified by tynli. 2011.01.05.
	// Revise for power sequence v07. 2011.03.21. by tynli.

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if(bMacPwrCtrlOn == _TRUE)	
		return status;

	//Disable XTAL OUTPUT for power saving. YJ,add,111206.
	bytetmp = (rtw_read8(Adapter, REG_XCK_OUT_CTRL) & (~BIT0));
	rtw_write8(Adapter, REG_XCK_OUT_CTRL, bytetmp);

	//Auto Power Down to CHIP-off State
	bytetmp = (rtw_read8(Adapter, REG_APS_FSMCO+1) & (~BIT7));
	rtw_write8(Adapter, REG_APS_FSMCO+1, bytetmp);

	// 0.
	rtw_write8(Adapter, REG_RSV_CTRL, 0x0);

	// HW Power on sequence
	if(!HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, Rtl8188E_NIC_ENABLE_FLOW))
		return _FAIL;

	//Enable Power Down Interrupt //YJ,add, 120406
	bytetmp = (rtw_read8(Adapter, REG_APS_FSMCO) | BIT4);
	rtw_write8(Adapter, REG_APS_FSMCO, bytetmp);
	
	// Gate PCIe memory clock automatic.
	bytetmp = rtw_read8(Adapter, REG_PCIE_CTRL_REG+2);
	rtw_write8(Adapter, REG_PCIE_CTRL_REG+2, bytetmp|BIT2);

	//eMAC time out function enable, 0x369[7]=1
	bytetmp = rtw_read8(Adapter, REG_WATCH_DOG+1);
	rtw_write8(Adapter, REG_WATCH_DOG+1, bytetmp|BIT7);

	// Disable 8051
	//rtw_write8(Adapter, REG_SYS_FUNC_EN+1, 0xFB);

	// Gate 40M clock used by USB
	bytetmp = rtw_read8(Adapter, REG_AFE_XTAL_CTRL_EXT+1);
	rtw_write8(Adapter, REG_AFE_XTAL_CTRL_EXT+1, (bytetmp|BIT1));

	bMacPwrCtrlOn = _TRUE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	
	return status;
	
}

#define LLT_CONFIG	5
static u32
LLT_table_init_88E(
	IN PADAPTER	Adapter
	)
{
	HAL_DATA_TYPE		*pHalData	= GET_HAL_DATA(Adapter);
	u32	numHQ		= 0x29;
	u32	numLQ		= 0x0D;
	u32	numNQ		= 0x01;
	u32	numPubQ	= 0x00;
	u32	value32;	//High+low page number
	u16	rxff_bndy = 0;
	u8	value8;
	u8	txpktbuf_bndy;
	RT_STATUS	status;

	DBG_8192C("=====>LLT_table_init_88E\n");
	txpktbuf_bndy = TX_PAGE_BOUNDARY_88E(Adapter);
	//value32 = 0x80730d29;

	value8 = (u8)_NPQ(numNQ);
	numPubQ = TX_TOTAL_PAGE_NUMBER_88E(Adapter) - numHQ - numLQ - numNQ;
	value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;

	// Set reserved page for each queue
	// 11.	RQPN 0x200[31:0]	= 0x80BD1C1C				// load RQPN
	rtw_write8(Adapter, REG_RQPN_NPQ, value8);
	rtw_write32(Adapter, REG_RQPN, value32);

	// 12.	TXRKTBUG_PG_BNDY 0x114[31:0] = 0x21FF00AC	//TXRKTBUG_PG_BNDY	
	rxff_bndy = MAX_RX_DMA_BUFFER_SIZE_88E(Adapter) - 1;
	rtw_write32(Adapter, REG_TRXFF_BNDY, ((rxff_bndy)<<16 | txpktbuf_bndy));

	// 13.	TDECTRL[15:8] 0x209[7:0] = 0xAC				// Beacon Head for TXDMA
	rtw_write8(Adapter,REG_TDECTRL+1, txpktbuf_bndy);

	// 14.	BCNQ_PGBNDY 0x424[7:0] =  0xAC				//BCNQ_PGBNDY
	// 2009/12/03 Why do we set so large boundary. confilct with document V11.
	rtw_write8(Adapter,REG_BCNQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter,REG_MGQ_BDNY, txpktbuf_bndy);

	// 15.	WMAC_LBK_BF_HD 0x45D[7:0] =  0xAC			//WMAC_LBK_BF_HD
	rtw_write8(Adapter,0x45D, txpktbuf_bndy);
	
	// Set Tx/Rx page size (Tx must be 128 Bytes, Rx can be 64,128,256,512,1024 bytes)
	// 16.	PBP [7:0] = 0x11								// TRX page size
	rtw_write8(Adapter,REG_PBP, 0x11);		

	// 17.	DRV_INFO_SZ = 0x04
	rtw_write8(Adapter,REG_RX_DRVINFO_SZ, 0x4);	

	status = InitLLTTable(Adapter, txpktbuf_bndy);

	return status;
	
}

//I don't kown why udelay is not enough for REG_APSD_CTRL+1
//so I add more 200 us for every udelay.
#define MORE_DELAY_VS_WIN

static u32 InitMAC(IN	PADAPTER Adapter)
{
	u8	bytetmp;
	u16	wordtmp;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct recv_priv	*precvpriv = &Adapter->recvpriv;
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	u16	retry = 0;
	u16	tmpU2b = 0;
	u32	boundary, status = _SUCCESS;

	DBG_8192C("=======>InitMAC()\n");

	// Independent some steps for RTL8188EE power on sequence.
	// Create RTL8723e power on sequence v01 which suggested by Scott.
	// First modified by tynli. 2011.01.05.
	// Revise for power sequence v07. 2011.03.21. by tynli.

	//Disable XTAL OUTPUT for power saving. YJ,add,111206.
	bytetmp = (rtw_read8(Adapter, REG_XCK_OUT_CTRL) & (~BIT0));
	rtw_write8(Adapter, REG_XCK_OUT_CTRL, bytetmp);

	//Auto Power Down to CHIP-off State
	bytetmp = (rtw_read8(Adapter, REG_APS_FSMCO+1) & (~BIT7));
	rtw_write8(Adapter, REG_APS_FSMCO+1, bytetmp);

	// 0.
	rtw_write8(Adapter, REG_RSV_CTRL, 0x0);

	// HW Power on sequence
	if(!HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, Rtl8188E_NIC_ENABLE_FLOW))
		return _FAIL;

	//Enable Power Down Interrupt //YJ,add, 120406
	bytetmp = (rtw_read8(Adapter, REG_APS_FSMCO) | BIT4);
	rtw_write8(Adapter, REG_APS_FSMCO, bytetmp);
	
	// Gate PCIe memory clock automatic.
	bytetmp = rtw_read8(Adapter, REG_PCIE_CTRL_REG+2);
	rtw_write8(Adapter, REG_PCIE_CTRL_REG+2, bytetmp|BIT2);

	//eMAC time out function enable, 0x369[7]=1
	bytetmp = rtw_read8(Adapter, REG_WATCH_DOG+1);
	rtw_write8(Adapter, REG_WATCH_DOG+1, bytetmp|BIT7);

	// Disable 8051
	//rtw_write8(Adapter, REG_SYS_FUNC_EN+1, 0xFB);

	// Gate 40M clock used by USB
	bytetmp = rtw_read8(Adapter, REG_AFE_XTAL_CTRL_EXT+1);
	rtw_write8(Adapter, REG_AFE_XTAL_CTRL_EXT+1, (bytetmp|BIT1));
	
	if(1) //Enable Tx RPT
	{
		//Enable Tx RPT Timer
		bytetmp = rtw_read8(Adapter, REG_TX_RPT_CTRL);
		rtw_write8(Adapter, REG_TX_RPT_CTRL, bytetmp|BIT1|BIT0);
		//Set MAX RPT MACID
		rtw_write8(Adapter, REG_TX_RPT_CTRL+1, 2);
		//Tx RPT Timer. Unit: 32us
		rtw_write16(Adapter, REG_TX_RPT_TIME, 0xCdf0);
	}
	
	// Add for wakeup online
	bytetmp = rtw_read8(Adapter, REG_SYS_CLKR);
	rtw_write8(Adapter, REG_SYS_CLKR, (bytetmp|BIT3));
	bytetmp = rtw_read8(Adapter, REG_GPIO_MUXCFG+1);
	rtw_write8(Adapter, REG_GPIO_MUXCFG+1, (bytetmp & ~BIT4));
	rtw_write8(Adapter, 0x367, 0x80);
		
	// Release MAC IO register reset
	// 9.	CR 0x100[7:0]	= 0xFF;
	// 10.	CR 0x101[1]	= 0x01; // Enable SEC block
	rtw_write16(Adapter,REG_CR, 0x2ff);


	// Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
	rtw_write16(Adapter, REG_CR+1, 0x06);
	
//	if(!pHalData->bMACFuncEnable)	
//	{
		//System init
		// 18.	LLT_table_init(Adapter);
		if(LLT_table_init_88E(Adapter) == _FAIL)
		{
			DBG_8192C("Failed to init LLT table\n");		
			return _FAIL;
		}
//	}
	// Clear interrupt and enable interrupt
	// 19.	HISR 0x124[31:0] = 0xffffffff; 
	//	   	HISRE 0x12C[7:0] = 0xFF
	// NO 0x12c now!!!!!
	rtw_write32(Adapter,REG_HISR_88E, 0xffffffff);
	rtw_write32(Adapter,REG_HISRE_88E, 0xffffffff);
	
	// 20.	HIMR 0x120[31:0] |= [enable INT mask bit map]; 
	// 21.	HIMRE 0x128[7:0] = [enable INT mask bit map]
	// The IMR should be enabled later after all init sequence is finished.

	// ========= PCIE related register setting =======
	// 22.	PCIE configuration space configuration
	// 23.	Ensure PCIe Device 0x80[15:0] = 0x0143 (ASPM+CLKREQ), 
	//          and PCIe gated clock function is enabled.	
	// PCIE configuration space will be written after all init sequence.(Or by BIOS)


	//
	// 2009/12/03 MH THe below section is not related to power document Vxx .
	// This is only useful for driver and OS setting.
	//
	// -------------------Software Relative Setting----------------------
	//

	wordtmp = rtw_read16(Adapter,REG_TRXDMA_CTRL);
	wordtmp &= 0xf;
	wordtmp |= 0xE771;	// HIQ->hi, MGQ->normal, BKQ->low, BEQ->hi, VIQ->low, VOQ->hi
	rtw_write16(Adapter,REG_TRXDMA_CTRL, wordtmp);

	
	// Reported Tx status from HW for rate adaptive.
	// 2009/12/03 MH This should be realtive to power on step 14. But in document V11  
	// still not contain the description.!!!
	//rtw_write8(Adapter,REG_FWHW_TXQ_CTRL+1, 0x1F);

	// Set RCR register
	rtw_write32(Adapter,REG_RCR, pHalData->ReceiveConfig);
	rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);
	// Set TCR register
	rtw_write32(Adapter,REG_TCR, pHalData->TransmitConfig);

	//
	// Set TX/RX descriptor physical address(from OS API).
	//
	rtw_write32(Adapter, REG_BCNQ_DESA, (u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_MGQ_DESA, (u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VOQ_DESA, (u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VIQ_DESA, (u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BEQ_DESA, (u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BKQ_DESA, (u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_HQ_DESA, (u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_RX_DESA, (u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma & DMA_BIT_MASK(32));
	
#ifdef CONFIG_64BIT_DMA
	// 2009/10/28 MH For DMA 64 bits. We need to assign the high 32 bit address
	// for NIC HW to transmit data to correct path.
	rtw_write32(Adapter, REG_BCNQ_DESA+4, 
		((u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_MGQ_DESA+4, 
		((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma)>>32);  
	rtw_write32(Adapter, REG_VOQ_DESA+4, 
		((u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_VIQ_DESA+4, 
		((u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_BEQ_DESA+4, 
		((u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_BKQ_DESA+4,
		((u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter,REG_HQ_DESA+4,
		((u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_RX_DESA+4, 
		((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma)>>32);


	// 2009/10/28 MH If RX descriptor address is not equal to zero. We will enable
	// DMA 64 bit functuion.
	// Note: We never saw thd consition which the descripto address are divided into
	// 4G down and 4G upper seperate area.
	if (((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma)>>32 != 0)
	{
		//DBG_8192C("RX_DESC_HA=%08lx\n", ((u64)priv->rx_ring_dma[RX_MPDU_QUEUE])>>32);
		DBG_8192C("Enable DMA64 bit\n");

		// Check if other descriptor address is zero and abnormally be in 4G lower area.
		if (((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma)>>32)
		{
			DBG_8192C("MGNT_QUEUE HA=0\n");
		}
		
		PlatformEnableDMA64(Adapter);
	}
	else
	{
		DBG_8192C("Enable DMA32 bit\n");
	}
#endif

	// 20100318 Joseph: Reset interrupt migration setting when initialization. Suggested by SD1.
	rtw_write32(Adapter, REG_INT_MIG, 0);	
	pHalData->bInterruptMigration = _FALSE;


	//2009.10.19. Reset H2C protection register. by tynli.
	rtw_write32(Adapter, REG_MCUTST_1, 0x0);

#if MP_DRIVER == 1
if (Adapter->registrypriv.mp_mode == 1)
{
	rtw_write32(Adapter, REG_MACID, 0x87654321);
	rtw_write32(Adapter, 0x0700, 0x87654321);
}
#endif

	//Enable RX DMA. //by YJ,120207
	rtw_write8(Adapter, REG_PCIE_CTRL_REG+1, 0);

#if 0
	if(pHalData->bEarlyModeEnable)
	{
		DBG_8192C("EarlyMode Enabled!!!\n");


		bytetmp = rtw_read8(Adapter, REG_EARLY_MODE_CONTROL);
#if RTL8188E_EARLY_MODE_PKT_NUM_10 == 1
		bytetmp = bytetmp|0x1f;
#else
		bytetmp = bytetmp|0xf;
#endif
		rtw_write8(Adapter, REG_EARLY_MODE_CONTROL, bytetmp);

		rtw_write8(Adapter, REG_EARLY_MODE_CONTROL+3, 0x81);

		//rtw_write8(Adapter, REG_MAX_AGGR_NUM, 0x0A); 
	}
#endif	
	//
	// -------------------Software Relative Setting----------------------
	//

	DBG_8192C("<=======InitMAC()\n");

	return _SUCCESS;
	
}

static VOID
EnableAspmBackDoor88EE(IN	PADAPTER Adapter)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);

	// 0x70f BIT7 is used to control L0S
	// 20100212 Tynli: Set register offset 0x70f in PCI configuration space to the value 0x23 
	// for all bridge suggested by SD1. Origianally this is only for INTEL.
	// 20100422 Joseph: Set PCI configuration space offset 0x70F to 0x93 to Enable L0s for all platform.
	// This is suggested by SD1 Glayrainx and for Lenovo's request.
	//if(GetPciBridgeVendor(Adapter) == PCI_BRIDGE_VENDOR_INTEL)
		rtw_write8(Adapter, 0x34b, 0x93);
	//else
	//	rtw_write8(Adapter, 0x34b, 0x23);
	rtw_write16(Adapter, 0x350, 0x870c);
	rtw_write8(Adapter, 0x352, 0x1);

	// 0x719 Bit3 is for L1 BIT4 is for clock request 
	// 20100427 Joseph: Disable L1 for Toshiba AMD platform. If AMD platform do not contain
	// L1 patch, driver shall disable L1 backdoor.
	if(pHalData->bSupportBackDoor)
		rtw_write8(Adapter, 0x349, 0x1b);
	else
		rtw_write8(Adapter, 0x349, 0x03);
	rtw_write16(Adapter, 0x350, 0x2718);
	rtw_write8(Adapter, 0x352, 0x1);
}

static u32 rtl8188ee_hal_init(PADAPTER Adapter)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);
	u32	rtStatus = _SUCCESS;
	u8	tmpU1b;
	u8	eRFPath;
	u32	i;
	BOOLEAN bSupportRemoteWakeUp, is2T2R;

_func_enter_;

	//
	// No I/O if device has been surprise removed
	//
	if (rtw_is_surprise_removed(Adapter))
	{
		DBG_8192C("rtl8188ee_hal_init(): bSurpriseRemoved!\n");
		return _SUCCESS;
	}

	DBG_8192C("=======>rtl8188ee_hal_init()\n");

	//rtl8188ee_reset_desc_ring(Adapter);
       	rtStatus =  _InitPowerOn_8188EE(Adapter);
	if(rtStatus != _SUCCESS)
	{
		DBG_8192C("%s failed\n",__FUNCTION__);
		return rtStatus;
	}
	//
	// 1. MAC Initialize
	//
	rtStatus = InitMAC(Adapter);
	if(rtStatus != _SUCCESS)
	{
		DBG_8192C("Init MAC failed\n");
		return rtStatus;
	}
//#if (MP_DRIVER != 1)
//if (Adapter->registrypriv.mp_mode != 1)
{
#if HAL_FW_ENABLE
	rtStatus = rtl8188e_FirmwareDownload(Adapter, _FALSE);

	if(rtStatus != _SUCCESS)
	{
		DBG_8192C("FwLoad failed\n");
		rtStatus = _SUCCESS;
		Adapter->bFWReady = _FALSE;
		pHalData->fw_ractrl = _FALSE;
	}
	else
	{
		DBG_8192C("FwLoad SUCCESSFULLY!!!\n");
		Adapter->bFWReady = _TRUE;
		#ifdef CONFIG_SFW_SUPPORTED
		pHalData->fw_ractrl = IS_VENDOR_8188E_I_CUT_SERIES(Adapter)?_TRUE:_FALSE;
		#else
		pHalData->fw_ractrl =_FALSE;
		#endif
	}
	rtl8188e_InitializeFirmwareVars(Adapter);
#endif
}
//#endif

// 20100318 Joseph: These setting only for FPGA.
// Add new type "ASIC" and set RFChipID and RF_Type in ReadAdapter function.
#if BOARD_TYPE==FPGA_2MAC
	pHalData->rf_chip = RF_PSEUDO_11N;
	pHalData->rf_type = RF_2T2R;
#elif BOARD_TYPE==FPGA_PHY
	#if FPGA_RF==FPGA_RF_8225
		pHalData->rf_chip = RF_8225;
		pHalData->rf_type = RF_2T2R;
	#elif FPGA_RF==FPGA_RF_0222D
		pHalData->rf_chip = RF_6052;
		pHalData->rf_type = RF_2T2R;
	#endif
#endif

	//
	// 2. Initialize MAC/PHY Config by MACPHY_reg.txt
	//
#if (HAL_MAC_ENABLE == 1)
	DBG_8192C("MAC Config Start!\n");
	rtStatus = PHY_MACConfig8188E(Adapter);
	if (rtStatus != _SUCCESS)
	{
		DBG_8192C("MAC Config failed\n");
		return rtStatus;
	}
	DBG_8192C("MAC Config Finished!\n");

	rtw_write32(Adapter,REG_RCR, rtw_read32(Adapter, REG_RCR)&~(RCR_ADF) );
#endif	// #if (HAL_MAC_ENABLE == 1)

	//
	// 3. Initialize BB After MAC Config PHY_reg.txt, AGC_Tab.txt
	//
#if (HAL_BB_ENABLE == 1)
	DBG_8192C("BB Config Start!\n");
	rtStatus = PHY_BBConfig8188E(Adapter);
	if (rtStatus!= _SUCCESS)
	{
		DBG_8192C("BB Config failed\n");
		return rtStatus;
	}
	DBG_8192C("BB Config Finished!\n");
#endif	// #if (HAL_BB_ENABLE == 1)


	_InitBeaconParameters(Adapter);

	
	//
	// 4. Initiailze RF RAIO_A.txt RF RAIO_B.txt
	//
	// 2007/11/02 MH Before initalizing RF. We can not use FW to do RF-R/W.
	//pHalData->Rf_Mode = RF_OP_By_SW_3wire;
#if (HAL_RF_ENABLE == 1)		
	DBG_8192C("RF Config started!\n");
	rtStatus = PHY_RFConfig8188E(Adapter);
	if(rtStatus != _SUCCESS)
	{
		DBG_8192C("RF Config failed\n");
		return rtStatus;
	}
	DBG_8192C("RF Config Finished!\n");

	// 20100329 Joseph: Restore RF register value for later use in channel switching.
	pHalData->RfRegChnlVal[0] = PHY_QueryRFReg(Adapter, 0, RF_CHNLBW, bRFRegOffsetMask);
	pHalData->RfRegChnlVal[1] = PHY_QueryRFReg(Adapter, 1, RF_CHNLBW, bRFRegOffsetMask);
#endif	// #if (HAL_RF_ENABLE == 1)	

	// After read predefined TXT, we must set BB/MAC/RF register as our requirement
	/*---- Set CCK and OFDM Block "ON"----*/
	PHY_SetBBReg(Adapter, rFPGA0_RFMOD, bCCKEn, 0x1);
	PHY_SetBBReg(Adapter, rFPGA0_RFMOD, bOFDMEn, 0x1);
#if (MP_DRIVER == 0)
	// Set to 20MHz by default
	// PHY_SetBBReg(Adapter, rFPGA0_AnalogParameter2, BIT10, 1);  //Aries mark, 2012/06/01, sync with windows driver 
#endif

	pHalData->CurrentWirelessMode = WIRELESS_MODE_AUTO;

	//3 Set Hardware(MAC default setting.)
	HwConfigureRTL8188EE(Adapter);

	//3 Set Wireless Mode
	// TODO: Emily 2006.07.13. Wireless mode should be set according to registry setting and RF type
	//Default wireless mode is set to "WIRELESS_MODE_N_24G|WIRELESS_MODE_G", 
	//and the RRSR is set to Legacy OFDM rate sets. We do not include the bit mask 
	//of WIRELESS_MODE_B currently. Emily, 2006.11.13
	//For wireless mode setting from mass. 
	//if(Adapter->ResetProgress == RESET_TYPE_NORESET)
	//	Adapter->HalFunc.SetWirelessModeHandler(Adapter, Adapter->RegWirelessMode);

	//3Security related
	//-----------------------------------------------------------------------------
	// Set up security related. 070106, by rcnjko:
	// 1. Clear all H/W keys.
	//-----------------------------------------------------------------------------
	invalidate_cam_all(Adapter);

	// Joseph debug: MAC_SEC_EN need to be set
	rtw_write8(Adapter, REG_CR+1, (rtw_read8(Adapter, REG_CR+1)|BIT1));

	pHalData->CurrentChannel = 6;//default set to 6

	/* Write correct tx power index */
	PHY_SetTxPowerLevel8188E(Adapter, pHalData->CurrentChannel);

	//2=======================================================
	// RF Power Save
	//2=======================================================
#if 1
	// Fix the bug that Hw/Sw radio off before S3/S4, the RF off action will not be executed 
	// in MgntActSet_RF_State() after wake up, because the value of pHalData->eRFPowerState 
	// is the same as eRfOff, we should change it to eRfOn after we config RF parameters.
	// Added by tynli. 2010.03.30.
	pwrpriv->rf_pwrstate = rf_on;

	// 20100326 Joseph: Copy from GPIOChangeRFWorkItemCallBack() function to check HW radio on/off.
	// 20100329 Joseph: Revise and integrate the HW/SW radio off code in initialization.
	tmpU1b = rtw_read8(Adapter, REG_MAC_PINMUX_CFG)&(~BIT3);
	rtw_write8(Adapter, REG_MAC_PINMUX_CFG, tmpU1b);
	tmpU1b = rtw_read8(Adapter, REG_GPIO_IO_SEL);
	DBG_8192C("GPIO_IN=%02x\n", tmpU1b);
	pwrpriv->rfoff_reason |= (tmpU1b & BIT3) ? 0 : RF_CHANGE_BY_HW;
	pwrpriv->rfoff_reason |= (pwrpriv->reg_rfoff) ? RF_CHANGE_BY_SW : 0;

	if(pwrpriv->rfoff_reason & RF_CHANGE_BY_HW)
		pwrpriv->b_hw_radio_off = _TRUE;

	if(pwrpriv->rfoff_reason > RF_CHANGE_BY_PS)
	{ // H/W or S/W RF OFF before sleep.
		DBG_8192C("InitializeAdapter8188EE(): Turn off RF for RfOffReason(%d) ----------\n", pwrpriv->rfoff_reason);
		//MgntActSet_RF_State(Adapter, rf_off, pwrpriv->rfoff_reason, _TRUE);
	}
	else
	{
		pwrpriv->rf_pwrstate = rf_on;
		pwrpriv->rfoff_reason = 0;

		DBG_8192C("InitializeAdapter8188EE(): Turn on  ----------\n");

		// LED control
		rtw_led_control(Adapter, LED_CTL_POWER_ON);

		//
		// If inactive power mode is enabled, disable rf while in disconnected state.
		// But we should still tell upper layer we are in rf on state.
		// 2007.07.16, by shien chang.
		//
		//if(!Adapter->bInHctTest)
			//IPSEnter(Adapter);
	}
#endif

	// Fix the bug that when the system enters S3/S4 then tirgger HW radio off, after system
	// wakes up, the scan OID will be set from upper layer, but we still in RF OFF state and scan
	// list is empty, such that the system might consider the NIC is in RF off state and will wait 
	// for several seconds (during this time the scan OID will not be set from upper layer anymore)
	// even though we have already HW RF ON, so we tell the upper layer our RF state here.
	// Added by tynli. 2010.04.01.
	//DrvIFIndicateCurrentPhyStatus(Adapter);

	if(Adapter->registrypriv.hw_wps_pbc)
	{
		tmpU1b = rtw_read8(Adapter, GPIO_IO_SEL);
		tmpU1b &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
		rtw_write8(Adapter, GPIO_IO_SEL, tmpU1b);	//enable GPIO[2] as input mode
	}

	//
	// Execute TX power tracking later
	//

	hal_init_macaddr(Adapter);

	// Joseph. Turn on the secret lock of ASPM.
	EnableAspmBackDoor88EE(Adapter);

#ifdef CONFIG_BT_COEXIST
	_InitBTCoexist(Adapter);
#endif

	rtl8188e_InitHalDm(Adapter);

	rtw_write32(Adapter,REG_MACID_NO_LINK_0,0xFFFFFFFF);
	rtw_write32(Adapter,REG_MACID_NO_LINK_1,0xFFFFFFFF);
	
#if defined(CONFIG_CONCURRENT_MODE) || defined(CONFIG_TX_MCAST2UNI)
#ifdef CONFIG_CHECK_AC_LIFETIME
	// Enable lifetime check for the four ACs
	rtw_write8(Adapter, REG_LIFETIME_CTRL, 0x0F);
#endif	// CONFIG_CHECK_AC_LIFETIME

#ifdef CONFIG_TX_MCAST2UNI
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x0400);	// unit: 256us. 256ms
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x0400);	// unit: 256us. 256ms
#else	// CONFIG_TX_MCAST2UNI
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x3000);	// unit: 256us. 3s
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x3000);	// unit: 256us. 3s
#endif	// CONFIG_TX_MCAST2UNI
#endif	// CONFIG_CONCURRENT_MODE || CONFIG_TX_MCAST2UNI


	//enable tx DMA to drop the redundate data of packet
	rtw_write16(Adapter,REG_TXDMA_OFFSET_CHK, (rtw_read16(Adapter,REG_TXDMA_OFFSET_CHK) | DROP_DATA_EN));

	pHalData->RegBcnCtrlVal = rtw_read8(Adapter, REG_BCN_CTRL);
	pHalData->RegTxPause = rtw_read8(Adapter, REG_TXPAUSE); 
	pHalData->RegFwHwTxQCtrl = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL+2);
	pHalData->RegReg542 = rtw_read8(Adapter, REG_TBTT_PROHIBIT+2);
	pHalData->RegCR_1 = rtw_read8(Adapter, REG_CR+1);

	//EnableInterrupt8188EE(Adapter);

#if (MP_DRIVER == 1)
if (Adapter->registrypriv.mp_mode == 1)
{
	Adapter->mppriv.channel = pHalData->CurrentChannel;
	MPT_InitializeAdapter(Adapter, Adapter->mppriv.channel);
}
#endif
{
	// 20100329 Joseph: Disable te Caliberation operation when Radio off.
	// This prevent from outputing signal when initialization in Radio-off state.
	if(pwrpriv->rf_pwrstate == rf_on)
	{
//		if(Adapter->ledpriv.LedStrategy != SW_LED_MODE10)
//			PHY_SetRFPathSwitch_8188E(Adapter, pHalData->bDefaultAntenna);	//Wifi default use Main

		if(pHalData->bIQKInitialized )
			PHY_IQCalibrate_8188E(Adapter, _TRUE);
		else
		{
			PHY_IQCalibrate_8188E(Adapter, _FALSE);
			pHalData->bIQKInitialized = _TRUE;
		}		
		ODM_TXPowerTrackingCheck(&pHalData->odmpriv );
		PHY_LCCalibrate_8188E(&pHalData->odmpriv );
	}
}

#if 0
	//WoWLAN setting. by tynli.
	rtw_hal_get_def_var(Adapter, HAL_DEF_WOWLAN , &bSupportRemoteWakeUp);
	if(bSupportRemoteWakeUp) // WoWLAN setting. by tynli.
	{
		u8	u1bTmp;
		u8	i;
#if 0
		u4Byte	u4bTmp;

		//Disable L2 support
		u4bTmp = PlatformEFIORead4Byte(Adapter, REG_PCIE_CTRL_REG);
		u4bTmp &= ~(BIT17);
		 PlatformEFIOWrite4Byte(Adapter, REG_PCIE_CTRL_REG, u4bTmp);
#endif

		// enable Rx DMA. by tynli.
		u1bTmp = rtw_read8(Adapter, REG_RXPKT_NUM+2);
		u1bTmp &= ~(BIT2);
		rtw_write8(Adapter, REG_RXPKT_NUM+2, u1bTmp);

		if(pPSC->WoWLANMode == eWakeOnMagicPacketOnly)
		{
			//Enable magic packet and WoWLAN function in HW.
			rtw_write8(Adapter, REG_WOW_CTRL, WOW_MAGIC);
		}
		else if (pPSC->WoWLANMode == eWakeOnPatternMatchOnly)
		{
			//Enable pattern match and WoWLAN function in HW.
			rtw_write8(Adapter, REG_WOW_CTRL, WOW_WOMEN);
		}
		else if (pPSC->WoWLANMode == eWakeOnBothTypePacket)
		{
			//Enable magic packet, pattern match, and WoWLAN function in HW.
			rtw_write8(Adapter, REG_WOW_CTRL, WOW_MAGIC|WOW_WOMEN); 
		}
		
		PlatformClearPciPMEStatus(Adapter);

		if(ADAPTER_TEST_STATUS_FLAG(Adapter, ADAPTER_STATUS_FIRST_INIT))
		{
			//Reset WoWLAN register and related data structure at the first init. 2009.06.18. by tynli.
			ResetWoLPara(Adapter);
		}
		else
		{
			if(pPSC->WoWLANMode > eWakeOnMagicPacketOnly)
			{
				//Rewrite WOL pattern and mask to HW.
				for(i=0; i<(MAX_SUPPORT_WOL_PATTERN_NUM-2); i++)
				{
					rtw_hal_set_hwreg(Adapter, HW_VAR_WF_MASK, (pu1Byte)(&i)); 
					rtw_hal_set_hwreg(Adapter, HW_VAR_WF_CRC, (pu1Byte)(&i)); 
				}
			}
		}
	}
#endif

	pHalData->RegFwHwTxQCtrl = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL+2);


	is2T2R = IS_2T2R(pHalData->VersionID);

	tmpU1b = EFUSE_Read1Byte(Adapter, 0x1FA);

	if(!(tmpU1b & BIT0))
	{
		PHY_SetRFReg(Adapter, RF_PATH_A, 0x15, 0x0F, 0x05);
		DBG_8192C("PA BIAS path A\n");
	}	


	if(!(tmpU1b & BIT1) && is2T2R)
	{
		PHY_SetRFReg(Adapter, RF_PATH_B, 0x15, 0x0F, 0x05);
		DBG_8192C("PA BIAS path B\n");
	}

	if(!(tmpU1b & BIT4))
	{
		tmpU1b = rtw_read8(Adapter, 0x16);
		tmpU1b &= 0x0F; 
		rtw_write8(Adapter, 0x16, tmpU1b | 0x80);
		rtw_udelay_os(10);
		rtw_write8(Adapter, 0x16, tmpU1b | 0x90);
		DBG_8192C("under 1.5V\n");
	}

/*{
	DBG_8192C("===== Start Dump Reg =====");
	for(i = 0 ; i <= 0xeff ; i+=4)
	{
		if(i%16==0)
			DBG_8192C("\n%04x: ",i);
		DBG_8192C("0x%08x ",rtw_read32(Adapter, i));
	}
	DBG_8192C("\n ===== End Dump Reg =====\n");
}*/
	#if defined(CONFIG_IOL_EFUSE_PATCH)	
	rtStatus = rtl8188e_iol_efuse_patch(Adapter);
	#endif

_func_exit_;

	return rtStatus;
}

//
// 2009/10/13 MH Acoording to documetn form Scott/Alfred....
// This is based on version 8.1.
//




VOID
hal_poweroff_8188ee(
	IN	PADAPTER			Adapter 
)
{
	u8 bMacPwrCtrlOn = _FALSE;
	u1Byte		u1bTmp;
	int count = 0;

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if(bMacPwrCtrlOn == _FALSE)	
		return;
	
	DBG_8192C("=====>%s\n",__FUNCTION__);

	//YJ,add,111208
	//Stop Tx Report Timer. 0x4EC[Bit1]=b'0
	u1bTmp = rtw_read8(Adapter, REG_TX_RPT_CTRL);
	rtw_write8(Adapter, REG_TX_RPT_CTRL, u1bTmp&(~BIT1));

	//Polling Rx DMA Idle. 0x286[BIT1]=b'1
	u1bTmp = rtw_read8(Adapter, REG_RXPKT_NUM+2);
	while(!(u1bTmp & BIT(1)) &&(count++ < 100))
	{
		rtw_udelay_os(10); // 10 us
		u1bTmp = rtw_read8(Adapter, REG_RXPKT_NUM+2);
	}
	
	//Stop RX DMA
	rtw_write8(Adapter, REG_PCIE_CTRL_REG+1, 0xFF);
	//YJ,add,111208,end	

	// Combo (PCIe + USB) Card and PCIe-MF Card
	// 1. Run LPS WL RFOFF flow
	HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, Rtl8188E_NIC_LPS_ENTER_FLOW);
	
	// 2. 0x1F[7:0] = 0		// turn off RF
	rtw_write8(Adapter, REG_RF_CTRL, 0x00);

	//	==== Reset digital sequence   ======
	if((rtw_read8(Adapter, REG_MCUFWDL)&BIT7) && 
		Adapter->bFWReady) //8051 RAM code
	{	
		_8051Reset88E(Adapter);
	}

	// Reset MCU. Suggested by Filen. 2011.01.26. by tynli.
	u1bTmp = rtw_read8(Adapter, REG_SYS_FUNC_EN+1);
	rtw_write8(Adapter, REG_SYS_FUNC_EN+1, (u1bTmp&(~BIT2)));

	// g.	MCUFWDL 0x80[1:0]=0				// reset MCU ready status
	rtw_write8(Adapter, REG_MCUFWDL, 0x00);

	//YJ,add,111212
	//Disable 32k
	u1bTmp = rtw_read8(Adapter, REG_32K_CTRL);
	rtw_write8(Adapter, REG_32K_CTRL, u1bTmp&(~BIT0));
	
	// HW card disable configuration.
	HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, Rtl8188E_NIC_DISABLE_FLOW);	

	// Reset MCU IO Wrapper
	u1bTmp = rtw_read8(Adapter, REG_RSV_CTRL+1);
	rtw_write8(Adapter, REG_RSV_CTRL+1, (u1bTmp&(~BIT3)));	
	u1bTmp = rtw_read8(Adapter, REG_RSV_CTRL+1);
	rtw_write8(Adapter, REG_RSV_CTRL+1, u1bTmp|BIT3);

	// 7. RSV_CTRL 0x1C[7:0] = 0x0E			// lock ISO/CLK/Power control register
	rtw_write8(Adapter, REG_RSV_CTRL, 0x0e);

	//YJ,test add, 111207. For Power Consumption.
	u1bTmp = rtw_read8(Adapter, GPIO_IN);
	rtw_write8(Adapter, GPIO_OUT, u1bTmp);
	rtw_write8(Adapter, GPIO_IO_SEL, 0x7F);

	u1bTmp = rtw_read8(Adapter, REG_GPIO_IO_SEL);
	//rtw_write8(Adapter, REG_GPIO_IO_SEL, (u1bTmp<<4)|u1bTmp);
	rtw_write8(Adapter, REG_GPIO_IO_SEL, (u1bTmp<<4));
	u1bTmp = rtw_read8(Adapter, REG_GPIO_IO_SEL+1);
	rtw_write8(Adapter, REG_GPIO_IO_SEL+1, u1bTmp|0x0F);
	Adapter->bFWReady = _FALSE;

	bMacPwrCtrlOn = _FALSE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);	
}

static u32 rtl8188ee_hal_deinit(PADAPTER Adapter)
{
	u8	u1bTmp = 0;
	u8	bSupportRemoteWakeUp = _FALSE;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);
	
_func_enter_;

	if (Adapter->bHaltInProgress == _TRUE)
	{
		DBG_8192C("====> Abort rtl8188ee_hal_deinit()\n");
		return _FAIL;
	}

	Adapter->bHaltInProgress = _TRUE;

	//
	// No I/O if device has been surprise removed
	//
	if (rtw_is_surprise_removed(Adapter)) {
		Adapter->bHaltInProgress = _FALSE;
		return _SUCCESS;
	}

	Adapter->bDriverIsGoingToUnload = _TRUE;

	RT_SET_PS_LEVEL(pwrpriv, RT_RF_OFF_LEVL_HALT_NIC);

	// Without supporting WoWLAN or the driver is in awake (D0) state, we should 	
	if(!bSupportRemoteWakeUp )//||!pMgntInfo->bPwrSaveState) 
	{
		// 2009/10/13 MH For power off test.
		hal_poweroff_8188ee(Adapter);	
	}
	else
	{
		u8	bSleep = _TRUE;

		//RxDMA
		//tynli_test 2009.12.16.
		u1bTmp = rtw_read8(Adapter, REG_RXPKT_NUM+2);
		rtw_write8(Adapter, REG_RXPKT_NUM+2, u1bTmp|BIT2);	

		//PlatformDisableASPM(Adapter);
		rtw_hal_set_hwreg(Adapter, HW_VAR_SWITCH_EPHY_WoWLAN, (u8 *)&bSleep);

		//tynli_test. 2009.12.17.
		u1bTmp = rtw_read8(Adapter, REG_SPS0_CTRL);
		rtw_write8(Adapter, REG_SPS0_CTRL, (u1bTmp|BIT1));

		//
		rtw_write8(Adapter, REG_APS_FSMCO+1, 0x0);

		PlatformClearPciPMEStatus(Adapter);

		// tynli_test for normal chip wowlan. 2010.01.26. Suggested by Sd1 Isaac and designer Alfred.
		rtw_write8(Adapter, REG_SYS_CLKR, (rtw_read8(Adapter, REG_SYS_CLKR)|BIT3));

		//prevent 8051 to be reset by PERST#
		rtw_write8(Adapter, REG_RSV_CTRL, 0x20);
		rtw_write8(Adapter, REG_RSV_CTRL, 0x60);
	}

	Adapter->bHaltInProgress = _FALSE;

_func_exit_;

	return _SUCCESS;
}

void SetHwReg8188EE(PADAPTER Adapter, u8 variable, u8* val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

_func_enter_;

	switch(variable)
	{
		case HW_VAR_SET_RPWM:
			rtw_write8(Adapter, REG_PCIE_HRPWM, *((u8 *)val));
			break;
		case HW_VAR_PCIE_STOP_TX_DMA:
			rtw_write16(Adapter, REG_PCIE_CTRL_REG, 0xff00);
			break;
		default:
			SetHwReg8188E(Adapter, variable, val);
			break;
	}

_func_exit_;
}

void GetHwReg8188EE(PADAPTER Adapter, u8 variable, u8* val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);	
_func_enter_;

	switch(variable)
	{
		default:
			GetHwReg8188E(Adapter, variable, val);
			break;
	}

_func_exit_;
}

//
//	Description: 
//		Query setting of specified variable.
//
u8
GetHalDefVar8188EE(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch(eVariable)
	{			
		case HW_VAR_MAX_RX_AMPDU_FACTOR:
			*(( u32*)pValue) = MAX_AMPDU_FACTOR_64K;
			break;

		case HAL_DEF_TX_LDPC:
		case HAL_DEF_RX_LDPC:
			*((u8 *)pValue) = _FALSE;
			break;
		case HAL_DEF_TX_STBC:
			*((u8 *)pValue) = 0;
			break;
		case HAL_DEF_RX_STBC:
			*((u8 *)pValue) = 1;
			break;			
		case HAL_DEF_PCI_SUUPORT_L1_BACKDOOR:		
			*((PBOOLEAN)pValue) = pHalData->bSupportBackDoor;		
			break;

		case HAL_DEF_PCI_AMD_L1_SUPPORT:
			*((PBOOLEAN)pValue) = _TRUE;// Support L1 patch on AMD platform in default, added by Roger, 2012.04.30.
			break;
		default:
			bResult = GetHalDefVar8188E(Adapter, eVariable, pValue);
			break;
	}

	return bResult;
}


//
//	Description:
//		Change default setting of specified variable.
//
u8
SetHalDefVar8188EE(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch(eVariable)
	{
		case HAL_DEF_PCI_SUUPORT_L1_BACKDOOR:
			pHalData->bSupportBackDoor = *((PBOOLEAN)pValue);
			break;
		default:
			bResult = SetHalDefVar(Adapter, eVariable, pValue);
			break;
	}

	return bResult;
}

static VOID
_BeaconFunctionEnable(
	IN	PADAPTER		Adapter,
	IN	BOOLEAN			Enable,
	IN	BOOLEAN			Linked
	)
{
	rtw_write8(Adapter, REG_BCN_CTRL, (BIT4 | BIT3 | BIT1));
	//SetBcnCtrlReg(Adapter, (BIT4 | BIT3 | BIT1), 0x00);
	//RT_TRACE(COMP_BEACON, DBG_LOUD, ("_BeaconFunctionEnable 0x550 0x%x\n", PlatformEFIORead1Byte(Adapter, 0x550)));			

	rtw_write8(Adapter, REG_RD_CTRL+1, 0x6F);
}

void SetBeaconRelatedRegisters8188EE(PADAPTER padapter)
{
	u32	value32;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u32 bcn_ctrl_reg = REG_BCN_CTRL;

#ifdef CONFIG_CONCURRENT_MODE
        if (padapter->iface_type == IFACE_PORT1){
		bcn_ctrl_reg = REG_BCN_CTRL_1;
        }
#endif
	//
	// Beacon interval (in unit of TU).
	//
	rtw_write16(padapter, REG_BCN_INTERVAL, pmlmeinfo->bcn_interval);
	//
	// ATIM window
	//
	rtw_write16(padapter, REG_ATIMWND, 0x2);
	_InitBeaconParameters(padapter);	

	
	//
	// Beacon interval (in unit of TU).
	//
	rtw_write16(padapter, REG_BCN_INTERVAL, pmlmeinfo->bcn_interval);
	//2008.10.24 added by tynli for beacon changed.
	//PHY_SetBeaconHwReg( Adapter, BcnInterval );

	//
	// DrvErlyInt (in unit of TU). (Time to send interrupt to notify driver to change beacon content)
	//
	//rtw_write8(Adapter, BCN_DMA_INT_92C+1, 0xC);

	//
	// BcnDMATIM(in unit of us). Indicates the time before TBTT to perform beacon queue DMA 
	//
	//PlatformEFIOWrite2Byte(Adapter, BCN_DMATIM_92C, 256); // HWSD suggest this value 2006.11.14

	//
	// Force beacon frame transmission even after receiving beacon frame from other ad hoc STA
	//
	//PlatformEFIOWrite2Byte(Adapter, BCN_ERRTH_92C, 100); // Reference from WMAC code 2006.11.14
	//suggest by wl, 20090902
	rtw_write16(padapter, REG_BCNTCFG, 0x660f);

	//For throughput
	//PlatformEFIOWrite2Byte(Adapter,TBTT_PROHIBIT_92C,0x0202);
	//suggest by wl, 20090902
	//rtw_write8(Adapter,REG_RXTSF_OFFSET_CCK, 0x30);	
	//rtw_write8(Adapter,REG_RXTSF_OFFSET_OFDM, 0x30);
	
	// Suggested by TimChen. 2009.01.25.
	// Rx RF to MAC data path time.
	rtw_write8(padapter,REG_RXTSF_OFFSET_CCK, 0x18);	
	rtw_write8(padapter,REG_RXTSF_OFFSET_OFDM, 0x18);

	rtw_write8(padapter,0x606, 0x30);

	//
	// Update interrupt mask for IBSS.
	//
	UpdateInterruptMask8188EE( padapter, RT_BCN_INT_MASKS, 0, 0, 0);

	_BeaconFunctionEnable(padapter, _TRUE, _TRUE);

	ResumeTxBeacon(padapter);

	//rtw_write8(padapter, 0x422, rtw_read8(padapter, 0x422)|BIT(6));
	
	//rtw_write8(padapter, 0x541, 0xff);

	//rtw_write8(padapter, 0x542, rtw_read8(padapter, 0x541)|BIT(0));

	rtw_write8(padapter, bcn_ctrl_reg, rtw_read8(padapter, bcn_ctrl_reg)|BIT(1));

}

static void rtl8188ee_init_default_value(_adapter * padapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 i;

	rtl8188e_init_default_value(padapter);

	//init default value
	pHalData->fw_ractrl = _FALSE;
	pHalData->LastHMEBoxNum = 0;
	pHalData->bIQKInitialized = _FALSE;
	pHalData->bDefaultAntenna = 1;

	//
	// Set TCR-Transmit Control Register. The value is set in InitializeAdapter8190Pci()
	//
	pHalData->TransmitConfig = CFENDFORM | BIT12 | BIT13;

	//
	// Set RCR-Receive Control Register . The value is set in InitializeAdapter8190Pci().
	//
	pHalData->ReceiveConfig = (\
		//RCR_APPFCS	
		// | RCR_APWRMGT
		// |RCR_ADD3
		// | RCR_ADF |
		RCR_AMF | RCR_APP_MIC| RCR_APP_ICV
		/*| RCR_AICV | RCR_ACRC32*/			/* Accept ICV error, CRC32 Error */
		| RCR_AB | RCR_AM			// Accept Broadcast, Multicast	 
     		| RCR_APM 					// Accept Physical match
     		//| RCR_AAP					// Accept Destination Address packets
     		| RCR_APP_PHYST_RXFF				// Accept PHY status
     		| RCR_HTC_LOC_CTRL
		);

#if (1 == RTL8188E_RX_PACKET_INCLUDE_CRC)
	pHalData->ReceiveConfig |= RCR_ACRC32;
#endif
	//
	// Set Interrupt Mask Register
	//
	// Make reference from WMAC code 2006.10.02, maybe we should disable some of the interrupt. by Emily
	pHalData->IntrMask[0]	= (u32)(			\
								IMR_ROK_88E		|
								IMR_RDU_88E		|
								IMR_VODOK_88E		|
								IMR_VIDOK_88E		|
								IMR_BEDOK_88E 		|
								IMR_BKDOK_88E		|
								IMR_MGNTDOK_88E 	|								
								IMR_HIGHDOK_88E 	|
//								IMR_CPWM_88E		|
//								IMR_CPWM2_88E		|
								IMR_C2HCMD_88E		|
//								IMR_HISR1_IND_INT_88E	|
//								IMR_ATIMEND_88E		|
//								IMR_BCNDMAINT_E_88E	|
//								IMR_HSISR_IND_ON_INT_88E	|
								IMR_BCNDERR0_88E		|
//								IMR_BCNDMAINT0_88E	|
//								IMR_TSF_BIT32_TOGGLE_88E	|
								IMR_TBDOK_88E		|
								IMR_TBDER_88E		|
//								IMR_GTINT3_88E		|
//								IMR_GTINT4_88E		|
								IMR_PSTIMEOUT_88E	|
//								IMR_TXCCK_88E		|
								0);
	pHalData->IntrMask[1] 	= (u32)(\
//								IMR_BCNDMAINT7_88E	|
//								IMR_BCNDMAINT6_88E	|
//								IMR_BCNDMAINT5_88E	|
//								IMR_BCNDMAINT4_88E	|
//								IMR_BCNDMAINT3_88E	|
//								IMR_BCNDMAINT2_88E	|
//								IMR_BCNDMAINT1_88E	|
//								IMR_BCNDOK7_88E		|
//								IMR_BCNDOK6_88E		|
//								IMR_BCNDOK5_88E		|
//								IMR_BCNDOK4_88E		|
//								IMR_BCNDOK3_88E		|
//								IMR_BCNDOK2_88E		|
//								IMR_BCNDOK1_88E		|
//								IMR_ATIMEND_E_88E	|
//								IMR_TXERR_88E		|
//								IMR_RXERR_88E		|
//								IMR_TXFOVW_88E		|
								IMR_RXFOVW_88E		|
								0);
	pHalData->IntrMaskToSet[0] = pHalData->IntrMask[0];
	pHalData->IntrMaskToSet[1] = pHalData->IntrMask[1];

	//init dm default value
	pHalData->bIQKInitialized = _FALSE;
	pHalData->odmpriv.RFCalibrateInfo.TM_Trigger = 0;//for IQK
	pHalData->pwrGroupCnt = 0;
	pHalData->PGMaxGroup= 13;
	pHalData->odmpriv.RFCalibrateInfo.ThermalValue_HP_index = 0;
	for(i = 0; i < HP_THERMAL_NUM; i++)
		pHalData->odmpriv.RFCalibrateInfo.ThermalValue_HP[i] = 0;
	pHalData->EfuseHal.fakeEfuseBank = 0;
	pHalData->EfuseHal.fakeEfuseUsedBytes = 0;
	_rtw_memset(pHalData->EfuseHal.fakeEfuseContent, 0xFF, EFUSE_MAX_HW_SIZE);
	_rtw_memset(pHalData->EfuseHal.fakeEfuseInitMap, 0xFF, EFUSE_MAX_MAP_LEN);
	_rtw_memset(pHalData->EfuseHal.fakeEfuseModifiedMap, 0xFF, EFUSE_MAX_MAP_LEN);
}

void rtl8188ee_set_hal_ops(_adapter * padapter)
{
	struct hal_ops	*pHalFunc = &padapter->HalFunc;

_func_enter_;
	
	pHalFunc->hal_power_on = _InitPowerOn_8188EE;
	pHalFunc->hal_power_off = hal_poweroff_8188ee;
		
	pHalFunc->hal_init = &rtl8188ee_hal_init;
	pHalFunc->hal_deinit = &rtl8188ee_hal_deinit;

	pHalFunc->inirp_init = &rtl8188ee_init_desc_ring;
	pHalFunc->inirp_deinit = &rtl8188ee_free_desc_ring;
	pHalFunc->irp_reset = &rtl8188ee_reset_desc_ring;

	pHalFunc->init_xmit_priv = &rtl8188ee_init_xmit_priv;
	pHalFunc->free_xmit_priv = &rtl8188ee_free_xmit_priv;

	pHalFunc->init_recv_priv = &rtl8188ee_init_recv_priv;
	pHalFunc->free_recv_priv = &rtl8188ee_free_recv_priv;

#ifdef CONFIG_SW_LED
	pHalFunc->InitSwLeds = &rtl8188ee_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8188ee_DeInitSwLeds;
#else //case of hw led or no led
	pHalFunc->InitSwLeds = NULL;
	pHalFunc->DeInitSwLeds = NULL;	
#endif //CONFIG_SW_LED

	pHalFunc->init_default_value = &rtl8188ee_init_default_value;
	pHalFunc->intf_chip_configure = &rtl8188ee_interface_configure;
	pHalFunc->read_adapter_info = &ReadAdapterInfo8188EE;

	pHalFunc->enable_interrupt = &EnableInterrupt8188EE;
	pHalFunc->disable_interrupt = &DisableInterrupt8188EE;
	pHalFunc->interrupt_handler = &rtl8188ee_interrupt;
 
	pHalFunc->SetHwRegHandler = &SetHwReg8188EE;
	pHalFunc->GetHwRegHandler = &GetHwReg8188EE;
  	pHalFunc->GetHalDefVarHandler = &GetHalDefVar8188EE;
 	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8188EE;

	pHalFunc->SetBeaconRelatedRegistersHandler = &SetBeaconRelatedRegisters8188EE;

	pHalFunc->hal_xmit = &rtl8188ee_hal_xmit;
	pHalFunc->mgnt_xmit = &rtl8188ee_mgnt_xmit;
	pHalFunc->hal_xmitframe_enqueue = &rtl8188ee_hal_xmitframe_enqueue;

#ifdef CONFIG_HOSTAPD_MLME
	pHalFunc->hostap_mgnt_xmit_entry = &rtl8188ee_hostap_mgnt_xmit_entry;
#endif

	rtl8188e_set_hal_ops(pHalFunc);
	
_func_exit_;

}

