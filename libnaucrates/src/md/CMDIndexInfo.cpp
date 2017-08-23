/*
 * CMDIndexInfo.cpp
 *
 *  Created on: Aug 22, 2017
 *      Author: shardikar
 */

#include "naucrates/md/CMDIndexInfo.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpopt;

CMDIndexInfo::CMDIndexInfo(IMDId *mdid, BOOL fIsPartial)
	: m_mdid(mdid), m_fIsPartial(fIsPartial)
{
}

CMDIndexInfo::~CMDIndexInfo()
{
	m_mdid->Release();
}


IMDId *CMDIndexInfo::PIMDId()
{
	return m_mdid;
}

BOOL CMDIndexInfo::FIsPartial()
{
	return m_fIsPartial;
}

void CMDIndexInfo::Serialize(gpdxl::CXMLSerializer *pxmlser) const
{
	pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenIndex));

	m_mdid->Serialize(pxmlser, CDXLTokens::PstrToken(EdxltokenMdid));
	pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenIndexPartial), m_fIsPartial);

	pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenIndex));
}
