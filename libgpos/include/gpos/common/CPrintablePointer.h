//	Copyright (C) 2017 Pivotal Software, Inc.

#ifndef GPOS_CPrintablePointer_H
#define GPOS_CPrintablePointer_H

#include "gpos/io/IOstream.h"

namespace gpos
{
	template <typename T>
	class CPrintablePointer
	{
		private:
			T *m_obj;
			friend IOstream &operator << (IOstream &os, CPrintablePointer p)
			{
				if (p.m_obj)
				{
					return os << *p.m_obj;
				}
				else
				{
					return os;
				}
			}

		public:
			explicit CPrintablePointer(T *obj) : m_obj(obj) {}
			CPrintablePointer(const CPrintablePointer &pointer) : m_obj(pointer.m_obj) {}
	};

	template <typename T>
	CPrintablePointer<T> GetPrintablePtr(T *obj)
	{
		return CPrintablePointer<T>(obj);
	}
}
#endif // GPOS_CPrintablePointer_H
