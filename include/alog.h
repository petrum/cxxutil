/*******************************************************************************
 *                           Author: Petru Marginean                           *
 *                          petru.marginean@gmail.com                          *
 ******************************************************************************/

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
    void init(std::size_t max_row, std::size_t max_col, const std::string& fName);
    ~ALog();
public:
    void write(const char*);
    static ALog& get();
    void stop();
private:
    ALog();
    ALog(const ALog&);
    void consume();
private:
    CircularQueue* pQueue_;
    std::thread consumer_;
    std::string fName_;
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

inline void ALog::init(std::size_t max_row, std::size_t max_col, const std::string& fName)
{
    FILE_LOG(logINFO) << "ALog::init('" << fName << "')";
    pQueue_ = new CircularQueue(max_row, max_col);
    fName_ = fName;
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

void ALog::write(const char* pText)
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
    std::ofstream ofs(fName_, std::ofstream::out);
    FILE_LOG(logINFO) << "ALog::consume() started";
    for(std::size_t i = 0; !(stopping_ && pQueue_->empty()); ++i)
    {
        char* pData = pQueue_->getNextReadBuffer();
        if (pData)
        {
            unsigned long long& dt = *reinterpret_cast<unsigned long long*>(pData);
            FILE_LOG(logINFO) << "Consumer read: " << (void*)pData << ", dt = " << dt;
            ++read;
        }
        if (i % 1000 == 0)
            usleep(1);
    }
    FILE_LOG(logINFO) << "ALog::consume() exited";
}

void ALog::stop()
{
    FILE_LOG(logINFO) << "ALog::stop() enter";
    stopping_ = true;
    if (consumer_.joinable())
        consumer_.join();
    FILE_LOG(logINFO) << "ALog::stop() exit";
}

ALog& ALog::get()
{
    static ALog log;
    return log;
}

struct ALogMsg
{
    ALogMsg(); 
    ~ALogMsg();
    ALogMsg& operator <<(int);
    ALogMsg& operator <<(unsigned int);
    ALogMsg& operator <<(long unsigned int i);
    ALogMsg& operator <<(double);
    ALogMsg& operator <<(const char*);
private:
    char* pData;
};

inline unsigned long long rdtsc() {
    unsigned int lo, hi;
    asm volatile (
     "rdtsc"
     : "=a"(lo), "=d"(hi) /* outputs */
     : "a"(0)             /* inputs */
     : "%ebx", "%ecx");     /* clobbers*/
    return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

inline ALogMsg::ALogMsg()
{
    pData = ALog::get().pQueue_->getNextWriteBuffer();
    if (!pData)
    {
        ++ALog::get().lost;
        return;
    }

    *reinterpret_cast<unsigned long long*>(pData) = rdtsc();
    //*reinterpret_cast<unsigned long long*>(pData) = 0;
    FILE_LOG(logDEBUG) << "ALogMsg::ALogMsg()1" << (void*)pData;
    pData += sizeof(unsigned long long);
    FILE_LOG(logDEBUG) << "ALogMsg::ALogMsg()2" << (void*)pData;
}
  
inline ALogMsg::~ALogMsg()
{
    if (!pData) return;
    ALog::get().pQueue_->writeComplete();
    ++ALog::get().written;
}

inline ALogMsg& ALogMsg::operator <<(int i)
{
    if (!pData) return *this;
    pData += sizeof(int);
    return *this;
}

inline ALogMsg& ALogMsg::operator <<(unsigned int i)
{
    if (!pData) return *this;
    *reinterpret_cast<int*>(pData) = 10;
    pData += sizeof(int);
    *reinterpret_cast<unsigned int*>(pData) = i;
    pData += sizeof(unsigned int);
    return *this;
}

inline ALogMsg& ALogMsg::operator <<(long unsigned int i)
{
    if (!pData) return *this;
    *reinterpret_cast<int*>(pData) = 11;
    pData += sizeof(int);
    *reinterpret_cast<long unsigned int*>(pData) = i;
    pData += sizeof(long unsigned int);
    return *this;
}

inline ALogMsg& ALogMsg::operator <<(double)
{
    if (!pData) return *this;
    pData += sizeof(double);
    return *this;
}

inline ALogMsg& ALogMsg::operator <<(const char*)
{
    if (!pData) return *this;
    return *this;
}

#endif //__ALOG_H__
