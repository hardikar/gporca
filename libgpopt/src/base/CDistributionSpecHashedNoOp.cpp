//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/base/CDistributionSpecHashedNoOp.h"
#include "gpopt/operators/CPhysicalMotionHashDistribute.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

CDistributionSpecHashedNoOp::CDistributionSpecHashedNoOp
	(
	ExpressionArray *pdrgpexpr
	)
:
CDistributionSpecHashed(pdrgpexpr, true)
{
}

CDistributionSpec::EDistributionType CDistributionSpecHashedNoOp::Edt() const
{
	return CDistributionSpec::EdtHashedNoOp;
}

BOOL CDistributionSpecHashedNoOp::Matches(const CDistributionSpec *pds) const
{
	return pds->Edt() == Edt();
}

void
CDistributionSpecHashedNoOp::AppendEnforcers
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CReqdPropPlan *,
	ExpressionArray *pdrgpexpr,
	CExpression *pexpr
	)
{
	DrvdPropArray *pdp = exprhdl.Pdp();
	CDistributionSpec *pdsChild = CDrvdPropPlan::Pdpplan(pdp)->Pds();
	CDistributionSpecHashed *pdsChildHashed = dynamic_cast<CDistributionSpecHashed *>(pdsChild);

	if (NULL == pdsChildHashed)
	{
		return;
	}

	ExpressionArray *pdrgpexprNoOpRedistributionColumns = pdsChildHashed->Pdrgpexpr();
	pdrgpexprNoOpRedistributionColumns->AddRef();
	CDistributionSpecHashedNoOp* pdsNoOp = GPOS_NEW(mp) CDistributionSpecHashedNoOp(pdrgpexprNoOpRedistributionColumns);
	pexpr->AddRef();
	CExpression *pexprMotion = GPOS_NEW(mp) CExpression
			(
					mp,
					GPOS_NEW(mp) CPhysicalMotionHashDistribute(mp, pdsNoOp),
					pexpr
			);
	pdrgpexpr->Append(pexprMotion);
}
