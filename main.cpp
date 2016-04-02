#include "scopeexit.h"
#include "enforce.h"
#include "filelog.h"
#include "alog.h"

bool foo(bool b)
{
    FILE_LOG(logINFO) << "foo(" << b << ")";
    return b;
}

int main(int argc, char* argv[])
{
    STD_FUNCTION_BEGIN;
    SCOPE_EXIT(foo(true));
    ALog::get().init(1000000, 256, "/tmp/test.log");
    std::size_t NUM = 10000000;
    FILE_LOG(logINFO) << "Started logging " << NUM << " messages";
    for (std::size_t i = 0; i != NUM; ++i)
    {
      ALog::get().write("Hellow world!");
    }
    FILE_LOG(logINFO) << "Finishing logging " << NUM << " messages";
    ALogMsg() << "Hello" << "World" << 123 << 3.2;
    ALog::get().stop();
    FILE_LOG(logINFO) << "Stopped";
    
    ENFORCE(foo(false))("Some issue here passing ")(false);
    return 0;
    STD_FUNCTION_END;
    return -1;
}

