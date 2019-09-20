//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008-2010 Greenplum Inc.
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMemoryPool.cpp
//
//	@doc:
//		Implementation of abstract interface; 
//		implements helper functions for extraction of allocation
//		header from memory block;
//---------------------------------------------------------------------------

#ifdef GPOS_DEBUG
#include "gpos/error/CFSimulator.h"
#endif // GPOS_DEBUG
#include "gpos/memory/CMemoryPool.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/memory/CMemoryVisitorPrint.h"
#include "gpos/task/ITask.h"


using namespace gpos;

// invalid exception
const ULONG_PTR CMemoryPool::m_invalid = ULONG_PTR_MAX;


ULONG
CMemoryPool::SizeOfAlloc
	(
	const void *ptr
	)
{
	GPOS_ASSERT(NULL != ptr);

	const AllocHeader *header = static_cast<const AllocHeader*>(ptr) - 1;
	return header->m_alloc;
}

//---------------------------------------------------------------------------
//	@function:
//		DeleteImpl
//
//	@doc:
//		implementation of Delete that can be used by operator new functions
//
//---------------------------------------------------------------------------
void
CMemoryPool::DeleteImpl
	(
	void *ptr,
	EAllocationType eat
	)
{
	// deletion of NULL pointers is legal
	if (NULL == ptr)
	{
		return;
	}

	// release allocation
	FreeAlloc(ptr, eat);

}  // namespace gpos

// EOF

#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPool::Print
//
//	@doc:
//		Walk all objects and print identification
//
//---------------------------------------------------------------------------
IOstream &
CMemoryPool::OsPrint
	(
	IOstream &os
	)
{
	os << "Memory pool: " << this;

	ITask *task = ITask::Self();
	if (NULL != task && task->IsTraceSet(EtracePrintMemoryLeakStackTrace))
	{
		os << ", stack trace: " << std::endl;
		m_stack_desc.AppendTrace(os, 8 /*ulDepth*/);
	}
	else
	{
		os << std::endl;
	}

	if (SupportsLiveObjectWalk())
	{
		CMemoryVisitorPrint visitor(os);
		WalkLiveObjects(&visitor);
	}

	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPool::AssertEmpty
//
//	@doc:
//		Static helper function to print out if the given pool is empty;
//		if the pool is not empty and we are compiled with debugging on then
//		an assertion will be thrown;
//
//---------------------------------------------------------------------------
void
CMemoryPool::AssertEmpty
	(
	IOstream &os
	)
{
	if (SupportsLiveObjectWalk() && NULL != ITask::Self() &&
	    !GPOS_FTRACE(EtraceDisablePrintMemoryLeak))
	{
		CMemoryVisitorPrint visitor(os);
		WalkLiveObjects(&visitor);

		if (0 != visitor.GetNumVisits())
		{
			os
				<< "Unfreed memory in memory pool "
				<< (void*)this
				<< ": "
				<< visitor.GetNumVisits()
				<< " objects leaked"
				<< std::endl;

			GPOS_ASSERT(!"leak detected");
		}
	}
}

#endif // GPOS_DEBUG


// EOF

