/*******************************************************************************
*                           Author: Petru Marginean                            *
*                          petru.marginean@gmail.com                           *
*******************************************************************************/

#ifndef __FILELOG_H__
#define __FILELOG_H__

#include "log.h"
#include "enforce.h"
#include <stdio.h>

class Output2FILE
{
public:
    static FILE*& Stream();
    static void Output(const std::string& msg);
};

inline FILE*& Output2FILE::Stream()
{
    static FILE* pStream = stderr;
    return pStream;
}

inline void Output2FILE::Output(const std::string& msg)
{   
    fprintf(Stream(), "%s", msg.c_str());
    fflush(Stream());
}

class FILELog : public Log<Output2FILE> {};
//typedef  Log<Output2FILE> FILELog;
#define FILE_LOG(messageLevel) if (Output2FILE::Stream() && messageLevel > FILELog::ReportingLevel()) ; else FILELog()(messageLevel)

#define STD_FUNCTION_BEGIN try {
#define STD_FUNCTION_END }\
    catch (const std::exception& e)\
    {\
        FILE_LOG(logERROR) << e.what();\
    }\
    catch (...)\
    {\
        FILE_LOG(logERROR) << "Unknown error";\
    }

#endif

