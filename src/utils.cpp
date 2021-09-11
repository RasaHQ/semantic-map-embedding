
#include <string>
#include "utils.hpp"

StopWatch::StopWatch() : 
  start_time(0),
  end_time(0),
  running(false)
{}


void StopWatch::start()
{
  this->start_time = get_unix_time();
  this->running = true;
}


void StopWatch::stop()
{
  this->end_time = get_unix_time();
  this->running = false;
}


std::string StopWatch::duration_string() const
{
  int64_t total_seconds = this->end_time - this->start_time;
  int days = total_seconds / (60 * 60 * 24);
  int hours = total_seconds / (60 * 60) - 24 * days;
  int minutes = total_seconds / 60 - 24 * 60 * days - 60 * hours;
  int seconds = total_seconds - 24 * 60 * 60 * days - 60 * 60 * hours - 60 * minutes;
  return std::to_string(days) + "d " + 
        std::to_string(hours) + "h " + 
        std::to_string(minutes) + "m " + 
        std::to_string(seconds) + "s";
}


std::ostream& operator<<(std::ostream& os, const StopWatch& watch)
{
    os << watch.duration_string();
    return os;
}
