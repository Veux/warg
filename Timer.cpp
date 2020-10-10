#include "stdafx.h"
#include "Timer.h"
template <typename T> void ASSERT(T t)
{
#ifndef DISABLE_ASSERT
  if (!t)
  {
    throw;
  }
#endif
}

Timer::Timer(uint32 samples)
{
  times = std::vector<float64>(samples, -1.);
  freq = SDL_GetPerformanceFrequency();
}

void Timer::start()
{
  if(stopped)
  begin = SDL_GetPerformanceCounter();
  stopped = false;
}

void Timer::start(uint64 t)
{
  ASSERT(t > 0);
  stopped = false;
  begin = t;
}

void Timer::stop()
{
  if (!stopped)
  {
    end = SDL_GetPerformanceCounter();
    stopped = true;
    ASSERT(end >= begin);
    times[current_index] = (float64)(end - begin) / freq;
    last_index = current_index;
    ++current_index;
    if (num_samples < times.size())
      ++num_samples;
    if (current_index > times.size() - 1)
      current_index = 0;
  }
}

void Timer::cancel() { stopped = true; }

void Timer::clear_all()
{
  stopped = true;
  num_samples = 0;
  current_index = 0;
  last_index = -1;
  begin = 0;
  end = 0;
  times = std::vector<float64>(times.size(), -1.);
}

float64 Timer::get_last()
{
  if (last_index != -1)
    return times[last_index];
  else
    return -1;
}

float64 Timer::moving_average()
{
  float64 sum = 0;
  uint32 count = 0;
  for (uint32 i = 0; i < num_samples; ++i)
  {
    sum += times[i];
    count += 1;
  }
  return sum / count;
}

uint32 Timer::sample_count() { return num_samples; }

uint32 Timer::max_sample_count() { return times.size(); }

float64 Timer::longest()
{
  float64 longest = 0.0;
  for (uint32 i = 0; i < num_samples; ++i)
  {
    if (times[i] > longest)
      longest = times[i];
  }
  return longest;
}

inline float64 Timer::shortest()
{
  float64 shortest = times[0];
  for (uint32 i = 0; i < num_samples; ++i)
  {
    if (times[i] < shortest)
      shortest = times[i];
  }
  return shortest;
}

std::string Timer::string_report()
{
  std::string s = "";
  s += "Last          : " + std::to_string(get_last()) + "\n";
  s += "Average       : " + std::to_string(moving_average()) + "\n";
  s += "Max           : " + std::to_string(longest()) + "\n";
  s += "Min           : " + std::to_string(shortest()) + "\n";
  s += "Jitter        : " + std::to_string(jitter()) + "\n";
  s += "Samples:      : " + std::to_string(sample_count()) + "\n";
  return s;
}

std::vector<float64> Timer::get_times()
{
  std::vector<float64> result;
  result.reserve(num_samples);
  for (auto &t : times)
  {
    if (t != -1)
      result.push_back(t);
  }
  return result;
}

// get all of the samples recorded sorted with latest at the front

std::vector<float64> Timer::get_ordered_times()
{
  std::vector<float64> result;
  for (int32 i = 0; i < num_samples; ++i)
  {
    int32 index = (int32)current_index - i;

    if (index < 0)
    {
      index = times.size() - abs(index);
    }

    result.push_back(times[index]);
  }
  return result;
}

uint64 Timer::get_begin() { return begin; }
uint64 Timer::get_end() { return end; }
