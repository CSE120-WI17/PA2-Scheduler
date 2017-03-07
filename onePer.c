//onePer.c
#include <stdio.h>
#include "aux.h"
#include "umix.h"

void Main() {  

  if( Fork() == 0) {
    SlowPrintf(1, "2222222");
    Exit();
  }
  
  if( Fork() == 0) {
    SlowPrintf(1, "3333333");
    Exit();
  }
  
  RequestCPUrate(99);
  
  while(1) {
    
  }
  Printf("Should not hit this");
  Exit();
}
