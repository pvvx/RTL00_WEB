/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 */


#ifndef _HAL_VECTOR_TABLE_H_
#define _HAL_VECTOR_TABLE_H_




extern _LONG_CALL_ROM_ VOID
VectorTableInitRtl8195A(
    IN  u32 StackP
);

extern _LONG_CALL_ROM_ VOID
VectorTableInitForOSRtl8195A(
    IN  VOID *PortSVC,
    IN  VOID *PortPendSVH,
    IN  VOID *PortSysTick    
);

extern _LONG_CALL_ROM_ BOOL
VectorIrqRegisterRtl8195A(
    IN  PIRQ_HANDLE pIrqHandle
);

extern _LONG_CALL_ROM_ BOOL
VectorIrqUnRegisterRtl8195A(
    IN  PIRQ_HANDLE pIrqHandle
);


extern _LONG_CALL_ROM_ VOID
VectorIrqEnRtl8195A(
    IN  PIRQ_HANDLE pIrqHandle
);

extern _LONG_CALL_ROM_ VOID
VectorIrqDisRtl8195A(
    IN  PIRQ_HANDLE pIrqHandle
);

 
extern _LONG_CALL_ROM_ VOID
HalPeripheralIntrHandle(VOID);
#endif //_HAL_VECTOR_TABLE_H_
