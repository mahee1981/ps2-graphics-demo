#ifndef DELTA_WATCH_HPP
#define DELTA_WATCH_HPP

#include <chrono>
class Deltawatch
{
  public:
    inline void CaptureStartMoment()
    {
        beforePoint = std::chrono::steady_clock::now();
    }
    inline void CaptureEndMoment()
    {
        afterPoint = std::chrono::steady_clock::now();
        deltaTime = afterPoint - beforePoint;
    }
    inline float GetDeltaMs()
    {
        return deltaTime.count();
    }
    Deltawatch()
    {
        CaptureStartMoment();
        CaptureEndMoment();
    }
  private:
    std::chrono::steady_clock::time_point beforePoint;
    std::chrono::steady_clock::time_point afterPoint;
    std::chrono::duration<float, std::milli> deltaTime;
};

#endif
