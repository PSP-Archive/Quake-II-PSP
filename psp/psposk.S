  .set noreorder

#include "pspstub.s"

STUB_START "sceUtility",0x40010000,0x00240005 
  STUB_FUNC 0xf6269b82,sceUtilityOskInitStart 
  STUB_FUNC 0x3dfaeba9,sceUtilityOskShutdownStart 
  STUB_FUNC 0x4b85c861,sceUtilityOskUpdate 
  STUB_FUNC 0xf3f76017,sceUtilityOskGetStatus  
STUB_END 
