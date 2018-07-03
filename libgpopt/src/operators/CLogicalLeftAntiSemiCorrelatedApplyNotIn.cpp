//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CLogicalLeftAntiSemiCorrelatedApplyNotIn.cpp
//
//	@doc:
//		Implementation of left anti semi correlated apply with NOT-IN/ANY semantics
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiCorrelatedApplyNotIn.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiCorrelatedApplyNotIn::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftAntiSemiCorrelatedApplyNotIn::PxfsCandidates
	(
	IMemoryPool *mp
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfImplementLeftAntiSemiCorrelatedApplyNotIn);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiCorrelatedApplyNotIn::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalLeftAntiSemiCorrelatedApplyNotIn::PopCopyWithRemappedColumns
	(
	IMemoryPool *mp,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *pdrgpcrInner = CUtils::PdrgpcrRemap(mp, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalLeftAntiSemiCorrelatedApplyNotIn(mp, pdrgpcrInner, m_eopidOriginSubq);
}


// EOF

