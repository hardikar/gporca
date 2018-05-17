//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CTask.h
//
//	@doc:
//		Interface class for task abstraction
//---------------------------------------------------------------------------
#ifndef GPOS_CTask_H
#define GPOS_CTask_H

#include "gpos/base.h"
#include "gpos/common/CList.h"
#include "gpos/error/CException.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/sync/CAutoMutex.h"
#include "gpos/sync/CEvent.h"
#include "gpos/sync/CMutex.h"
#include "gpos/task/CTaskContext.h"
#include "gpos/task/CTaskId.h"
#include "gpos/task/CTaskLocalStorage.h"
#include "gpos/task/ITask.h"
#include "gpos/task/IWorker.h"

namespace gpos
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CTask
	//
	//	@doc:
	//		Interface to abstract task (work unit);
	//		provides asynchronous task execution and error handling;
	//
	//---------------------------------------------------------------------------
	class CTask : public ITask
	{	

		friend class CAutoTaskProxy;
		friend class CAutoTaskProxyTest;
		friend class CTaskSchedulerFifo;
		friend class CWorker;
		friend class CWorkerPoolManager;
		friend class CUnittest;
		friend class CAutoSuspendAbort;

		private:

			// task memory pool -- exclusively used by this task
			IMemoryPool *m_pmp;
		
			// task context
			CTaskContext *m_task_ctxt;

			// error context
			IErrorContext *m_err_ctxt;
				
			// error handler stack
			CErrorHandler *m_err_handle;		

			// function to execute
			void *(*m_func)(void *);
			
			// function argument
			void *m_arg;
			
			// function result
			void *m_res;

			// TLS
			CTaskLocalStorage m_tls;
			
			// mutex for status change
			CMutexBase *m_mutex;

			// event to signal status change
			CEvent *m_event;

			// task status
			volatile ETaskStatus m_status;

			// cancellation flag
			volatile BOOL *m_cancel;

			// local cancellation flag; used when no flag is externally passed
			volatile BOOL m_cancel_local;

			// counter of requests to suspend cancellation
			ULONG m_abort_suspend_count;

			// flag denoting task completion report
			BOOL m_reported;

			// task identifier
			CTaskId m_tid;

			// ctor
			CTask
				(
				IMemoryPool *pmp,
				CTaskContext *task_ctxt,
				IErrorContext *err_ctxt,
				CEvent *event,
				volatile BOOL *cancel
				);

			// no copy ctor
			CTask(const CTask&);

			// set task status
			void SetStatus(ETaskStatus status);

			// signal task completion or error
			void Signal(ETaskStatus status) throw();

			// binding a task structure to a function and its arguments
			void Bind(void *(*func)(void*), void *arg);

			// execution, called by the owning worker
			void Execute();

			// check if task has been scheduled
			BOOL Scheduled() const;

			// check if task finished executing
			BOOL Finished() const;

			// check if task is currently executing
			BOOL Running() const
			{
				return EtsRunning == m_status;
			}

			// reported flag accessor
			BOOL Reported() const
			{
				return m_reported;
			}

			// set reported flag
			void SetReported()
			{
				GPOS_ASSERT(!m_reported && "Task already reported as completed");

				m_reported = true;
			}

		public:

			// dtor
			virtual ~CTask();

			// accessor for memory pool, e.g. used for allocating task parameters in
			IMemoryPool *Pmp() const
			{
				return m_pmp;
			}

			// TLS accessor
			CTaskLocalStorage &Tls()
			{
				return m_tls;
			}

			// task id accessor
			CTaskId &Tid()
			{
				return m_tid;
			}

			// task context accessor
			CTaskContext *TaskCtxt() const
			{
				return m_task_ctxt;
			}

			// basic output streams
			ILogger *LogOut() const
			{
				return this->m_task_ctxt->LogOut();
			}
			
			ILogger *LogErr() const
			{
				return this->m_task_ctxt->LogErr();
			}
			
			BOOL Trace
				(
				ULONG trace,
				BOOL val
				)
			{
				return this->m_task_ctxt->Trace(trace, val);
			}
			
			BOOL Trace
				(
				ULONG trace
				)
			{
				return this->m_task_ctxt->Trace(trace);
			}

			
			// locale
			ELocale Eloc() const
			{
				return m_task_ctxt->Eloc();
			}

			// check if task is canceled
			BOOL Canceled() const
			{
				return *m_cancel;
			}

			// reset cancel flag
			void ResetCancel()
			{
				*m_cancel = false;
			}

			// set cancel flag
			void Cancel()
			{
				*m_cancel = true;
			}

			// check if a request to suspend abort was received
			BOOL AbortSuspended() const
			{
				return (0 < m_abort_suspend_count);
			}

			// increment counter for requests to suspend abort
			void SuspendAbort()
			{
				m_abort_suspend_count++;
			}

			// decrement counter for requests to suspend abort
 			void ResumeAbort();

			// task status accessor
			ETaskStatus Status() const
			{
				return m_status;
			}

			// task result accessor
			void *Res() const
			{
				return m_res;
			}

			// error context
			IErrorContext *ErrCtxt() const
			{
				return m_err_ctxt;
			}
			
			// error context
			CErrorContext *ErrCtxtConvert()
			{
				return dynamic_cast<CErrorContext*>(m_err_ctxt);
			}

			// pending exceptions
			BOOL PendingExceptions() const
			{
				return m_err_ctxt->FPending();
			}

#ifdef GPOS_DEBUG
			// check if task has expected status
			BOOL CheckStatus(BOOL completed);
#endif // GPOS_DEBUG

			// slink for auto task proxy
			SLink m_linkAtp;

			// slink for task scheduler
			SLink m_linkTs;

			// slink for worker pool manager
			SLink m_linkWpm;

			static
			CTask *TaskSelf()
			{
				return dynamic_cast<CTask *>(ITask::TaskSelf());
			}

	}; // class CTask

}

#endif // !GPOS_CTask_H

// EOF

