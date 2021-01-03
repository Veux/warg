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
  if (stopped)
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
    float64 new_time = (float64)(end - begin) / freq;
    times[current_index] = new_time;
    shortest_sample = min(shortest_sample,new_time);
    longest_sample = max(longest_sample,new_time);
    sample_counter += 1 ;
    last_index = current_index;
    ++current_index;
    if (max_valid_index < times.size())
      ++max_valid_index;
    if (current_index > times.size() - 1)
      current_index = 0;
  }
}

void Timer::cancel()
{
  stopped = true;
}

void Timer::clear_all()
{
  stopped = true;
  max_valid_index = 0;
  current_index = 0;
  last_index = -1;  
  shortest_sample = std::numeric_limits<float64>::infinity();
  longest_sample = 0.0;
  begin = 0;
  end = 0;
  times = std::vector<float64>(times.size(), -1.);
}

void Timer::insert_time(float64 new_time)
{
  times[current_index] = new_time;
  shortest_sample = min(shortest_sample, new_time);
  longest_sample = max(longest_sample, new_time);
  sample_counter += 1;
  last_index = current_index;
  ++current_index;
  if (max_valid_index < times.size())
    ++max_valid_index;
  if (current_index > times.size() - 1)
    current_index = 0;
}

// returns the elapsed time of the current ongoing timer

float64 Timer::get_current()
{
  if (stopped)
    return 0.;
  float64 now = SDL_GetPerformanceCounter();
  return (float64)(now - begin) / freq;
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
  for (uint32 i = 0; i < max_valid_index; ++i)
  {
    sum += times[i];
    count += 1;
  }
  return sum / count;
}
float64 Timer::average()
{
  return sum / sample_counter;
}

uint32 Timer::sample_count()
{
  return sample_counter;
}

uint32 Timer::max_sample_count()
{
  return times.size();
}

float64 Timer::longest()
{
  return longest_sample;
}

float64 Timer::shortest()
{
  return shortest_sample;
}

float64 Timer::moving_longest()
{
  return *std::max_element(times.begin(),times.end());
}
float64 Timer::moving_shortest()
{
  return *std::min_element(times.begin(), times.end());
}

std::string Timer::string_report()
{
  std::string s = "";
  s += "Last          : " + std::to_string(get_last()) + "\n";
  s += "Moving average       : " + std::to_string(moving_average()) + "\n";
  s += "Moving Max           : " + std::to_string(moving_longest()) + "\n";
  s += "Moving Min           : " + std::to_string(moving_shortest()) + "\n";
  s += "Moving Jitter        : " + std::to_string(moving_jitter()) + "\n";
  s += "Moving samples:      : " + std::to_string(max_valid_index) + "\n";
  s += "Total average       : " + std::to_string(average()) + "\n";
  s += "Total Max           : " + std::to_string(longest()) + "\n";
  s += "Total Min           : " + std::to_string(shortest()) + "\n";
  s += "Total Jitter        : " + std::to_string(jitter()) + "\n";
  s += "Total samples:      : " + std::to_string(sample_counter) + "\n";
  return s;
}

std::vector<float64> Timer::get_times()
{
  std::vector<float64> result;
  result.reserve(max_valid_index);
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
  for (int32 i = 0; i < max_valid_index; ++i)
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

uint64 Timer::get_begin()
{
  return begin;
}
uint64 Timer::get_end()
{
  return end;
}
