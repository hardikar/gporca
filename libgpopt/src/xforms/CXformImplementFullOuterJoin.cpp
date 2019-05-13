//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementFullOuterJoin.h"
#include "gpopt/operators/CPhysicalFullMergeJoin.h"
#include "gpopt/xforms/CXformUtils.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


// ctor
CXformImplementFullOuterJoin::CXformImplementFullOuterJoin
	(
	IMemoryPool *mp
	)
	:
	CXformExploration
		(
		 // pattern
		GPOS_NEW(mp) CExpression
				(
				mp,
				GPOS_NEW(mp) CLogicalFullOuterJoin(mp),
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)), // outer child
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)), // inner child
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))  // scalar child
				)
		)
{}

CXform::EXformPromise
CXformImplementFullOuterJoin::Exfp
	(
	CExpressionHandle & //exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}

void
CXformImplementFullOuterJoin::Transform
	(
	CXformContext *pxfctxt,
	CXformResult *pxfres,
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CXformUtils::ImplementMergeJoin<CPhysicalFullMergeJoin>(pxfctxt, pxfres, pexpr);
}

// EOF