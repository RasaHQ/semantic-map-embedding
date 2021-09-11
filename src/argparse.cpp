
#include "argparse.hpp"
#include <algorithm>    // std::find
#include <exception>


ArgParser::ArgParser(int &argc, char **argv)
{
  for (int i=1; i < argc; ++i)
    this->tokens.push_back(std::string(argv[i]));
}


const std::string& ArgParser::get_option(const uint position) const
{
  if (this->tokens.size() <= position) 
    std::__throw_invalid_argument("Missing argument");
  return this->tokens[position];
}


const std::string& ArgParser::get_option(const std::string &name, const std::string &default_value) const
{
  if (this->get_option(name) == "") 
    return default_value;
  return this->get_option(name);
}


const std::string& ArgParser::get_option(const std::string &name) const
{
  std::vector<std::string>::const_iterator itr;
  itr =  std::find(this->tokens.begin(), this->tokens.end(), name);
  if (itr != this->tokens.end() && ++itr != this->tokens.end())
  {
    return *itr;
  }
  static const std::string empty_string("");
  return empty_string;
}


int ArgParser::get_option_as_int(const uint position) const
{
  return atoi(this->get_option(position).c_str());
}


int ArgParser::get_option_as_int(const std::string &name, const uint default_value) const
{
  return atoi(this->get_option(name, std::to_string(default_value)).c_str());
}


int ArgParser::get_option_as_int(const std::string &name) const
{
  return atoi(this->get_option(name).c_str());
}


float ArgParser::get_option_as_float(const std::string &name, const float default_value) const
{
  return atof(this->get_option(name, std::to_string(default_value)).c_str());
}


float ArgParser::get_option_as_float(const std::string &name) const
{
  return atof(this->get_option(name).c_str());
}


bool ArgParser::option_exists(const std::string &option) const
{
  return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
}
