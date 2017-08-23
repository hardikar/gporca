/*
 * CMDIndexInfo.h
 *
 *  Created on: Aug 22, 2017
 *      Author: shardikar
 */

#ifndef INCLUDE_NAUCRATES_MD_CMDINDEXINFO_H_
#define INCLUDE_NAUCRATES_MD_CMDINDEXINFO_H_

#include "gpos/base.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDInterface.h"

namespace gpopt
{
	using namespace gpos;
	using namespace gpmd;


	class CMDIndexInfo : public IMDInterface {
	private:

		IMDId *m_mdid;
		BOOL m_fIsPartial;

	public:
		CMDIndexInfo(IMDId *mdid, BOOL fIsPartial);
		virtual ~CMDIndexInfo();

		IMDId *PIMDId();
		BOOL FIsPartial();

		virtual
		void Serialize(gpdxl::CXMLSerializer *) const;
	};

}

#endif /* INCLUDE_NAUCRATES_MD_CMDINDEXINFO_H_ */
