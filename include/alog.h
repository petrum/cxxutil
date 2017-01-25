/*******************************************************************************
 *                           Author: Petru Marginean                           *
 *                          petru.marginean@gmail.com                          *
 ******************************************************************************/

/*
Potential issues:
  1. remove static (Meyers singleton etc)
  2. potential false sharing (head/tail etc)
  3. the consume keeps the pData warm in caches
*/

#ifndef __ALOG_H__
#define __ALOG_H__

#include <unistd.h>
#include <atomic>
#include <thread>
#include <sys/time.h>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include "filelog.h"
#include <time.h>

struct CircularQueue
{
    CircularQueue(std::size_t max_row, std::size_t max_col);
    ~CircularQueue();
    std::size_t next(std::size_t) const;
public: // producer
    char* getNextWriteBuffer();
    void writeComplete();
public: //consumer
    char* getNextReadBuffer();
    void readComplete();
    bool empty() const;
private:
    std::atomic<std::size_t> max_row_, max_col_, head_, tail_, len_;
    std::size_t tail2_;
    char *pData;
    friend std::ostream& operator <<(std::ostream& o, const CircularQueue& q);
};

inline std::ostream& operator <<(std::ostream& o, const CircularQueue& q)
{
    return o << "max_row = " << q.max_row_ << ", max_col = " << q.max_col_ << 
        ", head = " << q.head_ << ", tail = " << q.tail_ << ", tail2 = " << q.tail2_ << ", len = " << q.len_;
}

inline std::size_t CircularQueue::next(std::size_t i) const
{
    std::size_t n = i + max_col_;
    std::size_t res = n >= len_ ? 0 : n;
    FILE_LOG(logDEBUG) << "CircularQueue::next(" << i << ") = " << res;
    return res;
}

inline bool CircularQueue::empty() const
{
    bool isEmpty = next(head_) == tail_;
    FILE_LOG(logDEBUG) << "CircularQueue::empty() = " << isEmpty;
    return isEmpty;
}

inline CircularQueue::CircularQueue(std::size_t max_row, std::size_t max_col) : 
                     max_row_(max_row), max_col_(max_col), head_(0), tail_(max_col), len_(max_row * max_col), tail2_(next(tail_))
{
    pData = (char*)malloc(len_);
    FILE_LOG(logINFO) << "CircularQueue::CircularQueue(" << *this << ")";
}

inline CircularQueue::~CircularQueue()
{
    FILE_LOG(logINFO) << "CircularQueue::~CircularQueue()";
    free(pData);
    pData = 0;
}

inline char* CircularQueue::getNextWriteBuffer()
{    
    if (tail2_ == head_)
        //if (next(tail_) == head_)
    {
        FILE_LOG(logDEBUG) << "Stop writing, CircularQueue::getNextWriteBuffer1(), tail = " << tail_ << ", head = " << head_;
        return 0; // overwriting...
    }
    FILE_LOG(logDEBUG) << "CircularQueue::getNextWriteBuffer2() = " << (void*) &pData[tail_] << ", tail = " << tail_ << 
        ", head = " << head_;
    return &pData[tail_];
}

inline void CircularQueue::writeComplete()
{
    FILE_LOG(logDEBUG) << "CircularQueue::writeComplete()";
    tail_ = tail2_;
    tail2_ = next(tail_);
    //tail_ = next(tail_);
}

inline char* CircularQueue::getNextReadBuffer()
{
    std::size_t n = next(head_);
    if (n == tail_)
    {
        FILE_LOG(logDEBUG) << "Stop reading, CircularQueue::getNextReadBuffer1(), tail = " << tail_ << 
            ", head = " << head_ << ", next = " << n;
        return 0;
    }
    head_ = n;
    FILE_LOG(logDEBUG) << "CircularQueue::getNextReadBuffer2() = " << (void*) &pData[head_] << 
        ", tail = " << tail_ << ", head = " << head_;
    return &pData[head_];
}

struct ALog
{
    void init(std::size_t max_row, std::size_t max_col);
    ALog();
    ~ALog();
public:
    void write(const char*);
    static ALog& get();
    static ALog* pALog;
    void stop();
private:
    ALog(const ALog&);
    void consume();
private:
    CircularQueue* pQueue_;
    std::thread consumer_;
    std::atomic<bool> stopping_;
    std::size_t written, lost, read;
    friend struct ALogMsg;
};

inline char* doWrite(long int i, char* pData)
{
    pData[0] = 'a';
    pData = pData + 1;
    memcpy(pData, &i, sizeof(long int));
    return pData + sizeof(long int);
}

inline char* doWrite(const char* pText, char* pData)
{
    pData[0] = 'b';
    char c = strlen(pText);
    pData[1] = c;
    pData = pData + 2;
    memcpy(pData, pText, c);
    return pData + c;
}

inline ALog::ALog() : pQueue_(0), stopping_(false), written(0), lost(0), read(0)
{
    FILE_LOG(logINFO) << "ALog::ALog()";
}

inline void ALog::init(std::size_t max_row, std::size_t max_col)
{
    FILE_LOG(logINFO) << "ALog::init(" << max_row << ", " << max_col << ")";
    pQueue_ = new CircularQueue(max_row, max_col);
    consumer_ = std::thread(&ALog::consume, this);
}

inline ALog::~ALog()
{
    FILE_LOG(logINFO) << "Log::~ALog() enter";
    stop();
    delete pQueue_;
    FILE_LOG(logINFO) << "Log::~ALog() exit: written = " << written << ", lost = " << lost << 
        ", read = " << read << ", logged = " << written + lost;
}

inline void ALog::write(const char* pText)
{
    FILE_LOG(logDEBUG) << "ALog::write('" << pText << "')";
    char* pBuffer = pQueue_->getNextWriteBuffer();
    if (!pBuffer)
    {
        ++lost;
        return;
    }

    timeval ts;
    gettimeofday(&ts, 0);
    pBuffer = doWrite(ts.tv_sec, pBuffer);
    pBuffer = doWrite(ts.tv_usec, pBuffer);
    pBuffer = doWrite(pText, pBuffer);

    pQueue_->writeComplete();
    ++written;
}

inline void ALog::consume()
{
    STD_FUNCTION_BEGIN;
    FILE_LOG(logINFO) << "ALog::consume() started";
    for(std::size_t i = 0; !(stopping_ && pQueue_->empty()); ++i)
    {
        char* pData = pQueue_->getNextReadBuffer();
        if (pData)
        {
            std::ostringstream o;
            timespec& dt = *reinterpret_cast<timespec*>(pData);
            tm *pNow = localtime(&dt.tv_sec);
            char buffer[100] = {0};
            ENFORCE(strftime(buffer, sizeof(buffer), "%F %T", pNow));
            o << buffer;
            ENFORCE(snprintf(buffer, sizeof(buffer), ".%09ld: ", dt.tv_nsec));
            o << buffer;
            pData += sizeof(timespec);
            for (char ch; ch = pData++[0], ch != 'z';)
            {
                switch (ch)
                {
                case 'i':
                    o << *reinterpret_cast<int*>(pData);
                    pData += sizeof(int);
                    break;
                case 'u':
                    o << *reinterpret_cast<unsigned int*>(pData);
                    pData += sizeof(unsigned int);
                    break;
                case 'd':
                    o<< *reinterpret_cast<double*>(pData);
                    pData += sizeof(double);
                    break;
                case 'l':
                    o << *reinterpret_cast<long unsigned int*>(pData);
                    pData += sizeof(long unsigned int);
                    break;
                case 's':
                    o << pData;
                    pData += strlen(pData) + 1;
                    break;                
                default:
                    ENFORCE(false)("Found unexpected type '")(ch)("'");
                 }
            }
            FILE_LOG(logINFO) << o.str();
            ++read;
        }
        if (i % 1000 == 0)
            usleep(1);
    }
    STD_FUNCTION_END;
    FILE_LOG(logINFO) << "ALog::consume() exited";
}

inline void ALog::stop()
{
    FILE_LOG(logINFO) << "ALog::stop() enter";
    stopping_ = true;
    if (consumer_.joinable())
        consumer_.join();
    FILE_LOG(logINFO) << "ALog::stop() exit";
}

inline ALog& ALog::get()
{
    return *pALog;
}

template <typename T, std::size_t N>
    constexpr std::size_t countof(T const (&)[N]) noexcept
{
    return N;
}

struct ALogMsg
{
    ALogMsg(); 
    ~ALogMsg();
    ALogMsg& operator <<(int);
    ALogMsg& operator <<(unsigned int);
    ALogMsg& operator <<(long unsigned int i);
    ALogMsg& operator <<(double);

    template <typename T>                                                                                                                   inline ALogMsg& operator <<(const T& t)
    {
        constexpr std::size_t N = countof(t);
        if (!pData) return *this;
        pData++[0] = 's';
        memcpy(pData, t, N);
        pData += N;
        return *this;
    }
    
    template <typename T>
    inline ALogMsg& write(const T& t, char ch)
    {
        if (!pData) return *this;
        pData++[0] = ch;
        *reinterpret_cast<T*>(pData) = t;
        pData += sizeof(T);
        return *this;        
    }
private:
    char* pData;
};

/*
inline unsigned long long cpuid_rdtsc() {
    unsigned int lo, hi;
    asm volatile (
     "cpuid \n"
     "rdtsc"
     : "=a"(lo), "=d"(hi)
     : "a"(0)
     : "%ebx", "%ecx");
    return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

inline unsigned long long rdtsc() {
    unsigned int lo, hi;
    asm volatile (
     "rdtsc"
     : "=a"(lo), "=d"(hi)
     : "a"(0)
     : "%ebx", "%ecx");
    return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

inline unsigned long long rdtscp() {
    unsigned int lo, hi;
    asm volatile (
     "rdtscp"
     : "=a"(lo), "=d"(hi)
     : "a"(0)
     : "%ebx", "%ecx");
    return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}
*/

inline ALogMsg::ALogMsg()
{
    pData = ALog::get().pQueue_->getNextWriteBuffer();
    if (!pData)
    {
        ++ALog::get().lost;
        return;
    }
    //*reinterpret_cast<unsigned long long*>(pData) = rdtscp();
    timespec dt;
    ENFORCE(clock_gettime(CLOCK_REALTIME, &dt) != -1);
    *reinterpret_cast<timespec*>(pData) = dt;
    pData += sizeof(dt);
}
  
inline ALogMsg::~ALogMsg()
{
    if (!pData) return;
    pData[0] = 'z';
    ALog::get().pQueue_->writeComplete();
    ++ALog::get().written;
}

inline ALogMsg& ALogMsg::operator <<(int i)
{
    return write(i, 'i');
}

inline ALogMsg& ALogMsg::operator <<(unsigned int u)
{
    return write(u, 'u');
}

inline ALogMsg& ALogMsg::operator <<(long unsigned int l)
{
    return write(l, 'l');
}

inline ALogMsg& ALogMsg::operator <<(double d)
{
    return write(d, 'd');
}

#define ALOG ALogMsg()

#endif //__ALOG_H__
