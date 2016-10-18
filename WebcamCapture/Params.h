#pragma once

#include "Noncopyable.h"

#include <map>
#include <ostream>

class Params : Noncopyable
{
public:
  enum status
  {
    SUCCEED,
    INVALID_PARAM
  };

  enum param_id
  {
    FILE_DESTINATION,
    CAPTURE_DURATION_SEC,
    VIDEO_DEVICE_ID,
    VIDEO_DEVICE_NAME,
    AUDIO_DEVICE_ID,
    AUDIO_DEVICE_NAME,
    PARAMS_MIN = FILE_DESTINATION,
    PARAMS_MAX = AUDIO_DEVICE_NAME
  };

  static const char * params_name[PARAMS_MAX+1];
  static const char * params_key[PARAMS_MAX+1];

  Params(int argc, const char ** argv);

  void PrintInfo();
  void PrintParams();

  status GetStatus() const { return status_; }

  const std::string &GetString(param_id id);
  int GetInt(param_id id);
  int Set(int index, const std::string & value);

private:
  bool IsInternalParam(int idx);

private:
  typedef std::map<int, std::string> params_type;
  params_type params_;
  status status_;
};