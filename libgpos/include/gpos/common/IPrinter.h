//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Inc.
//
//	Interface for printing functions
//
//	TODO: Implement OsPrint() & DbgPrint() for all classes by inheriting from
//	this class.
//---------------------------------------------------------------------------
#ifndef GPOS_IPrinter_H
#define GPOS_IPrinter_H

#include "gpos/types.h"
#include "gpos/io/IOstream.h"

namespace gpos
{
	class CWStringDynamic;

	class IPrinter
	{
		private:

			// private copy ctor
			IPrinter(const IPrinter&);

#ifdef GPOS_DEBUG
			static
			CWStringDynamic *m_msg_holder;
#endif  // GPOS_DEBUG

		public:

			// ctor
			IPrinter()
			{}

			// dtor
			virtual
			~IPrinter()
			{}

			// print driver
			virtual
			IOstream &OsPrint(IOstream &os) const = 0;

#ifdef GPOS_DEBUG
			// debug print; for interactive debugging sessions only
			const WCHAR *DbgPrint() const;
#endif  // GPOS_DEBUG

	}; // class IPrinter
}

#endif // !GPOS_IPrinter_H

// EOF

