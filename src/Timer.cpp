#include <switch.h>
namespace Timer
{
  u64 s_startTicks;
  double _Time()
  {
    u64 elapsed = armGetSystemTick() - s_startTicks;
    return (elapsed * 625 / 12) / 1000000000.0;
  }

  void Start()
  {
    s_startTicks = armGetSystemTick();
  }
  float GetTime()
  {
    return (float)_Time();
  }
}