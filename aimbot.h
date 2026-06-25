#pragma once

#include <windows.h>

struct Vector3A {
    float x, y, z;
};

extern float aimbotFOV;
extern float aimbotSmoothness;
extern bool aimbotEnabled;
extern bool triggerbotEnabled;
extern int triggerbotDelay;

Vector3A CalculateAngle(const Vector3A& src, const Vector3A& dst);
void SmoothAngle(Vector3A& currentAngle, const Vector3A& targetAngle, float smooth);
float GetFOVRadius();
void Triggerbot();