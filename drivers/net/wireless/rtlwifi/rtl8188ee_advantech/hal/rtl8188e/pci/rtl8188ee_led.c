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

#include "drv_types.h"
#include <rtl8188e_hal.h>

#ifdef CONFIG_RTW_SW_LED

/* ********************************************************************************
 *	Prototype of protected function.
 * ******************************************************************************** */

/* ********************************************************************************
 * LED_819xUsb routines.
 * ******************************************************************************** */

/*
 *	Description:
 *		Turn on LED according to LedPin specified.
 *   */
static void
SwLedOn_8188EE(
	_adapter			*padapter,
	PLED_PCIE		pLed
)
{
	u8	LedCfg;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct led_priv	*ledpriv = adapter_to_led(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	/* 2009/10/26 MH Issau if tyhe device is 8c DID is 0x8176, we need to enable bit6 to */
	/* enable GPIO8 for controlling LED.	 */

	switch (pLed->LedPin) {
	case LED_PIN_GPIO0:
		break;

	case LED_PIN_LED0:
		if (ledpriv->LedStrategy == SW_LED_MODE10) {
			/* RTW_INFO("In SwLedOn SW_LED_MODE10, LedAddr:%X LEDPIN=%d\n",REG_LEDCFG0, pLed->LedPin); */

			LedCfg = rtw_read8(padapter, REG_LEDCFG0);
			rtw_write8(padapter, REG_LEDCFG0, LedCfg & 0x10); /* SW control led0 on.			 */
		} else {
			/* RTW_INFO("In SwLedOn,LedAddr:%X LEDPIN=%d\n",REG_LEDCFG2, pLed->LedPin); */

			LedCfg = rtw_read8(padapter, REG_LEDCFG2);
			rtw_write8(padapter, REG_LEDCFG2, (LedCfg & 0xf0) | BIT5 | BIT6); /* SW control led0 on. */
		}
		break;

	case LED_PIN_LED1:
		/* RTW_INFO("In SwLedOn,LedAddr:%X LEDPIN=%d\n",REG_LEDCFG1, pLed->LedPin); */

		LedCfg = rtw_read8(padapter, REG_LEDCFG1);
		rtw_write8(padapter, REG_LEDCFG1, LedCfg & 0x10); /* SW control led0 on. */
		break;

	default:
		break;
	}

	pLed->bLedOn = _TRUE;
}


/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 *   */
static void
SwLedOff_8188EE(
	_adapter			*padapter,
	PLED_PCIE		pLed
)
{
	u8	LedCfg;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct led_priv	*ledpriv = adapter_to_led(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	/*  */
	/* 2009/10/23 MH Issau eed to move the LED GPIO from bit  0 to bit3. */
	/* 2009/10/26 MH Issau if tyhe device is 8c DID is 0x8176, we need to enable bit6 to */
	/* enable GPIO8 for controlling LED.	 */
	/* 2010/06/16 Supprt Open-drain arrangement for controlling the LED. Added by Roger. */
	/*  */
	switch (pLed->LedPin) {

	case LED_PIN_GPIO0:
		break;

	case LED_PIN_LED0:
		if (ledpriv->LedStrategy == SW_LED_MODE10) {
			/* RTW_INFO("In SwLedOff,LedAddr:%X LEDPIN=%d\n",REG_LEDCFG0, pLed->LedPin); */
			LedCfg = rtw_read8(padapter, REG_LEDCFG0);

			LedCfg &= 0x10; /* Set to software control.							 */
			rtw_write8(padapter, REG_LEDCFG0, LedCfg | BIT3);
		} else {
			/* RTW_INFO("In SwLedOff,LedAddr:%X LEDPIN=%d\n",REG_LEDCFG2, pLed->LedPin); */
			LedCfg = rtw_read8(padapter, REG_LEDCFG2);

			LedCfg &= 0xf0; /* Set to software control.				 */
			if (pHalData->bLedOpenDrain == _TRUE) { /* Open-drain arrangement for controlling the LED */
				LedCfg &= 0x90; /* Set to software control.				 */
				rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3));
				LedCfg = rtw_read8(padapter, REG_MAC_PINMUX_CFG);
				LedCfg &= 0xFE;
				rtw_write8(padapter, REG_MAC_PINMUX_CFG, LedCfg);

			} else
				rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3 | BIT5 | BIT6));
		}
		break;

	case LED_PIN_LED1:
		/* RTW_INFO("In SwLedOff,LedAddr:%X LEDPIN=%d\n",REG_LEDCFG1, pLed->LedPin); */
		LedCfg = rtw_read8(padapter, REG_LEDCFG1);

		LedCfg &= 0x10; /* Set to software control.							 */
		rtw_write8(padapter, REG_LEDCFG1, LedCfg | BIT3);
		break;

	default:
		break;
	}

	pLed->bLedOn = _FALSE;

}

/*
 *	Description:
 *		Initialize all LED_871x objects.
 *   */
void
rtl8188ee_InitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv *pledpriv = adapter_to_led(padapter);

	pledpriv->LedControlHandler = LedControlPCIE;

	pledpriv->SwLedOn = SwLedOn_8188EE;
	pledpriv->SwLedOff = SwLedOff_8188EE;

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);
}


/*
 *	Description:
 *		DeInitialize all LED_819xUsb objects.
 *   */
void
rtl8188ee_DeInitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv	*ledpriv = adapter_to_led(padapter);

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
}
#endif /*CONFIG_RTW_SW_LED*/
