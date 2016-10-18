#include "params.h"
#include <iostream>
#include <string>

const char * Params::params_name[PARAMS_MAX+1] = 
{
  "file destination",
  "capture duration in seconds",
  "video device ID",
  "video device name",
  "audio device ID",
  "audio device name"
};

const char * Params::params_key[PARAMS_MAX+1] = 
{
  "-f",
  "-d",
  "-v",
  "-video_name",
  "-a",
  "-audio_name"
};

const int CONST_CAPTURE_DURATION_SEC = 5;

Params::Params(int argc, const char ** argv)
  :status_(SUCCEED)
{
  for (int i = 0; i < argc; ++i)
  {
    for (int params_it = PARAMS_MIN; params_it <= PARAMS_MAX; ++params_it)
    {
      if (strstr(argv[i], params_key[params_it]) != nullptr && strstr(argv[i]+strlen(params_key[params_it]), "=") != nullptr)
      {
        params_[params_it] = argv[i]+strlen(params_key[params_it])+1;
      }
    }
  }

  //check for required params
  params_type::const_iterator it = params_.find(FILE_DESTINATION);
  if (it == params_.end())
  {
    status_ = INVALID_PARAM;
    return;
  }
  it = params_.find(VIDEO_DEVICE_ID);
  if (it == params_.end())
  {
    status_ = INVALID_PARAM;
    return;
  }
  it = params_.find(CAPTURE_DURATION_SEC);
  if (it == params_.end())
  {
    params_[CAPTURE_DURATION_SEC] = std::to_string(CONST_CAPTURE_DURATION_SEC);
  }
}

const std::string & Params::GetString(param_id id)
{
  return params_[id];
}

int Params::GetInt(param_id id)
{
  if (params_.find(id) != params_.end())
  {
    return atoi(params_.at(id).c_str());
  }
  return -1;
}

int Params::Set(int index, const std::string & value)
{
  if (index < PARAMS_MIN || index > PARAMS_MAX)
  {
    return -1;
  }

  params_[index] = value;
  return index;
}

void Params::PrintInfo()
{
  std::cout << "==== Please define params: ====" << std::endl;
  for (int it = PARAMS_MIN; it != PARAMS_MAX; ++it)
  {
    if (!IsInternalParam(it))
    {
      std::cout <<  params_key[it] << "=<" << params_name[it] << ">" << std::endl;
    }
  }
  std::cout <<  "For example:\n"
                "WebcamCapture.exe -f=c:\\output.avi -d=10 -v=0 -a=1\n"
                "Where d=10 is 10 seconds of capturing\n"
                "v=0 is the first capture video device in list,\n"
                "a=1 is the second capture sound device.\n";
  std::cout << std::endl;
}

void Params::PrintParams()
{
  std::cout << "========= Get params: =========" << std::endl;
  for (auto it : params_)
  {
    if (!IsInternalParam(it.first))
    {
      std::cout << "[" << params_name[it.first] << "] = " << it.second.c_str() << std::endl;
    }
  }
  std::cout << std::endl;
}

bool Params::IsInternalParam(int idx)
{
  return idx == VIDEO_DEVICE_NAME || idx == AUDIO_DEVICE_NAME;
}
