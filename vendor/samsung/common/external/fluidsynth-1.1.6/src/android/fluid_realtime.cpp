#include <fluid_realtime.h>
#include <pthread.h>
#include <pthread_internal.h>
#include <SchedulingPolicyService.h>
#include <fluid_sys.h>

int fluid_getAndroidRealtime(pid_t tid, int pro)    
{
    int res;
    pthread_internal_t* thread_;
    thread_ = reinterpret_cast<pthread_internal_t*>(tid);

    if ((res = android::requestPriority(getpid(), thread_->tid, pro)) != 0) {
        fluid_log(FLUID_ERR, "Failed to get SCHED_FIFO priority pid %d tid %d; error %d",
		    getpid(), thread_->tid, res);
        return -1;
    }
    return 0;
}

