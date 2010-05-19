/* -*-c++-*-
 * Delta3D
 * Copyright 2010, Alion Science and Technology
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * This software was developed by Alion Science and Technology Corporation under
 * circumstances in which the U. S. Government may have rights in the software.
 *
 * David Guthrie
 */

#include <prefix/dtutilprefix.h>
#include <dtUtil/threadpool.h>

#include <OpenThreads/Thread>
#include <OpenThreads/Atomic>
#include <OpenThreads/Block>
#include <OpenThreads/Mutex>
#include <queue>
#include <set>
#include <map>
#include <algorithm>

namespace dtUtil
{

   class TaskThread;

   class DT_UTIL_EXPORT TaskQueue : public osg::Referenced
   {
       public:

           TaskQueue();

           /** Return true if the operation queue is empty. */
           bool Empty() const { return mTasks.empty(); }

           /** Return the num of pending tasks that are sitting in the TaskQueue.*/
           unsigned int GetNumTasksInQueue() const { return unsigned(mTasks.size()); }

           /** Add a task to end of TaskQueue, this will be
             * executed by the task thread once this operation gets to the head of the queue.*/
           void Add(ThreadPoolTask& task, unsigned queueId);

//           /** Remove task from TaskQueue.*/
//           void Remove(ThreadPoolTask& task);
//
//           /** Remove named task from TaskQueue.*/
//           void Remove(const dtUtil::RefString& name);

           /** Remove all tasks from TaskQueue.*/
           void RemoveAllTasks();

           /** Run all the tasks with the given priority. and optionally wait until all threads complete their tasks for this queue as well.
            */
           void ExecuteTasks(bool waitForAllTasksToBeCompleted = true, unsigned maxQueueId = 0);

           /**
            * Run one task
            * @param blockIfEmpty if the queue is empty at the start, then block until a task is queued or the block
            *                     is otherwise released.
            * @param maxQueueId execute only tasks with a queue id less than equal to the one passed it.
            * @return true if a task was executed.
            */
           bool ExecuteSingleTask(bool blockIfEmpty = true, unsigned maxQueueId = INT_MAX);

           /** Release tasks block that is used to block threads that are waiting on an empty tasks queue.*/
           void ReleaseTasksBlock();

           typedef std::set<TaskThread*> TaskThreads;

           /** Get the set of TaskThreads that are sharing this TaskQueue. */
           const TaskThreads& getTaskThreads() const { return mTaskThreads; }

       protected:

           virtual ~TaskQueue();

           friend class TaskThread;

           void AddTaskThread(TaskThread* thread);
           void RemoveTaskThread(TaskThread* thread);

           struct TaskQueueItem
           {
              dtCore::RefPtr<ThreadPoolTask> mTask;
              unsigned mQueueId;
              bool operator < (const TaskQueueItem& item) const { return mQueueId < item.mQueueId; }
           };

           typedef std::priority_queue<TaskQueueItem> Tasks;

           OpenThreads::Mutex     mTasksMutex;
           OpenThreads::Block     mTasksBlock;
           Tasks                  mTasks;

           TaskThreads            mTaskThreads;
           std::map<unsigned, unsigned>  mInProcessTasks;
   };

   TaskQueue::TaskQueue():
       osg::Referenced(true)
   {
   }

   TaskQueue::~TaskQueue()
   {
   }

   void TaskQueue::Add(ThreadPoolTask& task, unsigned queueId)
   {
       // acquire the lock on the operations queue to prevent anyone else for modifying it at the same time
       OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);

       TaskQueueItem newItem;
       newItem.mTask = &task;
       newItem.mQueueId = queueId;
       // add the operation to the end of the list
       mTasks.push(newItem);

       mTasksBlock.release();
   }

//   void TaskQueue::Remove(ThreadPoolTask& task)
//   {
//       // acquire the lock on the operations queue to prevent anyone else for modifying it at the same time
//       OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);
//
//       for(Tasks::iterator itr = mTasks.begin();
//           itr!=mTasks.end();)
//       {
//           if ((*itr)==&task)
//           {
//               bool needToResetCurrentIterator = (mCurrentTaskIterator == itr);
//
//               itr = mTasks.erase(itr);
//
//               if (needToResetCurrentIterator) mCurrentTaskIterator = itr;
//
//           }
//           else ++itr;
//       }
//
//       if (mTasks.empty())
//       {
//           mTasksBlock.reset();
//       }
//   }

//   void TaskQueue::Remove(const dtUtil::RefString& name)
//   {
//       // acquire the lock on the operations queue to prevent anyone else for modifying it at the same time
//       OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);
//
//       // find the remove all operations with specified name
//       for(Tasks::iterator itr = mTasks.begin();
//           itr!=mTasks.end();)
//       {
//           if ((*itr)->GetName() == name)
//           {
//               bool needToResetCurrentIterator = (mCurrentTaskIterator == itr);
//
//               itr = mTasks.erase(itr);
//
//               if (needToResetCurrentIterator) mCurrentTaskIterator = itr;
//           }
//           else ++itr;
//       }
//
//       if (mTasks.empty())
//       {
//           mTasksBlock.reset();
//       }
//   }

   void TaskQueue::RemoveAllTasks()
   {
       OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);

       while (!mTasks.empty())
       {
          mTasks.pop();
       }

        mTasksBlock.reset();
   }

   bool TaskQueue::ExecuteSingleTask(bool blockIfEmpty, unsigned maxQueueId)
   {
      if (blockIfEmpty && mTasks.empty())
      {
          mTasksBlock.block();
      }

      dtCore::RefPtr<ThreadPoolTask> currentTask = NULL;
      unsigned queueId = 0;
      {
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);

         if (mTasks.empty())
         {
            return false;
         }

         queueId = mTasks.top().mQueueId;

         if (queueId > maxQueueId)
         {
            return false;
         }

         currentTask = mTasks.top().mTask;

         ++mInProcessTasks[queueId];

         mTasks.pop();

         if (mTasks.empty())
         {
            mTasksBlock.reset();
         }
      }

      /// execute
      (*currentTask)();

      if (currentTask->GetKeep())
      {
         // re-add the task before decrementing the in process count so that code won't think all tasks are done
         Add(*currentTask, queueId);
      }

      {
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);

         --mInProcessTasks[queueId];
      }

      return true;
   }

   void TaskQueue::ExecuteTasks(bool waitForAllTasksToBeComplete, unsigned maxQueueId)
   {
      // First empty the queue
      while (ExecuteSingleTask(false, maxQueueId))
         ;

      if (waitForAllTasksToBeComplete)
      {
         unsigned tasksInProcess = 1;
         // Then wait for all other threads to complete their tasks.
         while (tasksInProcess > 0)
         {
            tasksInProcess = 0;

            {
               OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mTasksMutex);

               std::map<unsigned, unsigned>::iterator i, iend;
               i = mInProcessTasks.begin();
               iend = mInProcessTasks.end();
               for (; i != iend; ++i)
               {
                  if (i->first <= maxQueueId)
                  {
                     tasksInProcess += i->second;
                  }
               }
            }

            if (tasksInProcess > 0)
            {
               OpenThreads::Thread::YieldCurrentThread();
            }
         }
      }
   }

   void TaskQueue::ReleaseTasksBlock()
   {
       mTasksBlock.release();
   }

   void TaskQueue::AddTaskThread(TaskThread* thread)
   {
       mTaskThreads.insert(thread);
   }

   void TaskQueue::RemoveTaskThread(TaskThread* thread)
   {
      mTaskThreads.erase(thread);
   }

   class  TaskThread : public osg::Referenced, public OpenThreads::Thread
   {
   public:
      TaskThread(TaskQueue& queue);

      /** Run does the operation thread run loop.*/
      virtual void run();

      /** Cancel this thread.*/
      virtual int cancel();

   protected:

      virtual ~TaskThread();

      OpenThreads::Mutex         mThreadMutex;
      dtCore::RefPtr<TaskQueue>  mTaskQueue;
      volatile bool mDone;
   };

   TaskThread::TaskThread(TaskQueue& queue)
   : osg::Referenced(true)
   , mTaskQueue(&queue)
   , mDone(false)
   {
   }

   TaskThread::~TaskThread()
   {
      cancel();
   }

   int TaskThread::cancel()
   {
      int result = 0;
      if (isRunning())
      {
         result = OpenThreads::Thread::cancel();
         mDone = true;
         mTaskQueue->ReleaseTasksBlock();

         int i = 200;
         // then wait for the the thread to stop running.
         while (isRunning() && i > 0)
         {
            mTaskQueue->ReleaseTasksBlock();
            OpenThreads::Thread::YieldCurrentThread();
            --i;
         }
      }

      return result;
   }

   void TaskThread::run()
   {
      bool firstTime = true;

      // Run Loop
      while (!mDone)
      {
         dtCore::RefPtr<ThreadPoolTask> task;
         dtCore::RefPtr<TaskQueue> queue;

         {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mThreadMutex);
            queue = mTaskQueue;
         }

         //printf("Preparing To Run a task! %p \n", this);
         // execute any task and block if there are none
         if ((!queue->ExecuteSingleTask() && !mDone) || firstTime)
         {
            //printf("Yielding worker thread! %p \n", this);
            OpenThreads::Thread::YieldCurrentThread();
            firstTime = false;
         }


         testCancel();
      }

   }



   //////////////////////////////////////////////////
   //////////////////////////////////////////////////
   ThreadPoolTask::ThreadPoolTask()
   : mName("Task")
   , mKeep(false)
   {
   }

   IMPLEMENT_PROPERTY(ThreadPoolTask, dtUtil::RefString, Name);
   IMPLEMENT_PROPERTY(ThreadPoolTask, bool, Keep);


   //////////////////////////////////////////////////
   //////////////////////////////////////////////////

   class ThreadPoolImpl
   {
   public:
      ThreadPoolImpl()
      : mTaskThreadForBackgroundOnly(false)
      , mInitialized(false)
      {
      }

      dtCore::RefPtr<TaskQueue> mTaskQueue;
      dtCore::RefPtr<TaskQueue> mBackgroundQueue;

      std::vector<dtCore::RefPtr<TaskThread> > mTaskThreads;
      bool mTaskThreadForBackgroundOnly;
      bool mInitialized;
   };

   static ThreadPoolImpl gThreadPoolImpl;

   //////////////////////////////////////////////////
   //////////////////////////////////////////////////
   void ThreadPool::Init(int numThreads)
   {
      if (gThreadPoolImpl.mInitialized)
      {
         return;
      }

      if (numThreads < 0)
      {
         numThreads = OpenThreads::GetNumberOfProcessors() - 1;
      }

      gThreadPoolImpl.mTaskQueue = new TaskQueue;
      gThreadPoolImpl.mBackgroundQueue = gThreadPoolImpl.mTaskQueue;

      if (numThreads <= 0)
      {
         // On a single core box, or if the user specifies 0 worker threads,
         // we still have to create one thread for background processes.
         // Immediate stuff will only be run when ExecuteTasks is called.
         numThreads = 1;
         gThreadPoolImpl.mTaskThreadForBackgroundOnly = true;
         gThreadPoolImpl.mBackgroundQueue = new TaskQueue;
      }

      for (int i = 0; i < numThreads; ++i)
      {
         dtCore::RefPtr<TaskThread> newThread;
         // the background queue may also be the main task queue.
         newThread = new TaskThread(*gThreadPoolImpl.mBackgroundQueue);

         gThreadPoolImpl.mTaskThreads.push_back(newThread);
         newThread->start();
      }
      gThreadPoolImpl.mInitialized = true;
   }

   //////////////////////////////////////////////////
   void ThreadPool::Shutdown()
   {
      gThreadPoolImpl.mTaskThreads.clear();
      gThreadPoolImpl.mTaskQueue = NULL;
      gThreadPoolImpl.mBackgroundQueue = NULL;
      gThreadPoolImpl.mInitialized = false;
   }

   //////////////////////////////////////////////////
   void ThreadPool::AddTask(ThreadPoolTask& task, PoolQueue queue)
   {
      if (queue == IMMEDIATE)
      {
         gThreadPoolImpl.mTaskQueue->Add(task, 0);
      }
      else if (queue == BACKGROUND)
      {
         // in cases where worker threads > 0, the background queue is the same pointer as the task queue
         gThreadPoolImpl.mBackgroundQueue->Add(task, 1);
      }
   }

   //////////////////////////////////////////////////
   void ThreadPool::ExecuteTasks()
   {
      gThreadPoolImpl.mTaskQueue->ExecuteTasks(true, 0);
   }

   //////////////////////////////////////////////////
   unsigned ThreadPool::GetNumImmediateWorkerThreads()
   {
      if (gThreadPoolImpl.mTaskThreadForBackgroundOnly)
      {
         return 1U;
      }

      return gThreadPoolImpl.mTaskThreads.size() + 1U;
   }

   //////////////////////////////////////////////////
   //////////////////////////////////////////////////
   //////////////////////////////////////////////////
   //////////////////////////////////////////////////
   ThreadPool::ThreadPool()
   {
   }

   //////////////////////////////////////////////////
   ThreadPool::ThreadPool(ThreadPool&)
   {
   }

   //////////////////////////////////////////////////
   ThreadPool::~ThreadPool()
   {
   }

   //////////////////////////////////////////////////
   ThreadPool& ThreadPool::operator=(ThreadPool&)
   {
      return *this;
   }

}