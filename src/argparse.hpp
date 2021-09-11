
#pragma once

#include <vector>
#include <string>

class ArgParser
{
  // ArgParser class based on https://stackoverflow.com/a/868894/6760298
  // Helps with parsing command line arguments

  public:
    ArgParser (int &argc, char **argv);

    const std::string& get_option(const uint position) const;
    const std::string& get_option(const std::string &name, const std::string &default_value) const;
    const std::string& get_option(const std::string &name) const;

    int get_option_as_int(const uint position) const;    
    int get_option_as_int(const std::string &name, const uint default_value) const;
    int get_option_as_int(const std::string &name) const;
    float get_option_as_float(const std::string &name, const float default_value) const;
    float get_option_as_float(const std::string &name) const;

    bool option_exists(const std::string &option) const;

  private:
    std::vector <std::string> tokens;
};
