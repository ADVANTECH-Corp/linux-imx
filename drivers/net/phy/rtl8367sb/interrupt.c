/*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * $Revision$
 * $Date$
 *
 * Purpose : RTK switch high-level API for RTL8367/RTL8367C
 * Feature : Here is a list of all functions and variables in Interrupt module.
 *
 */

#include "rtk_switch.h"
#include "rtk_error.h"
#include "interrupt.h"
#include <linux/string.h>

#include "dal/dal_mgmt.h"

/* Function Name:
 *      rtk_int_polarity_set
 * Description:
 *      Set interrupt polarity configuration.
 * Input:
 *      type - Interruptpolarity type.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      The API can set interrupt polarity configuration.
 */
rtk_api_ret_t rtk_int_polarity_set(rtk_int_polarity_t type)
{
    rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_polarity_set)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_polarity_set(type);    
    RTK_API_UNLOCK();

    return retVal;
}

/* Function Name:
 *      rtk_int_polarity_get
 * Description:
 *      Get interrupt polarity configuration.
 * Input:
 *      None
 * Output:
 *      pType - Interruptpolarity type.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 * Note:
 *      The API can get interrupt polarity configuration.
 */
rtk_api_ret_t rtk_int_polarity_get(rtk_int_polarity_t *pType)
{
    rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_polarity_get)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_polarity_get(pType);    
    RTK_API_UNLOCK();

    return retVal;
}

/* Function Name:
 *      rtk_int_control_set
 * Description:
 *      Set interrupt trigger status configuration.
 * Input:
 *      type - Interrupt type.
 *      enable - Interrupt status.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 *      RT_ERR_ENABLE       - Invalid enable input.
 * Note:
 *      The API can set interrupt status configuration.
 *      The interrupt trigger status is shown in the following:
 *      - INT_TYPE_LINK_STATUS
 *      - INT_TYPE_METER_EXCEED
 *      - INT_TYPE_LEARN_LIMIT
 *      - INT_TYPE_LINK_SPEED
 *      - INT_TYPE_CONGEST
 *      - INT_TYPE_GREEN_FEATURE
 *      - INT_TYPE_LOOP_DETECT
 *      - INT_TYPE_8051,
 *      - INT_TYPE_CABLE_DIAG,
 *      - INT_TYPE_ACL,
 *      - INT_TYPE_SLIENT
 */
rtk_api_ret_t rtk_int_control_set(rtk_int_type_t type, rtk_enable_t enable)
{
    rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_control_set)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_control_set(type, enable);    
    RTK_API_UNLOCK();

    return retVal;
}

/* Function Name:
 *      rtk_int_control_get
 * Description:
 *      Get interrupt trigger status configuration.
 * Input:
 *      type - Interrupt type.
 * Output:
 *      pEnable - Interrupt status.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      The API can get interrupt status configuration.
 *      The interrupt trigger status is shown in the following:
 *      - INT_TYPE_LINK_STATUS
 *      - INT_TYPE_METER_EXCEED
 *      - INT_TYPE_LEARN_LIMIT
 *      - INT_TYPE_LINK_SPEED
 *      - INT_TYPE_CONGEST
 *      - INT_TYPE_GREEN_FEATURE
 *      - INT_TYPE_LOOP_DETECT
 *      - INT_TYPE_8051,
 *      - INT_TYPE_CABLE_DIAG,
 *      - INT_TYPE_ACL,
 *      - INT_TYPE_UPS,
 *      - INT_TYPE_SLIENT
 */
rtk_api_ret_t rtk_int_control_get(rtk_int_type_t type, rtk_enable_t *pEnable)
{
    rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_control_get)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_control_get(type, pEnable);    
    RTK_API_UNLOCK();

    return retVal;
}

/* Function Name:
 *      rtk_int_status_set
 * Description:
 *      Set interrupt trigger status to clean.
 * Input:
 *      None
 * Output:
 *      pStatusMask - Interrupt status bit mask.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT - Invalid input parameters.
 * Note:
 *      The API can clean interrupt trigger status when interrupt happened.
 *      The interrupt trigger status is shown in the following:
 *      - INT_TYPE_LINK_STATUS    (value[0] (Bit0))
 *      - INT_TYPE_METER_EXCEED   (value[0] (Bit1))
 *      - INT_TYPE_LEARN_LIMIT    (value[0] (Bit2))
 *      - INT_TYPE_LINK_SPEED     (value[0] (Bit3))
 *      - INT_TYPE_CONGEST        (value[0] (Bit4))
 *      - INT_TYPE_GREEN_FEATURE  (value[0] (Bit5))
 *      - INT_TYPE_LOOP_DETECT    (value[0] (Bit6))
 *      - INT_TYPE_8051           (value[0] (Bit7))
 *      - INT_TYPE_CABLE_DIAG     (value[0] (Bit8))
 *      - INT_TYPE_ACL            (value[0] (Bit9))
 *      - INT_TYPE_SLIENT         (value[0] (Bit11))
 *      The status will be cleared after execute this API.
 */
rtk_api_ret_t rtk_int_status_set(rtk_int_status_t *pStatusMask)
{
    rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_status_set)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_status_set(pStatusMask);    
    RTK_API_UNLOCK();

    return retVal;
}

/* Function Name:
 *      rtk_int_status_get
 * Description:
 *      Get interrupt trigger status.
 * Input:
 *      None
 * Output:
 *      pStatusMask - Interrupt status bit mask.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      The API can get interrupt trigger status when interrupt happened.
 *      The interrupt trigger status is shown in the following:
 *      - INT_TYPE_LINK_STATUS    (value[0] (Bit0))
 *      - INT_TYPE_METER_EXCEED   (value[0] (Bit1))
 *      - INT_TYPE_LEARN_LIMIT    (value[0] (Bit2))
 *      - INT_TYPE_LINK_SPEED     (value[0] (Bit3))
 *      - INT_TYPE_CONGEST        (value[0] (Bit4))
 *      - INT_TYPE_GREEN_FEATURE  (value[0] (Bit5))
 *      - INT_TYPE_LOOP_DETECT    (value[0] (Bit6))
 *      - INT_TYPE_8051           (value[0] (Bit7))
 *      - INT_TYPE_CABLE_DIAG     (value[0] (Bit8))
 *      - INT_TYPE_ACL            (value[0] (Bit9))
 *      - INT_TYPE_SLIENT         (value[0] (Bit11))
 *
 */
rtk_api_ret_t rtk_int_status_get(rtk_int_status_t* pStatusMask)
{
    rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_status_get)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_status_get(pStatusMask);    
    RTK_API_UNLOCK();

    return retVal;
}

/* Function Name:
 *      rtk_int_advanceInfo_get
 * Description:
 *      Get interrupt advanced information.
 * Input:
 *      adv_type - Advanced interrupt type.
 * Output:
 *      info - Information per type.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API can get advanced information when interrupt happened.
 *      The status will be cleared after execute this API.
 */
rtk_api_ret_t rtk_int_advanceInfo_get(rtk_int_advType_t adv_type, rtk_int_info_t *pInfo)
{
     rtk_api_ret_t retVal;

    if (NULL == RT_MAPPER->int_advanceInfo_get)
        return RT_ERR_DRIVER_NOT_FOUND;
    
    RTK_API_LOCK();
    retVal = RT_MAPPER->int_advanceInfo_get(adv_type, pInfo);    
    RTK_API_UNLOCK();

    return retVal;
}

