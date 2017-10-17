/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 */

#include "rtl8195a.h"

#ifdef CONFIG_GPIO_EN

HAL_GPIO_ADAPTER gHAL_Gpio_Adapter;
extern PHAL_GPIO_ADAPTER _pHAL_Gpio_Adapter;

extern VOID GPIO_PullCtrl_8195a(u32 chip_pin, u8 pull_type);

/**
  * @brief  To get the GPIO IP Pin name for the given chip pin name
  *
  * @param  chip_pin: The chip pin name.
  *
  * @retval The gotten GPIO IP pin name
  */
u32 
HAL_GPIO_GetPinName(
    u32 chip_pin
)
{
    return HAL_GPIO_GetIPPinName_8195a((u32)chip_pin);
}

/**
  * @brief  Set the GPIO pad Pull type
  *
  *  @param    pin: The pin for pull type control.
  *  @param    mode: the pull type for the pin.
  *  @return   None
  */
VOID 
HAL_GPIO_PullCtrl(
    u32 pin,
    u32 mode    
)
{
    u8 pull_type;

    switch (mode) {
        case hal_PullNone:
            pull_type = DIN_PULL_NONE;
            break;
            
        case hal_PullDown:
            pull_type = DIN_PULL_LOW;
            break;
            
        case hal_PullUp:
            pull_type = DIN_PULL_HIGH;
            break;
            
        case hal_OpenDrain:
        default:
            pull_type = DIN_PULL_NONE;
            break;
    }

//    HAL_GPIO_PullCtrl_8195a (pin, pull_type);
    GPIO_PullCtrl_8195a (pin, pull_type);
}


/**
  * @brief  Initializes a GPIO Pin by the GPIO_Pin parameters.
  *
  * @param  GPIO_Pin: The data structer which contains the parameters for the GPIO Pin initialization.
  *
  * @retval HAL_Status
  */
VOID 
HAL_GPIO_Init(
    HAL_GPIO_PIN  *GPIO_Pin
)
{
    u8 port_num;
    u8 pin_num;
    u32 chip_pin;


    if (_pHAL_Gpio_Adapter == NULL) {
        _pHAL_Gpio_Adapter = &gHAL_Gpio_Adapter;
//        DBG_GPIO_INFO("HAL_GPIO_Init: Initial GPIO Adapter\n ");
    }

    port_num = HAL_GPIO_GET_PORT_BY_NAME(GPIO_Pin->pin_name);
    pin_num = HAL_GPIO_GET_PIN_BY_NAME(GPIO_Pin->pin_name);
    chip_pin = GPIO_GetChipPinName_8195a(port_num, pin_num);
#if CONFIG_DEBUG_LOG > 3
    if (GpioFunctionChk(chip_pin, ENABLE) == _FALSE) {
//    	if((chip_pin > 0x03) && (chip_pin != 0x25)) {
    		DBG_GPIO_ERR("HAL_GPIO_Init: GPIO Pin(%x) Unavailable\n ", chip_pin);
    		return;
//    	}
//    	else DBG_GPIO_WARN("HAL_GPIO_Init: GPIO Pin(%x) Warning for RTL8710AF!\n ", chip_pin);
    }
#endif
    // Make the pin pull control default as High-Z
    GPIO_PullCtrl_8195a(chip_pin, HAL_GPIO_HIGHZ);

    //    HAL_Status ret =
#if CONFIG_DEBUG_LOG > 3
    HAL_Status ret = HAL_GPIO_Init_8195a(GPIO_Pin);
    if (ret != HAL_OK) {
        GpioFunctionChk(chip_pin, DISABLE);
    }
#else
    HAL_GPIO_Init_8195a(GPIO_Pin);
#endif
}

/**
  * @brief  Initializes a GPIO Pin as a interrupt signal
  *
  * @param  GPIO_Pin: The data structer which contains the parameters for the GPIO Pin initialization.
  *
  * @retval HAL_Status
  */
VOID 
HAL_GPIO_Irq_Init(
    HAL_GPIO_PIN  *GPIO_Pin
)
{
    if (_pHAL_Gpio_Adapter == NULL) {
        _pHAL_Gpio_Adapter = &gHAL_Gpio_Adapter;
//        DBG_GPIO_INFO("%s: Initial GPIO Adapter\n ", __FUNCTION__);
    }

    if (_pHAL_Gpio_Adapter->IrqHandle.IrqFun == NULL) {
        _pHAL_Gpio_Adapter->IrqHandle.IrqFun = (IRQ_FUN)HAL_GPIO_MbedIrqHandler_8195a;
        _pHAL_Gpio_Adapter->IrqHandle.Priority = 6;
        HAL_GPIO_RegIrq_8195a(&_pHAL_Gpio_Adapter->IrqHandle);        
        InterruptEn(&_pHAL_Gpio_Adapter->IrqHandle);
//        DBG_GPIO_INFO("%s: Initial GPIO IRQ Adapter\n ", __FUNCTION__);
    }

#if CONFIG_DEBUG_LOG > 3
    u8 port_num = HAL_GPIO_GET_PORT_BY_NAME(GPIO_Pin->pin_name);
    u8 pin_num = HAL_GPIO_GET_PIN_BY_NAME(GPIO_Pin->pin_name);
    u32 chip_pin = GPIO_GetChipPinName_8195a(port_num, pin_num);
    if (GpioFunctionChk(chip_pin, ENABLE) == _FALSE) {
        DBG_GPIO_ERR("HAL_GPIO_Irq_Init: GPIO Pin(%x) Unavailable\n ", chip_pin);
        return;
    }
#endif
    DBG_GPIO_INFO("HAL_GPIO_Irq_Init: GPIO(name=0x%x)(mode=%d)\n ", GPIO_Pin->pin_name, 
        GPIO_Pin->pin_mode);
    HAL_GPIO_MaskIrq_8195a(GPIO_Pin);
//    HAL_Status ret =

#if CONFIG_DEBUG_LOG > 3
    HAL_Status ret = HAL_GPIO_Init_8195a(GPIO_Pin);
    if (ret != HAL_OK) {
        GpioFunctionChk(chip_pin, DISABLE);
    }
#else
    HAL_GPIO_Init_8195a(GPIO_Pin);
#endif
}

/**
  * @brief  UnInitial GPIO Adapter
  *
  *
  * @retval HAL_Status
  */
VOID
HAL_GPIO_IP_DeInit(
    VOID
)
{
    if (_pHAL_Gpio_Adapter != NULL) {
        InterruptDis(&_pHAL_Gpio_Adapter->IrqHandle);
        HAL_GPIO_UnRegIrq_8195a(&_pHAL_Gpio_Adapter->IrqHandle);                
        _pHAL_Gpio_Adapter = NULL;
    }
    
}

/**
  * @brief  De-Initializes a GPIO Pin, reset it as default setting.
  *
  * @param  GPIO_Pin: The data structer which contains the parameters for the GPIO Pin.
  *
  * @retval HAL_Status
  */
VOID 
HAL_GPIO_DeInit(
    HAL_GPIO_PIN  *GPIO_Pin
)
{
#if CONFIG_DEBUG_LOG > 3
    u8 port_num = HAL_GPIO_GET_PORT_BY_NAME(GPIO_Pin->pin_name);
    u8 pin_num = HAL_GPIO_GET_PIN_BY_NAME(GPIO_Pin->pin_name);
#endif
    HAL_GPIO_DeInit_8195a(GPIO_Pin);
#if CONFIG_DEBUG_LOG > 3
    GpioFunctionChk(GPIO_GetChipPinName_8195a(port_num, pin_num), DISABLE);
#endif
}


#endif // CONFIG_GPIO_EN
