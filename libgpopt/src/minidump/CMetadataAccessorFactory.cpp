//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMetadataAccessorFactory.h"
#include "gpos/common/CSharedPtr.h"
#include "naucrates/md/CMDProviderMemory.h"


namespace gpopt
{
	CMetadataAccessorFactory::CMetadataAccessorFactory
		(
			IMemoryPool *pmp,
			CDXLMinidump *pdxlmd,
			const CHAR *szFileName
		)
	{

		// set up MD providers
		CSharedPtr<IMDProvider> spmdp(GPOS_NEW(pmp) CMDProviderMemory(pmp, szFileName));

		const DrgPsysid *pdrgpsysid = pdxlmd->Pdrgpsysid();
		CSharedPtr<DrgPmdp> spdrgpmdp(GPOS_NEW(pmp) DrgPmdp(pmp));

		// ensure there is at least ONE system id
		spdrgpmdp->Append(spmdp);

		for (ULONG ul = 1; ul < pdrgpsysid->UlLength(); ul++)
		{
			spdrgpmdp->Append(spmdp);
		}

		m_apmda = GPOS_NEW(pmp) CMDAccessor(pmp, CMDCache::Pcache(), pdxlmd->Pdrgpsysid(), spdrgpmdp.Get());
	}

	CMDAccessor *CMetadataAccessorFactory::Pmda()
	{
		return m_apmda.Pt();
	}
}
