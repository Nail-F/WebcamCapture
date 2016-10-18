#include "WinDevices.h"
#include <string>
#include <iostream>
#include <uuids.h>
#include <xlocale>
#include <xlocbuf>
#include "StringAorW.h"
#include <VFWMSGS.H>

WinDevices::WinDevices()
  : pMoniker(nullptr)
  , device_id_(0)
{
  FindDevice();
}

WinDevices::~WinDevices(void)
{
  if (pMoniker)
  {
    pMoniker->Release();
  }
}

const std::string & WinDevices::DeviceName(int index) const
{
  auto it = device_list_.find(index);
  if (it != device_list_.end())
  {
    return it->second;
  }
  else
  {
    static const std::string empty;
    return empty;
  }
}

void WinDevices::Print()
{
  std::cout << "======== Device list: =========" << std::endl;
  for (auto it : device_list_)
  {
    std::cout << "id: " << it.first << " name: " << it.second.c_str() << std::endl;
  }
  std::cout << std::endl;
}

void WinDevices::FindDevice()
{
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (SUCCEEDED(hr))
  {
    IEnumMoniker *pEnum;

    hr = EnumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
    if (SUCCEEDED(hr))
    {
      DisplayDeviceInformation(pEnum);
      pEnum->Release();
    }
    hr = EnumerateDevices(CLSID_AudioInputDeviceCategory, &pEnum);
    if (SUCCEEDED(hr))
    {
      DisplayDeviceInformation(pEnum);
      pEnum->Release();
    }
    CoUninitialize();
  }
}

void WinDevices::DisplayDeviceInformation(IEnumMoniker *pEnum)
{
  IMoniker *pMoniker = NULL;

  while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
  {
    IPropertyBag *pPropBag;
    HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
    if (FAILED(hr))
    {
      pMoniker->Release();
      continue;
    } 

    VARIANT var;
    VariantInit(&var);

    // Get description or friendly name.
    hr = pPropBag->Read(L"Description", &var, 0);
    if (FAILED(hr))
    {
      hr = pPropBag->Read(L"FriendlyName", &var, 0);
    }
    if (SUCCEEDED(hr))
    {
      StringAorW name(var.bstrVal);
      device_list_[device_id_] = name.StrA();
      //printf("FriendlyName: \"%S\"\n", var.bstrVal);
      VariantClear(&var); 
    }

    hr = pPropBag->Write(L"FriendlyName", &var);

    // WaveInID applies only to audio capture devices.
    hr = pPropBag->Read(L"WaveInID", &var, 0);
    if (SUCCEEDED(hr))
    {
      //printf("WaveIn ID: %d\n", var.lVal);
      VariantClear(&var); 
    }

    hr = pPropBag->Read(L"DevicePath", &var, 0);
    if (SUCCEEDED(hr))
    {
      // The device path is not intended for display.
      //printf("Device ID: %d, device path: \"%S\"\n",device_id_, var.bstrVal);
      VariantClear(&var); 
    }

    if (pPropBag)
    {
      pPropBag->Release();
    }

    pPropBag->Release();

    ++device_id_;
  }
}

HRESULT WinDevices::EnumerateDevices(REFGUID category, IEnumMoniker **ppEnum)
{
  // Create the System Device Enumerator.
  ICreateDevEnum *pDevEnum;
  HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,  
    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

  if (SUCCEEDED(hr))
  {
    // Create an enumerator for the category.
    hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
    if (hr == S_FALSE)
    {
      hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
    }
    pDevEnum->Release();
  }
  return hr;
}
