#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <strmif.h>
#include <map>

class WinDevices
{
public:
  WinDevices();
  ~WinDevices();

  const std::string & DeviceName(int index) const;

  void Print();

private:
  void FindDevice();
  HRESULT EnumerateDevices(REFGUID category, IEnumMoniker **ppEnum);
  void DisplayDeviceInformation(IEnumMoniker *pEnum);

private:
  typedef std::map<int, std::string> device_list_type;

  IMoniker *pMoniker;
  device_list_type device_list_;
  int device_id_;
};