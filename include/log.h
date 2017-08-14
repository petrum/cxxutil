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
#include "enforce.h"

enum TLogLevel {logERROR, logWARNING, logINFO, logDEBUG, logDEBUG1, logDEBUG2, logDEBUG3, 
                logDEBUG4, logDEBUG5, logDEBUG6, logALL};

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
    if (level == "DEBUG1")
        return logDEBUG1;
    if (level == "DEBUG2")
        return logDEBUG2;
    if (level == "DEBUG3")
        return logDEBUG3;
    if (level == "DEBUG4")
        return logDEBUG4;
    if (level == "DEBUG5")
        return logDEBUG5;
    if (level == "DEBUG6")
        return logDEBUG6;
    if (level == "ALL")
        return logALL;
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
    static const char* const  buffer[] = {"ERROR", "WARNING", "INFO", "DEBUG", "DEBUG1", "DEBUG2", 
                                          "DEBUG3", "DEBUG4", "DEBUG5", "DEBUG6", "ALL"};
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
