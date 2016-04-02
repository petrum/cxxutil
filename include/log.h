/*******************************************************************************
*                           Author: Petru Marginean                            *
*                          petru.marginean@gmail.com                           *
*******************************************************************************/

#ifndef __LOG_H__
#define __LOG_H__

#include <sstream>
#include <iomanip>
#include <stdlib.h>
#include <sys/time.h>
#include <cstdio>

enum TLogLevel {logERROR, logWARNING, logINFO, logDEBUG};

template <typename T>
class Log
{
public:
    Log();
    ~Log();
    std::ostringstream& operator()(TLogLevel level = logINFO);
public:
    static TLogLevel& ReportingLevel();
    static void SetReportingLevel(const std::string& s); 
protected:
    std::ostringstream os;
private:
    Log(const Log&);
    Log& operator =(const Log&);
    TLogLevel messageLevel;
    static unsigned long& ThreadID();
};

inline unsigned long CurrentThreadID()
{
    return pthread_self();
}

template <typename T>
unsigned long& Log<T>::ThreadID()
{
    static unsigned long firstThreadID = CurrentThreadID();
    return firstThreadID;
}

inline TLogLevel FromString(const std::string& level)
{
    if (level == "ERROR")
        return logERROR;
    if (level == "WARNING")
        return logWARNING;
    if (level == "INFO")
        return logINFO;
    if (level == "DEBUG")
        return logDEBUG;
    return logINFO;
}

inline std::string GetEnv(const char* pName, const char* pDefault)
{   
    ENFORCE(pName);
    char* pRes = getenv(pName);
    ENFORCE(pRes || pDefault)("The environment variable '")(pName)("' is missing; "
        "please provide a default if you do not want an exception here");
    return pRes ? pRes : pDefault;
}

template <typename T>
TLogLevel& Log<T>::ReportingLevel()
{
    static TLogLevel reportingLevel = FromString(GetEnv("LOG_LEVEL", "INFO"));
    return reportingLevel;
}

template <typename T>
void Log<T>::SetReportingLevel(const std::string& s)
{
    ReportingLevel() = FromString(s);
}

inline const char* const LogToString(TLogLevel level)
{
    static const char* const  buffer[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
    return buffer[level];
}

template <typename T>
std::ostringstream& Log<T>::operator()(TLogLevel level)
{
    messageLevel = level;
    return os;
}

template <typename T>
Log<T>::Log() : messageLevel(logINFO)
{
    os << std::setprecision(16);
}

inline std::string NowTime()
{ 
  struct timeval tv;
  gettimeofday(&tv, 0);
  char buffer[50];
  tm r = {0};
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %X", localtime_r(&tv.tv_sec, &r));
  char result[100] = {0};
  std::sprintf(result, "%s.%06ld", buffer, (long)tv.tv_usec);
  return result;
}

template <typename T>
Log<T>::~Log()
{   
    if (T::Stream())
    {
        std::ostringstream tmp;
        tmp << NowTime() << " [" << CurrentThreadID() << "]";
        tmp << " " << LogToString(messageLevel) << ": ";
        tmp << std::string(messageLevel <= logDEBUG ? 0 : messageLevel - logDEBUG, '\t');
        tmp << os.str();
        tmp << "\n";
        T::Output(tmp.str());
    }
}

#endif
