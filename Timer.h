#pragma once
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
using namespace glm;
struct Timer
{
  // 'samples' is the number of start() -> stop() times to keep track of
  Timer(uint32 samples);

  // begin a new sample
  void start();

  // begin a new sample at begin time t rather than now
  void start(uint64 t);

  // stop and record time of current sample, does not resume the timer
  void stop();

  // stop recording and trash the sample we were recording
  void cancel();

  // resets the timer
  void clear_all();

  // manually inserts a time as if we did a start->stop sequence that resulted in this time
  void insert_time(float64 new_time);

  // returns the elapsed time of the current ongoing timer
  float64 get_current();

  // returns the time between the latest (start->stop)
  float64 get_last();

  // returns the average of all the samples taken
  float64 moving_average();
  float64 average();

  // returns the current number of samples taken so far
  uint32 sample_count();

  // returns the maximum number of samples that are saved
  uint32 max_sample_count();

  // returns the longest (start->stop) time out of all samples taken
  float64 longest();
  float64 moving_longest();

  // returns the shortest (start->stop) time out of all samples taken
  float64 shortest();
  float64 moving_shortest();

  // returns longest - shortest
  float64 jitter()
  {
    return longest() - shortest();
  }
  float64 moving_jitter()
  {
    return moving_longest() - moving_shortest();
  }

  // constructs a string providing all the above data
  std::string string_report();

  // get all of the active samples that have completed
  std::vector<float64> get_times();

  // get all of the samples recorded sorted with latest at the front
  std::vector<float64> get_ordered_times();

  // get the timestamp of the last call to start()
  uint64 get_begin();

  // get the timestamp of the last call to stop() that was called while the
  // timer was running
  uint64 get_end();

private:
  uint64 freq;
  uint64 begin;
  uint64 end;
  bool stopped = true;
  float64 shortest_sample = std::numeric_limits<float64>::infinity();
  float64 longest_sample = 0.0;
  float64 sum = 0;
  uint32 sample_counter = 0;
  uint32 max_valid_index = 0;
  uint32 last_index = -1;
  uint32 current_index = 0;
  std::vector<float64> times;
};
