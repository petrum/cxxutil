#include "scopeexit.h"
#include "enforce.h"
#include "filelog.h"
#include "alog.h"

#include <pthread.h>
#include <sched.h>

void accelerate()
{
    int policy = SCHED_RR;
    sched_param param;
    param.sched_priority = sched_get_priority_max(policy);
    ENFORCE(pthread_setschedparam(pthread_self(), policy, &param) == 0);
}

bool foo(bool b)
{
    FILE_LOG(logINFO) << "foo(" << b << ")";
    return b;
}

ALog aLog;
ALog* const ALog::pALog = &aLog;

int main(int argc, char* argv[])
{
    STD_FUNCTION_BEGIN;
    accelerate();
    SCOPE_EXIT(foo(true));
    ALog::get().init(1000000, 256, "/tmp/test.log");
    SCOPE_EXIT(ALog::get().stop());
    std::size_t NUM = 10;
    FILE_LOG(logINFO) << "Started logging " << NUM << " messages";
    for (std::size_t i = 0; i != NUM; ++i)
    {
        ALOG << "Hello" << " World " << 3.2 << " blabla " << i;
        ALOG << "Hello" << " World " << 3.2 << " blabla " << i;
        usleep(1);
    }
    FILE_LOG(logINFO) << "Finishing logging " << NUM << " messages";
    FILE_LOG(logINFO) << "Stopped";
    
    ENFORCE(foo(false))("Some issue here passing ")(false);
    return 0;
    STD_FUNCTION_END;
    return -1;
}

