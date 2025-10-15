
#include "../../modulos/adc/adc.h"
void pid_control_step(void) ;
float getDCPID(float mA);
void setPIDParams(float kp, float ki, float ts);
void resetPID();
