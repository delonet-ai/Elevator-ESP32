#pragma once
#include "web_control_policy.h"

void webCalibrationPoll();
bool webCalibrationBlocksCommands();
void webCalibrationExternalStop();
uint32_t webCalibrationOwner();
uint32_t webCalibrationGeneration();
uint8_t webCalibrationResult();
