//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CDynamicPtrArray.h
//
//	@doc:
//		Dynamic array of pointers frequently used in optimizer data structures
//---------------------------------------------------------------------------
#ifndef GPOS_CDynamicArray_H
#define GPOS_CDynamicArray_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/clibwrapper.h"

namespace gpos
{
	// frequently used destroy functions

	// NOOP
	template<class T>
	inline void CleanupNULL(T*) {}

	// plain delete
	template<class T>
	inline void CleanupDelete(T *pt)
	{
		GPOS_DELETE(pt);
	}

	// delete of array
	template<class T>
	inline void CleanupDeleteRg(T *pt)
	{
		GPOS_DELETE_ARRAY(pt);
	}

	// release ref-count'd object
	template<class T>
	inline void CleanupRelease(T *pt)
	{
		(dynamic_cast<CRefCount*>(pt))->Release();
	}
	

	// comparison function signature
	typedef INT (*PfnCompare)(const void *, const void *);

	//---------------------------------------------------------------------------
	//	@class:
	//		CDynamicArray
	//
	//	@doc:
	//		Simply dynamic array for pointer types
	//
	//---------------------------------------------------------------------------
	template <class T>
	class CDynamicArray : public CRefCount
	{
		private:
		
			// memory pool
			IMemoryPool *m_pmp;
			
			// currently allocated size
			ULONG m_ulAllocated;
			
			// min size
			ULONG m_ulMinSize;
			
			// current size
			ULONG m_ulSize;

			// expansion factor
			ULONG m_ulExp;

			// actual array
			T *m_pt;

			// default comparison function for elements
			static INT Cmp(const void *pv1, const void *pv2)
            {
                T t1 = *(T *) pv1;
				T t2 = *(T *) pv2;

                if (t1 < t2)
                {
                    return -1;
                }

                if (t1 > t2)
                {
                    return 1;
                }

                return 0;
            }

			// private copy ctor
			CDynamicArray<T> (const CDynamicArray<T> &);
			
			// resize function
			void Resize(ULONG ulNewSize)
            {
                GPOS_ASSERT(ulNewSize > m_ulAllocated && "Invalid call to Resize, cannot shrink array");

                // get new target array
                T *pt = GPOS_NEW_ARRAY(m_pmp, T, ulNewSize);

                if (m_ulSize > 0)
                {
                    GPOS_ASSERT(NULL != m_pt);

                    clib::PvMemCpy(pt, m_pt, sizeof(T) * m_ulSize);

                    GPOS_DELETE_ARRAY(m_pt);
                }

                m_pt = pt;

                m_ulAllocated = ulNewSize;
            }
			
		public:
		
			// ctor
			explicit
			CDynamicArray<T> (IMemoryPool *pmp, ULONG ulMinSize = 4, ULONG ulExp = 10)
            :
            m_pmp(pmp),
            m_ulAllocated(0),
            m_ulMinSize(std::max((ULONG)4, ulMinSize)),
            m_ulSize(0),
            m_ulExp(std::max((ULONG)2, ulExp)),
            m_pt(NULL)
            {
                // do not allocate in constructor; defer allocation to first insertion
            }

			// dtor
			~CDynamicArray<T> ()
            {
                Clear();

                GPOS_DELETE_ARRAY(m_pt);
            }
	
			// clear elements
			void Clear()
            {
                for(ULONG i = 0; i < m_ulSize; i++)
                {
					// pfnDestroy(m_pt[i]);
                }
                m_ulSize = 0;
            }
	
			// append element to end of array
			void Append(T t)
            {
                if (m_ulSize == m_ulAllocated)
                {
                    // resize at least by 4 elements or percentage as given by ulExp
                    ULONG ulExpand = (ULONG) (m_ulAllocated * (1 + (m_ulExp/100.0)));
                    ULONG ulMinExpand = m_ulAllocated + 4;

                    Resize(std::max(std::max(ulMinExpand, ulExpand), m_ulMinSize));
                }

                GPOS_ASSERT(m_ulSize < m_ulAllocated);

                m_pt[m_ulSize] = t;
                ++m_ulSize;
            }
			
			// append array -- flatten it
			void AppendArray(const CDynamicArray<T> *pdrg)
            {
                GPOS_ASSERT(NULL != pdrg);
                GPOS_ASSERT(this != pdrg && "Cannot append array to itself");

                ULONG ulTotalSize = m_ulSize + pdrg->m_ulSize;
                if (ulTotalSize > m_ulAllocated)
                {
                    Resize(ulTotalSize);
                }

                GPOS_ASSERT(m_ulSize <= m_ulAllocated);
                GPOS_ASSERT_IMP(m_ulSize == m_ulAllocated, 0 == pdrg->m_ulSize);

                GPOS_ASSERT(ulTotalSize <= m_ulAllocated);

                // at this point old memory is no longer accessible, hence, no self-copy
                if (pdrg->m_ulSize > 0)
                {
                    GPOS_ASSERT(NULL != pdrg->m_ppt);
                    clib::PvMemCpy(m_pt + m_ulSize, pdrg->m_ppt, pdrg->m_ulSize * sizeof(T));
                }

                m_ulSize = ulTotalSize;
            }

			
			// number of elements currently held
			ULONG UlLength() const
            {
                return m_ulSize;
            }

			// sort array
			void Sort(PfnCompare pfncompare = Cmp)
            {
                clib::QSort(m_pt, m_ulSize, sizeof(T), pfncompare);
            }

			// equality check
			BOOL FEqual(const CDynamicArray<T> *pdrg) const
            {
                BOOL fEqual = (UlLength() == pdrg->UlLength());

                for (ULONG i = 0; i < m_ulSize && fEqual; i++)
                {
                    fEqual = (m_pt[i] == pdrg->m_pt[i]);
                }

                return fEqual;
            }
			
			// lookup object
			T PtLookup(const T t) const
            {
                GPOS_ASSERT(NULL != t);

                for (ULONG i = 0; i < m_ulSize; i++)
                {
                    if (m_pt[i] == t)
                    {
                        return m_pt[i];
                    }
                }

                return NULL;
            }
			
			// lookup object position
			ULONG UlPos(const T t) const
            {
                GPOS_ASSERT(NULL != t);

                for (ULONG ul = 0; ul < m_ulSize; ul++)
                {
                    if (m_pt[ul] == t)
                    {
                        return ul;
                    }
                }

                return ULONG_MAX;
            }

#ifdef GPOS_DEBUG
			// check if array is sorted
			BOOL FSorted() const
            {
                for (ULONG i = 1; i < m_ulSize; i++)
                {
                    if ((ULONG_PTR)(m_pt[i - 1]) > (ULONG_PTR)(m_pt[i]))
                    {
                        return false;
                    }
                }

                return true;
            }
#endif // GPOS_DEBUG
						
			// accessor for n-th element
			T operator [] (ULONG ulPos) const
            {
                GPOS_ASSERT(ulPos < m_ulSize && "Out of bounds access");
                return (T) m_pt[ulPos];
            }
			
			// replace an element in the array
			void Replace(ULONG ulPos, T pt)
            {
                GPOS_ASSERT(ulPos < m_ulSize && "Out of bounds access");
				// pfnDestroy(m_pt[ulPos]);
                m_pt[ulPos] = pt;
            }
	}; // class CDynamicArray
		

	// commonly used array types

	// arrays of unsigned integers
	typedef CDynamicArray<ULONG> DrgPul;
	// array of unsigned integer arrays
//	typedef CDynamicArray<DrgPul, CleanupRelease> DrgPdrgPul;
//
//	// arrays of integers
//	typedef CDynamicArray<INT, CleanupDelete> DrgPi;
//
//	// array of strings
//	typedef CDynamicArray<CWStringBase, CleanupDelete> DrgPstr;
//
//	// arrays of chars
//	typedef CDynamicArray<CHAR, CleanupDelete> DrgPsz;

}

#endif // !GPOS_CDynamicArray_H

// EOF

