/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #define LOG_NDEBUG 0
#define LOG_TAG "libutils.threads"

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cutils/log.h"
#include "Errors.h"
#include "Thread.h"


/*
 * ===========================================================================
 *      Thread wrappers
 * ===========================================================================
 */

namespace cocos2d {

// ----------------------------------------------------------------------------

/*
 * This is our thread object!
 */

Thread::Thread(bool canCallJava)
:         mThread(nullptr),
          mStatus(NO_ERROR),
          mExitPending(false), mRunning(false)
    , mTid(-1)
{

}

Thread::~Thread() {
    mThread->join();
    delete mThread;
}

status_t Thread::readyToRun() {
    return NO_ERROR;
}

status_t Thread::run() {
    std::lock_guard<std::mutex> _l(mLock);

    if (mRunning) {
        // thread already started
        return INVALID_OPERATION;
    }

    // reset status and exitPending to their default value, so we can
    // try again after an error happened (either below, or in readyToRun())
    mStatus = NO_ERROR;
    mExitPending = false;
    mThread = nullptr;

    // hold a strong reference on ourself
    mHoldSelf.reset(this);

    mRunning = true;

    mThread = new std::thread(_threadLoop, this);

    if (mThread == nullptr) {
        mStatus = UNKNOWN_ERROR;   // something happened!
        mRunning = false;
        mThread = nullptr;
        mHoldSelf.reset();  // "this" may have gone away after this.

        return UNKNOWN_ERROR;
    }

    // Do not refer to mStatus here: The thread is already running (may, in fact
    // already have exited with a valid mStatus result). The NO_ERROR indication
    // here merely indicates successfully starting the thread and does not
    // imply successful termination/execution.
    return NO_ERROR;

    // Exiting scope of mLock is a memory barrier and allows new thread to run
}

int Thread::_threadLoop(void *user) {
    Thread *const self = static_cast<Thread *>(user);

    std::shared_ptr <Thread> strong(self->mHoldSelf);
    std::weak_ptr <Thread> weak(strong);
    self->mHoldSelf.reset();
    // this is very useful for debugging with gdb
    self->mTid = std::this_thread::get_id();

    bool first = true;

    do {
        bool result;
        if (first) {
            first = false;
            self->mStatus = self->readyToRun();
            result = (self->mStatus == NO_ERROR);

            if (result && !self->exitPending()) {
                // Binder threads (and maybe others) rely on threadLoop
                // running at least once after a successful ::readyToRun()
                // (unless, of course, the thread has already been asked to exit
                // at that point).
                // This is because threads are essentially used like this:
                //   (new ThreadSubclass())->run();
                // The caller therefore does not retain a strong reference to
                // the thread and the thread would simply disappear after the
                // successful ::readyToRun() call instead of entering the
                // threadLoop at least once.
                result = self->threadLoop();
            }
        } else {
            result = self->threadLoop();
        }

        // establish a scope for mLock
        {
            std::unique_lock<std::mutex> _l(self->mLock);
            if (result == false || self->mExitPending) {
                self->mExitPending = true;
                self->mRunning = false;
                // clear thread ID so that requestExitAndWait() does not exit if
                // called by a new thread using the same thread ID as this one.
                self->mThread = nullptr;
                // note that interested observers blocked in requestExitAndWait are
                // awoken by broadcast, but blocked on mLock until break exits scope
                self->mThreadExitedCondition.notify_all();
                break;
            }
        }

        // Release our strong reference, to let a chance to the thread
        // to die a peaceful death.
        strong.reset();
        // And immediately, re-acquire a strong reference for the next loop
        strong = weak.lock();
    } while (strong != 0);

    return 0;
}

void Thread::requestExit() {
    std::lock_guard<std::mutex> _l(mLock);
    mExitPending = true;
}

status_t Thread::requestExitAndWait() {
    std::unique_lock<std::mutex> _l(mLock);
    if (mThread->get_id() == std::this_thread::get_id()) {
        ALOGW(
                "Thread (this=%p): don't call waitForExit() from this "
                        "Thread object's thread. It's a guaranteed deadlock!",
                this);

        return WOULD_BLOCK;
    }

    mExitPending = true;

    while (mRunning == true) {
        mThreadExitedCondition.wait(_l);
    }
    // This next line is probably not needed any more, but is being left for
    // historical reference. Note that each interested party will clear flag.
    mExitPending = false;

    return mStatus;
}

status_t Thread::join() {
    std::unique_lock<std::mutex> _l(mLock);
    if (mThread->get_id() == std::this_thread::get_id()) {
        ALOGW(
                "Thread (this=%p): don't call join() from this "
                        "Thread object's thread. It's a guaranteed deadlock!",
                this);

        return WOULD_BLOCK;
    }

    while (mRunning == true) {
        mThreadExitedCondition.wait(_l);
    }

    return mStatus;
}

bool Thread::isRunning() const {
    std::lock_guard<std::mutex> _l(mLock);
    return mRunning;
}

std::thread::id Thread::getTid() const
{
    // mTid is not defined until the child initializes it, and the caller may need it earlier
    std::lock_guard<std::mutex> _l(mLock);
    std::thread::id tid;
    if (mRunning) {
        tid = mThread->get_id();
    } else {
        ALOGW("Thread (this=%p): getTid() is undefined before run()", this);
//        tid = nullptr;
    }
    return tid;
}

bool Thread::exitPending() const {
    std::lock_guard<std::mutex> _l(mLock);
    return mExitPending;
}

};  // namespace cocos2d
