//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2004-2015 Pivotal Software, Inc.
//
//	@filename:
//		CWorkerPoolManager.cpp
//
//	@doc:
//		Central scheduler; 
//		* maintains global worker-local-storage
//		* keeps track of all worker pools
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/common/CAutoP.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/error/CFSimulator.h" // for GPOS_FPSIMULATOR
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CMemoryPool.h"
#include "gpos/memory/CMemoryPoolInjectFault.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/memory/CMemoryPoolTracker.h"
#include "gpos/memory/CMemoryVisitorPrint.h"
#include "gpos/task/CAutoSuspendAbort.h"


using namespace gpos;
using namespace gpos::clib;


// global instance of memory pool manager
CMemoryPoolManager *CMemoryPoolManager::m_memory_pool_mgr = NULL;

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::CMemoryPoolManager
//
//	@doc:
//		Ctor.
//
//---------------------------------------------------------------------------
CMemoryPoolManager::CMemoryPoolManager
	(
	CMemoryPool *internal,
	EMemoryPoolType memory_pool_type
	)
	:
	m_internal_memory_pool(internal),
	m_allow_global_new(true),
	m_ht_all_pools(NULL),
	m_memory_pool_type(memory_pool_type)
{
	GPOS_ASSERT(NULL != internal);
	GPOS_ASSERT(GPOS_OFFSET(CMemoryPool, m_link) == GPOS_OFFSET(CMemoryPoolTracker, m_link));
}

void
CMemoryPoolManager::Setup()
{
	m_ht_all_pools = GPOS_NEW(m_internal_memory_pool) CSyncHashtable<CMemoryPool, ULONG_PTR>();
	m_ht_all_pools->Init
		(
		m_internal_memory_pool,
		GPOS_MEMORY_POOL_HT_SIZE,
		GPOS_OFFSET(CMemoryPool, m_link),
		GPOS_OFFSET(CMemoryPool, m_hash_key),
		&(CMemoryPool::m_invalid),
		HashULongPtr,
		EqualULongPtr
		);

	// create pool used in allocations made using global new operator
	m_global_memory_pool = CreateMemoryPool();
}

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Init
//
//	@doc:
//		Initializer for global memory pool manager
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolManager::Init()
{
	if (NULL == CMemoryPoolManager::m_memory_pool_mgr)
	{
		return SetupGlobalMemoryPoolManager<CMemoryPoolManager, CMemoryPoolTracker>();
	}

	return GPOS_OK;
}


CMemoryPool *
CMemoryPoolManager::CreateMemoryPool()
{
	CMemoryPool *mp = NewMemoryPool();

	// accessor scope
	{
		// HERE BE DRAGONS
		// See comment in CCache::InsertEntry
		const ULONG_PTR hashKey = mp->GetHashKey();
		MemoryPoolKeyAccessor acc(*m_ht_all_pools, hashKey);
		acc.Insert(mp);
	}

	return mp;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::NewMemoryPool
//
//	@doc:
//		Create new pool
//
//---------------------------------------------------------------------------
CMemoryPool *
CMemoryPoolManager::NewMemoryPool()
{
	return GPOS_NEW(m_internal_memory_pool) CMemoryPoolTracker();
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Destroy
//
//	@doc:
//		Release returned pool
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::Destroy
	(
	CMemoryPool *mp
	)
{
	GPOS_ASSERT(NULL != mp);

	// accessor scope
	{
		// HERE BE DRAGONS
		// See comment in CCache::InsertEntry
		const ULONG_PTR hashKey = mp->GetHashKey();
		MemoryPoolKeyAccessor acc(*m_ht_all_pools, hashKey);
		acc.Remove(mp);
	}

	mp->TearDown();

	GPOS_DELETE(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::TotalAllocatedSize
//
//	@doc:
//		Return total allocated size in bytes
//
//---------------------------------------------------------------------------
ULLONG
CMemoryPoolManager::TotalAllocatedSize()
{
	ULLONG total_size = 0;
	MemoryPoolIter iter(*m_ht_all_pools);
	while (iter.Advance())
	{
		MemoryPoolIterAccessor acc(iter);
		CMemoryPool *mp = acc.Value();
		if (NULL != mp)
		{
			total_size = total_size + mp->TotalAllocatedSize();
		}
	}

	return total_size;
}

void
CMemoryPoolManager::DeleteImpl(void* ptr, CMemoryPool::EAllocationType eat)
{
	CMemoryPoolTracker::DeleteImpl(ptr, eat);
}


ULONG
CMemoryPoolManager::UserSizeOfAlloc(const void* ptr)
{
	return CMemoryPoolTracker::UserSizeOfAlloc(ptr);
}
#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::OsPrint
//
//	@doc:
//		Print contents of all allocated memory pools
//
//---------------------------------------------------------------------------
IOstream &
CMemoryPoolManager::OsPrint
	(
	IOstream &os
	)
{
	os << "Print memory pools: " << std::endl;

	MemoryPoolIter iter(*m_ht_all_pools);
	while (iter.Advance())
	{
		CMemoryPool *mp = NULL;
		{
			MemoryPoolIterAccessor acc(iter);
			mp = acc.Value();
		}

		if (NULL != mp)
		{
			os << *mp << std::endl;
		}
	}

	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::PrintOverSizedPools
//
//	@doc:
//		Print memory pools with total allocated size above given threshold
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::PrintOverSizedPools
	(
	CMemoryPool *trace,
	ULLONG size_threshold // size threshold in bytes
	)
{
	CAutoTraceFlag Abort(EtraceSimulateAbort, false);
	CAutoTraceFlag OOM(EtraceSimulateOOM, false);
	CAutoTraceFlag Net(EtraceSimulateNetError, false);
	CAutoTraceFlag IO(EtraceSimulateIOError, false);

	MemoryPoolIter iter(*m_ht_all_pools);
	while (iter.Advance())
	{
		MemoryPoolIterAccessor acc(iter);
		CMemoryPool *mp = acc.Value();

		if (NULL != mp)
		{
			ULLONG size = mp->TotalAllocatedSize();
			if (size > size_threshold)
			{
				CAutoTrace at(trace);
				at.Os() << std::endl << "OVERSIZED MEMORY POOL: " << size << " bytes " << std::endl;
			}
		}
	}
}
#endif // GPOS_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::DestroyMemoryPoolAtShutdown
//
//	@doc:
//		Destroy a memory pool at shutdown
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::DestroyMemoryPoolAtShutdown
	(
	CMemoryPool *mp
	)
{
	GPOS_ASSERT(NULL != mp);

#ifdef GPOS_DEBUG
	gpos::oswcerr << "Leaked " << *mp << std::endl;
#endif // GPOS_DEBUG

	mp->TearDown();
	GPOS_DELETE(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Cleanup
//
//	@doc:
//		Clean-up memory pools
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::Cleanup()
{
#ifdef GPOS_DEBUG
	if (0 < m_global_memory_pool->TotalAllocatedSize())
	{
		// allocations made by calling global new operator are not deleted
		gpos::oswcerr << "Memory leaks detected"<< std::endl << *m_global_memory_pool << std::endl;
	}
#endif // GPOS_DEBUG

	GPOS_ASSERT(NULL != m_global_memory_pool);
	Destroy(m_global_memory_pool);

	// cleanup left-over memory pools;
	// any such pool means that we have a leak
	m_ht_all_pools->DestroyEntries(DestroyMemoryPoolAtShutdown);
	GPOS_DELETE(m_ht_all_pools);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Shutdown
//
//	@doc:
//		Delete memory pools and release manager
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::Shutdown()
{
	// cleanup remaining memory pools
	Cleanup();

	// save off pointers for explicit deletion
	CMemoryPool *internal = m_internal_memory_pool;

	GPOS_DELETE(CMemoryPoolManager::m_memory_pool_mgr);
	CMemoryPoolManager::m_memory_pool_mgr = NULL;

#ifdef GPOS_DEBUG
	internal->AssertEmpty(oswcerr);
#endif // GPOS_DEBUG

	Free(internal);
}

// EOF

