
#include "../../modulos/adc/adc.h"
void pid_control_step(void) ;
float getDCPID(float referencia_mA,int index);
void setPIDParams(float kp, float ki, float kd);
void setPIDTs(float ts);
void getPIDParams(int index, float* kp, float* ki, float* kd) ;
float getPIDTs(int index) ;
void PID_Init(int index, float kp, float ki, float kd, float ts);
void setPIDParams(int index, float kp, float ki, float kd);

bool resetPID(int index);