//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalGet.cpp
//
//	@doc:
//		Implementation of basic table access
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/COptCtxt.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalGet.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CName.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::CLogicalGet
//
//	@doc:
//		ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalGet::CLogicalGet
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_pnameAlias(NULL),
	m_ptabdesc(NULL),
	m_pdrgpcrOutput(NULL),
	m_pdrgpdrgpcrPart(NULL),
	m_pcrsDist(NULL)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::CLogicalGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalGet::CLogicalGet
	(
	IMemoryPool *memory_pool,
	const CName *pnameAlias,
	CTableDescriptor *ptabdesc
	)
	:
	CLogical(memory_pool),
	m_pnameAlias(pnameAlias),
	m_ptabdesc(ptabdesc),
	m_pdrgpcrOutput(NULL),
	m_pdrgpdrgpcrPart(NULL),
	m_pcrsDist(NULL)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameAlias);

	// generate a default column set for the table descriptor
	m_pdrgpcrOutput = PdrgpcrCreateMapping(memory_pool, m_ptabdesc->Pdrgpcoldesc(), UlOpId());
	
	if (m_ptabdesc->IsPartitioned())
	{
		m_pdrgpdrgpcrPart = PdrgpdrgpcrCreatePartCols(memory_pool, m_pdrgpcrOutput, m_ptabdesc->PdrgpulPart());
	}

	m_pcrsDist = CLogical::PcrsDist(memory_pool, m_ptabdesc, m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::CLogicalGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalGet::CLogicalGet
	(
	IMemoryPool *memory_pool,
	const CName *pnameAlias,
	CTableDescriptor *ptabdesc,
	ColRefArray *pdrgpcrOutput
	)
	:
	CLogical(memory_pool),
	m_pnameAlias(pnameAlias),
	m_ptabdesc(ptabdesc),
	m_pdrgpcrOutput(pdrgpcrOutput),
	m_pdrgpdrgpcrPart(NULL)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameAlias);

	if (m_ptabdesc->IsPartitioned())
	{
		m_pdrgpdrgpcrPart = PdrgpdrgpcrCreatePartCols(memory_pool, m_pdrgpcrOutput, m_ptabdesc->PdrgpulPart());
	}

	m_pcrsDist = CLogical::PcrsDist(memory_pool, m_ptabdesc, m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::~CLogicalGet
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CLogicalGet::~CLogicalGet()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrOutput);
	CRefCount::SafeRelease(m_pdrgpdrgpcrPart);
	CRefCount::SafeRelease(m_pcrsDist);
	
	GPOS_DELETE(m_pnameAlias);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalGet::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalGet::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}
	CLogicalGet *popGet = CLogicalGet::PopConvert(pop);
	
	return m_ptabdesc->MDId()->Equals(popGet->m_ptabdesc->MDId()) &&
			m_pdrgpcrOutput->Equals(popGet->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalGet::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *pdrgpcrOutput = NULL;
	if (must_exist)
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemapAndCreate(memory_pool, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrOutput, colref_mapping, must_exist);
	}
	CName *pnameAlias = GPOS_NEW(memory_pool) CName(memory_pool, *m_pnameAlias);
	m_ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CLogicalGet(memory_pool, pnameAlias, m_ptabdesc, pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalGet::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & // exprhdl
	)
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(m_pdrgpcrOutput);

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PcrsDeriveNotNull
//
//	@doc:
//		Derive not null output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalGet::PcrsDeriveNotNull
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
	const
{
	// get all output columns
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(CDrvdPropRelational::GetRelationalProperties(exprhdl.Pdp())->PcrsOutput());

	// filters out nullable columns
	CColRefSetIter crsi(*CDrvdPropRelational::GetRelationalProperties(exprhdl.Pdp())->PcrsOutput());
	while (crsi.Advance())
	{
		CColRefTable *pcrtable = CColRefTable::PcrConvert(const_cast<CColRef*>(crsi.Pcr()));
		if (pcrtable->IsNullable())
		{
			pcrs->Exclude(pcrtable);
		}
	}

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::FInputOrderSensitive
//
//	@doc:
//		Not called for leaf operators
//
//---------------------------------------------------------------------------
BOOL
CLogicalGet::FInputOrderSensitive() const
{
	GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalGet::PkcDeriveKeys
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & // exprhdl
	)
	const
{
	const BitSetArray *pdrgpbs = m_ptabdesc->PdrgpbsKeys();

	return CLogical::PkcKeysBaseTable(memory_pool, pdrgpbs, m_pdrgpcrOutput);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalGet::PxfsCandidates
	(
	IMemoryPool *memory_pool
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	
	(void) xform_set->ExchangeSet(CXform::ExfGet2TableScan);
	
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PstatsDerive
//
//	@doc:
//		Load up statistics from metadata
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalGet::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // not used
	)
	const
{
	// requesting stats on distribution columns to estimate data skew
	IStatistics *pstatsTable = PstatsBaseTable(memory_pool, exprhdl, m_ptabdesc, m_pcrsDist);
	
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool, m_pdrgpcrOutput);
	CUpperBoundNDVs *upper_bound_NDVs = GPOS_NEW(memory_pool) CUpperBoundNDVs(pcrs, pstatsTable->Rows());
	CStatistics::CastStats(pstatsTable)->AddCardUpperBound(upper_bound_NDVs);

	return pstatsTable;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalGet::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}
	else
	{
		os << SzId() << " ";
		// alias of table as referenced in the query
		m_pnameAlias->OsPrint(os);

		// actual name of table in catalog and columns
		os << " (";
		m_ptabdesc->Name().OsPrint(os);
		os << "), Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
		os << "] Key sets: {";
		
		const ULONG ulColumns = m_pdrgpcrOutput->Size();
		const BitSetArray *pdrgpbsKeys = m_ptabdesc->PdrgpbsKeys();
		for (ULONG ul = 0; ul < pdrgpbsKeys->Size(); ul++)
		{
			CBitSet *pbs = (*pdrgpbsKeys)[ul];
			if (0 < ul)
			{
				os << ", ";
			}
			os << "[";
			ULONG ulPrintedKeys = 0;
			for (ULONG ulKey = 0; ulKey < ulColumns; ulKey++)
			{
				if (pbs->Get(ulKey))
				{
					if (0 < ulPrintedKeys)
					{
						os << ",";
					}
					os << ulKey;
					ulPrintedKeys++;
				}
			}
			os << "]";
		}
		os << "}";
		
	}
		
	return os;
}



// EOF

