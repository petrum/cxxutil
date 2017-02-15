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
    FILE_LOG(logINFO) << "Max priority is " << param.sched_priority;
    ENFORCE(pthread_setschedparam(pthread_self(), policy, &param) == 0);
}

bool foo(bool b)
{
    FILE_LOG(logINFO) << "foo(" << b << ")";
    return b;
}

void setaffinity(int i)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    ENFORCE(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
}

ALog aLog;
ALog* const ALog::pALog = &aLog;

int main(int argc, char* argv[])
{
    STD_FUNCTION_BEGIN;
    SCOPE_EXIT(foo(true));
    ALog::get().init(1000000, 256, "/tmp/test.log");
    setaffinity(11);
    accelerate();
    SCOPE_EXIT(ALog::get().stop());
    std::size_t NUM = 10;
    usleep(1 * 1000000);
    FILE_LOG(logINFO) << "Started logging " << NUM << " messages";
    ALOG << "Hello-0" << " World0 " << 3.2 << " blabla0 ";
    ALOG << "Hello-1" << " World1 " << 3.2 << " blabla1 ";
    ALOG << "Hello-2" << " World2 " << 3.2 << " blabla2 ";
    ALOG << "Hello-3" << " World3 " << 3.2 << " blabla3 ";
    for (std::size_t i = 0; i != NUM; ++i)
    {
        ALOG << "Hello1" << " World " << 3.2 << " blabla " << i;
        ALOG << "Hello2" << " World " << 3.2 << " blabla " << i;
        usleep(1000000);
    }
    FILE_LOG(logINFO) << "Finishing logging " << NUM << " messages";
    ENFORCE(foo(false))("Some issue here passing ")(false);
    return 0;
    STD_FUNCTION_END;
    return -1;
}

