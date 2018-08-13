//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMetadataAccessorFactory.h"
#include "gpos/common/CAutoRef.h"
#include "naucrates/md/CMDProviderMemory.h"

namespace gpopt
{
	CMetadataAccessorFactory::CMetadataAccessorFactory
		(
			IMemoryPool *mp,
			CDXLMinidump *pdxlmd,
			const CHAR *file_name
		)
	{

		// set up MD providers
		CAutoRef<CMDProviderMemory> apmdp(GPOS_NEW(mp) CMDProviderMemory(mp, file_name));
		const SysidPtrArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
		CAutoRef<MDProviderPtrArray> apdrgpmdp(GPOS_NEW(mp) MDProviderPtrArray(mp));

		// ensure there is at least ONE system id
		apmdp->AddRef();
		apdrgpmdp->Append(apmdp.Value());

		for (ULONG ul = 1; ul < pdrgpsysid->Size(); ul++)
		{
			apmdp->AddRef();
			apdrgpmdp->Append(apmdp.Value());
		}

		m_apmda = GPOS_NEW(mp) CMDAccessor(mp, CMDCache::Pcache(), pdxlmd->GetSysidPtrArray(), apdrgpmdp.Value());
	}

	CMDAccessor *CMetadataAccessorFactory::Pmda()
	{
		return m_apmda.Value();
	}
}
