//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Inc.
//---------------------------------------------------------------------------

#include "gpos/common/IPrinter.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/string/CWStringDynamic.h"

using namespace gpos;

#ifdef GPOS_DEBUG

// Manages the memory for debug printer string
// NB: This is allocated the first time DbgPrint() is called; but never free'd
// till the server is shut down. But since DbgPrint() is intended for use in
// debug sessions only, this doesn't matter...
CWStringDynamic *
IPrinter::m_msg_holder = NULL;

const WCHAR *
IPrinter::DbgPrint() const
{
	if (NULL == m_msg_holder)
	{
		// First time - set up message holder
		IMemoryPool *mp = CMemoryPoolManager::GetMemoryPoolMgr()->GetGlobalMemoryPool();
		m_msg_holder = GPOS_NEW(mp) CWStringDynamic(mp);
	}
	m_msg_holder->Reset();

	COstreamString os(m_msg_holder);
	this->OsPrint(os);

	return m_msg_holder->GetBuffer();
}

#endif  // GPOS_DEBUG
// EOF

