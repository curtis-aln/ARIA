#pragma once


struct Range
{
    float min, max;
    float clamp(float v) const { return std::clamp(v, min, max); }
};
