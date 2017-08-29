//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CSharedPtr.h
//
//	@doc:
//		Base class for reference counting in the optimizer; 
//		Ref-counting is single-threaded only;
//		Enforces allocation on the heap, i.e. no deletion unless the 
//		count has really dropped to zero
//---------------------------------------------------------------------------
#ifndef GPOS_CSharedPtr_H
#define GPOS_CSharedPtr_H

#include "gpos/utils.h"
#include "gpos/assert.h"
#include "gpos/sync/atomic.h"
#include "gpos/types.h"

#include "gpos/error/CException.h"
#include "gpos/common/CHeapObject.h"
#include "gpos/task/ITask.h"

#ifdef GPOS_32BIT
#define GPOS_WIPED_MEM_PATTERN		0xCcCcCcCc
#else
#define GPOS_WIPED_MEM_PATTERN		0xCcCcCcCcCcCcCcCc
#endif

namespace gpos
{
	template <typename T>
	class CSharedPtr
	{
		private:
			CRefCount *m_ptr;

		public:
			// Constructor
			explicit CSharedPtr(T *ptr) : m_ptr(ptr)
			{
			}

			// Copy constructor
			explicit CSharedPtr(const CSharedPtr& sptr) : m_ptr(sptr.m_ptr)
			{
				sptr.m_ptr->AddRef();
			}

			~CSharedPtr()
			{
				m_ptr->Release();
			}

			T *Get() const
			{
				return dynamic_cast<T*> (m_ptr);
			}

			T* operator ->() const
			{
				return Get();
			}

			T& operator * () const
			{
				return *Get();
			}

	}; // class CSharedPtr
}

#endif // !GPOS_CSharedPtr_H

// EOF

