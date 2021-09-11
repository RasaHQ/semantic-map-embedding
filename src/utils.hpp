
#pragma once

#include <chrono>
#include <cpuid.h>
#include <cstring>
#include <algorithm>
#include "data.hpp"


class StopWatch
{
public:
  StopWatch();

	void start();
	void stop();

	inline int64_t get_start_unix_time() {
		return this->start_time;
	}

	std::string duration_string() const;

	friend std::ostream& operator<<(std::ostream& os, const StopWatch& watch);

protected:
	int64_t start_time;
	int64_t end_time;
	bool running;
};


inline int64_t get_unix_time()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


template<class T> 
inline T squared(T x)
{
	return x * x;
}


inline static Float vec_squared(const Float* const vec, size_t size)
{
	Float result = 0.;
	for (size_t i = 0; i < size; ++i)
	{
		result += squared(vec[i]);
	}
	return result;
}


inline std::string get_cpu_name()
{
	// Linux CPU info based on https://stackoverflow.com/a/50021699
	char cpu_name[0x40];
	unsigned int cpu_info[4] = {0,0,0,0};

	__cpuid(0x80000000, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
	unsigned int nExIds = cpu_info[0];

	std::memset(cpu_name, 0, sizeof(cpu_name));

	for (unsigned int i = 0x80000000; i <= nExIds; ++i)
	{
			__cpuid(i, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);

			if (i == 0x80000002)
					std::memcpy(cpu_name, cpu_info, sizeof(cpu_info));
			else if (i == 0x80000003)
					std::memcpy(cpu_name + 16, cpu_info, sizeof(cpu_info));
			else if (i == 0x80000004)
					std::memcpy(cpu_name + 32, cpu_info, sizeof(cpu_info));
	}

	std::string result = static_cast<std::string>(cpu_name);

	// Trim whitespace on both ends
	result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](int ch) { return !std::isspace(ch); }));
	result.erase(std::find_if(result.rbegin(), result.rend(), [](int ch) { return !std::isspace(ch); }).base(), result.end());

	return result;
}
