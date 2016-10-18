#include "Params.h"
#include "WebcamCapture.h"
#include "WinDevices.h"
#include <iostream>

int main(int argc, const char ** argv)
{
  system("@echo off");
  system("@chcp 65001");
  std::cout << std::endl;
  setlocale(LC_ALL, "ru-RU"); 
  Params params(argc, argv);

  if (params.GetStatus() == Params::INVALID_PARAM)
  {
    params.PrintInfo();
  }

  params.PrintParams();

  WinDevices devices;
  devices.Print();

  if (params.GetStatus() == Params::INVALID_PARAM)
  {
    return -1;
  }

  params.Set(Params::VIDEO_DEVICE_NAME, devices.DeviceName(params.GetInt(Params::VIDEO_DEVICE_ID)));
  params.Set(Params::AUDIO_DEVICE_NAME, devices.DeviceName(params.GetInt(Params::AUDIO_DEVICE_ID)));

  WebcamCapture webcam(params.GetInt(Params::CAPTURE_DURATION_SEC), params.GetString(Params::FILE_DESTINATION), params.GetString(Params::VIDEO_DEVICE_NAME), params.GetString(Params::AUDIO_DEVICE_NAME));

  if (webcam.Status() == 0)
  {
    webcam.Work();
  }
   return 0;
}
