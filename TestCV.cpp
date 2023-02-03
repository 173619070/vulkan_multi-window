// TestCV.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <Windows.h>
#include <iostream>

#include <functional>
#include <condition_variable>
#include <vector>
using namespace std;

struct ThreadData
{
    int index;
    int frameID;
};


class MyThread
{
public:
    typedef std::function<void()> ThreadFunc;

    explicit MyThread(ThreadFunc func)
        : m_ThreadFunc(func)
        , m_Thread(func)
    {

    }

    ~MyThread()
    {
        if (m_Thread.joinable())
        {
            m_Thread.join();
        }
    }

    MyThread(MyThread const&) = delete;

    MyThread& operator=(MyThread const&) = delete;

private:
    std::thread m_Thread;
    ThreadFunc  m_ThreadFunc;
};


std::mutex                  m_FrameStartLock;
std::condition_variable     m_FrameStartCV;

std::mutex                  m_ThreadDoneLock;
std::condition_variable     m_ThreadDoneCV;
int                         m_ThreadDoneCount;

std::vector<ThreadData*>    m_ThreadDatas;
std::vector<MyThread*>      m_Threads;
bool                        m_ThreadRunning;
int                         m_MainFrameID;

void ThreadRendering(void* param)
{
    ThreadData* threadData = (ThreadData*)param;
    threadData->frameID = 0;

    while (true)
    {
        {
            std::unique_lock<std::mutex> guardLock(m_FrameStartLock);
            if (threadData->frameID == m_MainFrameID)
            {
                printf("threadData->frameID == m_MainFrameID:%d %d idx=%d waiting\n", threadData->frameID, m_MainFrameID, threadData->index);
                m_FrameStartCV.wait(guardLock);
            }
            else
                printf("threadData->frameID != m_MainFrameID:%d %d idx=%d\n", threadData->frameID, m_MainFrameID, threadData->index);

        }

        threadData->frameID = m_MainFrameID;

        if (!m_ThreadRunning)
        {
            break;
        }

                printf("thread_frameid:                      %d %d idx=%d\n", threadData->frameID, m_MainFrameID, threadData->index);


        // notify thread done
        {
            std::lock_guard<std::mutex> lockGuard(m_ThreadDoneLock);
            m_ThreadDoneCount += 1;
            m_ThreadDoneCV.notify_one();
        }
    }
}


int main()
{
    int numThreads = 10;

    // thread task
    m_MainFrameID = 0;
    m_ThreadRunning = true;

    m_ThreadDatas.resize(numThreads);
    m_Threads.resize(numThreads);

    for (int i = 0; i < numThreads; ++i)
    {
        // prepare thread data
        m_ThreadDatas[i] = new ThreadData();
        // start thread
        m_ThreadDatas[i]->index = i;
        m_Threads[i] = new MyThread(
            [=]
            {
                ThreadRendering(m_ThreadDatas[i]);
            }
            );
    }

    while (1) {
        // notify fram start
        printf("update\n");
        {
            std::lock_guard<std::mutex> lockGuard(m_FrameStartLock);
            m_ThreadDoneCount = 0;
            m_MainFrameID += 1;
            m_FrameStartCV.notify_all();
        }

        // wait for thread done
        {
            std::unique_lock<std::mutex> lockGuard(m_ThreadDoneLock);
            while (m_ThreadDoneCount != m_Threads.size())
            {
                m_ThreadDoneCV.wait(lockGuard);
            }
        }
    }
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
