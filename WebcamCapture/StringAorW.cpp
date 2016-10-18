#include "StringAorW.h"

#include <codecvt>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

StringAorW::StringAorW(const char *astr, bool duplicate) : 
  is_owner_(duplicate),
  string_a_(duplicate? _strdup(astr): astr),
  string_buffer_w_(nullptr),
  is_unicode_(false)
{
}

StringAorW::StringAorW(const std::string &astr, bool duplicate) : 
  is_owner_(duplicate),
  string_a_(duplicate? _strdup(astr.data()): astr.data()),
  string_buffer_w_(nullptr),
  is_unicode_(false)
{
}

StringAorW::StringAorW(const wchar_t *wstr, bool duplicate) : 
  is_owner_(duplicate),
  string_w_(duplicate? _wcsdup(wstr): wstr),
  string_buffer_a_(nullptr),
  is_unicode_(true)
{
}

StringAorW::StringAorW(const std::wstring &wstr, bool duplicate) : 
  is_owner_(duplicate),
  string_w_(duplicate? _wcsdup(wstr.data()): wstr.data()),
  string_buffer_a_(nullptr),
  is_unicode_(true)
{
}

StringAorW::~StringAorW()
{
  if (!is_unicode_)
  {
    if (is_owner_)
    {
      free(const_cast<char*>(string_a_));
    }
    if (string_buffer_w_)
    {
      delete[] string_buffer_w_;
    }
  }
  else
  {
    if (is_owner_)
    {
      free(const_cast<wchar_t*>(string_w_));
    }
    if (string_buffer_a_) 
    {
      delete[] string_buffer_a_;
    }
  }
}

const wchar_t *StringAorW::StrW(size_t size) const
{
  if (is_unicode_) 
  {
    return string_w_ ? string_w_ : L"";
  }

  if (!string_buffer_w_)
  {
    if (!string_a_)
    {
      return L"";
    }
    size_t len = 0;
    if(size)
    {
      len = size + 1;
    }
    else
    {
      len = strlen(string_a_) + 1;
    }
    string_buffer_w_ = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, string_a_, static_cast<int>(len), string_buffer_w_, static_cast<int>(len));
  }
  return string_buffer_w_;
}

std::string StringAorW::ToUTF8()const
{
  std::string result;
  std::wstring_convert< std::codecvt_utf8_utf16<wchar_t>> convert;
  result = convert.to_bytes(StrW()); 
  return result;
}

const char *StringAorW::StrA(size_t size) const
{
  if (!is_unicode_)
  {
    return string_a_ ? string_a_ : "";
  }

  if (!string_buffer_a_)
  {
    if (!string_w_)
    {
      return "";
    }

    size_t len = 0;
    if(size)
    {
      len = size + 1;
    }
    else
    {
      len = wcslen(string_w_) + 1;	// assuming no special characters
    }
    string_buffer_a_ = new char[len];
    WideCharToMultiByte(CP_ACP, 0, string_w_, static_cast<int>(len), string_buffer_a_, static_cast<int>(len), NULL, NULL);
  }
  return string_buffer_a_;
}

bool StringAorW::IsNull() const
{
  return is_unicode_ ? string_w_ == NULL : string_a_ == NULL;
}

StringAorW::operator bool() const
{
  return !IsNull();
}

bool StringAorW::IsNullOrEmpty() const
{
  return IsNull() || (is_unicode_ ? (*string_w_ == L'\0') : (*string_a_ == '\0'));
}

void *StringAorW::Data() const
{
  return is_unicode_ ? (void *)string_w_ : (void *)string_a_;
}

bool StringAorW::Compare(const char *astr) const
{
 if (IS_INTRESOURCE(Data()) || IS_INTRESOURCE(astr))
 {
   return Data() == astr;
 }
 return 0 == strcmp(astr, StrA());
}

bool StringAorW::Compare(const wchar_t *wstr) const
{
 if (IS_INTRESOURCE(Data()) || IS_INTRESOURCE(wstr))
 {
   return Data() == wstr;
 }
 return 0 == wcscmp(wstr, StrW());
}

bool StringAorW::Compare(const StringAorW &awstr) const
{
 if (is_unicode_)
 {
   return awstr.Compare(string_w_);
 }
 return awstr.Compare(string_a_);
}
