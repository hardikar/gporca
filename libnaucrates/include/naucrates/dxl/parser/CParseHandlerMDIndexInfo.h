//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDIndex.h
//
//	@doc:
//		SAX parse handler class for parsing an MD index
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDIndexInfo_H
#define GPDXL_CParseHandlerMDIndexInfo_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/IMDIndex.h"
#include "naucrates/md/CMDrelationGPDB.h"

namespace gpdxl
{
	using namespace gpos;
	using namespace gpmd;
	using namespace gpnaucrates;

	XERCES_CPP_NAMESPACE_USE
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerMDIndex
	//
	//	@doc:
	//		Parse handler class for parsing an MD index
	//
	//---------------------------------------------------------------------------
	class CParseHandlerMDIndexInfo : public CParseHandlerBase
	{
		private:
			// mdid of the index
			IMDId *m_pmdid;

			// is the index partial
			BOOL m_fPartial;

			DrgPmdIndexInfo *m_pdrgpmdIndexInfo;

			// private copy ctor
			CParseHandlerMDIndexInfo(const CParseHandlerMDIndexInfo&);

			// process the start of an element
			void StartElement
				(
				const XMLCh* const xmlszUri, 		// URI of element's namespace
 				const XMLCh* const xmlszLocalname,	// local part of element's name
				const XMLCh* const xmlszQname,		// element's qname
				const Attributes& attr				// element's attributes
				);

			// process the end of an element
			void EndElement
				(
				const XMLCh* const xmlszUri, 		// URI of element's namespace
				const XMLCh* const xmlszLocalname,	// local part of element's name
				const XMLCh* const xmlszQname		// element's qname
				);

		public:

			// ctor
			CParseHandlerMDIndexInfo
				(
				IMemoryPool *pmp,
				CParseHandlerManager *pphm,
				CParseHandlerBase *pphRoot
				);

			DrgPmdIndexInfo *PdrgpmdIndexInfo();
	};
}

#endif // !GPDXL_CParseHandlerMDIndexInfo_H

// EOF
