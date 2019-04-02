#ifndef __POLLEDTIMING_H__
#define __POLLEDTIMING_H__


/*
 PolledTimeout.h - Encapsulation of a polled Timeout
 
 Copyright (c) 2018 Daniel Salazar. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <limits>

#include <Arduino.h>

namespace esp8266
{


namespace polledTimeout
{

namespace YieldPolicy
{

struct DoNothing
{
  static void execute() {}
};

struct YieldOrSkip
{
  static void execute() {delay(0);}
};

template <unsigned long delayMs>
struct YieldAndDelayMs
{
  static void execute() {delay(delayMs);}
};

} //YieldPolicy

namespace TimePolicy
{

struct TimeSourceMillis
{
  // time policy in milli-seconds based on millis()

  using timeType = decltype(millis());
  static timeType time() {return millis();}
  static constexpr timeType ticksPerSecond    = 1000;
  static constexpr timeType ticksPerSecondMax = 1000;
};

struct TimeSourceCycles
{
  // time policy based on ESP.getCycleCount()
  // this particular time measurement is intended to be called very often
  // (every loop, every yield)

  using timeType = decltype(ESP.getCycleCount());
  static timeType time() {return ESP.getCycleCount();}
  static constexpr timeType ticksPerSecond    = F_CPU;
  static constexpr timeType ticksPerSecondMax = 160000000; // Mhz
};

template <typename TimeSourceType, unsigned long long second_th, unsigned long long rangeCompensate = 1>
struct TimeUnit
{
  using timeType = typename TimeSourceType::timeType;

  static constexpr timeType user2UnitMultiplierMax = (TimeSourceType::ticksPerSecondMax * rangeCompensate) / second_th;
  static constexpr timeType user2UnitMultiplier    = (TimeSourceType::ticksPerSecond    * rangeCompensate) / second_th;
  static constexpr timeType user2UnitDivider       = rangeCompensate;
  static constexpr timeType timeMax                = (std::numeric_limits<timeType>::max() - 1) / user2UnitMultiplierMax;
  static constexpr timeType neverExpires           = std::numeric_limits<timeType>::max();

  static timeType toTimeTypeUnit (const timeType userUnit) { return (userUnit * user2UnitMultiplier) / user2UnitDivider; }
  static timeType toUserUnit (const timeType internalUnit) { return (internalUnit * user2UnitDivider) / user2UnitMultiplier; }
  static timeType time () {return TimeSourceType::time(); }
};

using TimeMillis     = TimeUnit< TimeSourceMillis,       1000 >;
using TimeFastMillis = TimeUnit< TimeSourceCycles,       1000 >;
using TimeFastMicros = TimeUnit< TimeSourceCycles,    1000000 >;
using TimeFastNanos  = TimeUnit< TimeSourceCycles, 1000000000, 25>;

} //TimePolicy

template <bool PeriodicT, typename YieldPolicyT = YieldPolicy::DoNothing, typename TimePolicyT = TimePolicy::TimeMillis>
class timeoutTemplate
{
public:
  using timeType = typename TimePolicyT::timeType;

  static constexpr timeType neverExpires = TimePolicyT::neverExpires;

  timeoutTemplate(const timeType userTimeout)
  {
    reset(userTimeout);
  }

  bool expired()
  {
    YieldPolicyT::execute(); //in case of DoNothing: gets optimized away
    if(PeriodicT)           //in case of false: gets optimized away
      return expiredRetrigger();
    return expiredOneShot();
  }
  
  operator bool()
  {
    return expired(); 
  }
  
  bool canExpire ()
  {
    return !_neverExpires;
  }

  void reset(const timeType newUserTimeout)
  {
    reset();
    _timeout = TimePolicyT::toTimeTypeUnit(newUserTimeout);
    _neverExpires = (newUserTimeout < 0) || (newUserTimeout > timeMax());
  }

  void reset()
  {
    _start = TimePolicyT::time();
  }

  void resetToNeverExpires ()
  {
    _timeout = 1; // because _timeout==0 has precedence
    _neverExpires = true;
  }

  timeType getTimeout() const
  {
    return TimePolicyT::toUserUnit(_timeout);
  }
  
  static constexpr timeType timeMax()
  {
    return TimePolicyT::timeMax;
  }

private:

  ICACHE_RAM_ATTR
  bool checkExpired(const timeType internalUnit) const
  {
    // (_timeout == 0) is not checked here

    // returns "can expire" and "time expired"
    return (!_neverExpires) && ((internalUnit - _start) >= _timeout);
  }

protected:

  ICACHE_RAM_ATTR
  bool expiredRetrigger()
  {
    if (_timeout == 0)
      // "always expired"
      return true;

    timeType current = TimePolicyT::time();
    if(checkExpired(current))
    {
      unsigned long n = (current - _start) / _timeout; //how many _timeouts periods have elapsed, will usually be 1 (current - _start >= _timeout)
      _start += n  * _timeout;
      return true;
    }
    return false;
  }
  
  ICACHE_RAM_ATTR
  bool expiredOneShot() const
  {
    // returns "always expired" or "has expired"
    return (_timeout == 0) || checkExpired(TimePolicyT::time());
  }
  
  timeType _timeout;
  timeType _start;
  bool _neverExpires;
};

// legacy type names, deprecated (unit is milliseconds)

using oneShot = polledTimeout::timeoutTemplate<false> /*__attribute__((deprecated("use oneShotMs")))*/;
using periodic = polledTimeout::timeoutTemplate<true> /*__attribute__((deprecated("use periodicMs")))*/;

// standard versions (based on millis())
// timeMax() is 49.7 days ((2^32)-2 ms)

using oneShotMs = polledTimeout::timeoutTemplate<false>;
using periodicMs = polledTimeout::timeoutTemplate<true>;

// "Fast" versions sacrifices time range for improved precision and reduced execution time (by 86%)
// (cpu cycles for ::expired(): 372 (millis()) vs 52 (getCycleCount))
// timeMax() values:
// Ms: max is 26843       ms (26.8  s)
// Us: max is 26843545    us (26.8  s)
// Ns: max is  1073741823 ns ( 1.07 s)

using oneShotFastMs = polledTimeout::timeoutTemplate<false, YieldPolicy::DoNothing, TimePolicy::TimeFastMillis>;
using periodicFastMs = polledTimeout::timeoutTemplate<true, YieldPolicy::DoNothing, TimePolicy::TimeFastMillis>;
using oneShotFastUs = polledTimeout::timeoutTemplate<false, YieldPolicy::DoNothing, TimePolicy::TimeFastMicros>;
using periodicFastUs = polledTimeout::timeoutTemplate<true, YieldPolicy::DoNothing, TimePolicy::TimeFastMicros>;
using oneShotFastNs = polledTimeout::timeoutTemplate<false, YieldPolicy::DoNothing, TimePolicy::TimeFastNanos>;
using periodicFastNs = polledTimeout::timeoutTemplate<true, YieldPolicy::DoNothing, TimePolicy::TimeFastNanos>;

} //polledTimeout


/* A 1-shot timeout that auto-yields when in CONT can be built as follows:
 * using oneShotYield = esp8266::polledTimeout::timeoutTemplate<false, esp8266::polledTimeout::YieldPolicy::YieldOrSkip>;
 *
 * Other policies can be implemented by the user, e.g.: simple yield that panics in SYS, and the polledTimeout types built as needed as shown above, without modifying this file.
 */

}//esp8266

#endif
