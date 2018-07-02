//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDelete.cpp
//
//	@doc:
//		Implementation of logical Delete operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CPartIndexMap.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalDelete.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::CLogicalDelete
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDelete::CLogicalDelete
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_ptabdesc(NULL),
	m_pdrgpcr(NULL),
	m_pcrCtid(NULL),
	m_pcrSegmentId(NULL)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::CLogicalDelete
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalDelete::CLogicalDelete
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	ColRefArray *colref_array,
	CColRef *pcrCtid,
	CColRef *pcrSegmentId
	)
	:
	CLogical(memory_pool),
	m_ptabdesc(ptabdesc),
	m_pdrgpcr(colref_array),
	m_pcrCtid(pcrCtid),
	m_pcrSegmentId(pcrSegmentId)

{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != colref_array);
	GPOS_ASSERT(NULL != pcrCtid);
	GPOS_ASSERT(NULL != pcrSegmentId);

	m_pcrsLocalUsed->Include(m_pdrgpcr);
	m_pcrsLocalUsed->Include(m_pcrCtid);
	m_pcrsLocalUsed->Include(m_pcrSegmentId);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::~CLogicalDelete
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalDelete::~CLogicalDelete()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcr);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalDelete::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalDelete *popDelete = CLogicalDelete::PopConvert(pop);

	return m_pcrCtid == popDelete->PcrCtid() &&
			m_pcrSegmentId == popDelete->PcrSegmentId() &&
			m_ptabdesc->MDId()->Equals(popDelete->Ptabdesc()->MDId()) &&
			m_pdrgpcr->Equals(popDelete->Pdrgpcr());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalDelete::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcr));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrCtid));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrSegmentId));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDelete::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *colref_array = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcr, colref_mapping, must_exist);
	CColRef *pcrCtid = CUtils::PcrRemap(m_pcrCtid, colref_mapping, must_exist);
	CColRef *pcrSegmentId = CUtils::PcrRemap(m_pcrSegmentId, colref_mapping, must_exist);
	m_ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CLogicalDelete(memory_pool, m_ptabdesc, colref_array, pcrCtid, pcrSegmentId);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalDelete::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & //exprhdl
	)
{
	CColRefSet *pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsOutput->Include(m_pdrgpcr);
	return pcrsOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalDelete::PkcDeriveKeys
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalDelete::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	// pass on max card of first child
	return exprhdl.GetRelationalProperties(0)->Maxcard();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDelete::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfDelete2DML);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalDelete::PstatsDerive
	(
	IMemoryPool *, // memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // not used
	)
	const
{
	return PstatsPassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDelete::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalDelete::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os	<< SzId()
		<< " (";
	m_ptabdesc->Name().OsPrint(os);
	os << "), Deleted Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcr);
	os	<< "], ";
	m_pcrCtid->OsPrint(os);
	os	<< ", ";
	m_pcrSegmentId->OsPrint(os);

	return os;
}

// EOF

