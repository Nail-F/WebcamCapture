#pragma once

class Noncopyable
{
protected:
  /*constexpr*/ Noncopyable() {} /*= default*/;
  virtual ~Noncopyable() {} /*= default*/;
private:
  Noncopyable(const Noncopyable&) /*= delete*/;
  Noncopyable& operator=(const Noncopyable&) /*= delete*/;
};
