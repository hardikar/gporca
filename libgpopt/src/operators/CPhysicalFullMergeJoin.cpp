//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalInnerNLJoin.cpp
//
//	@doc:
//		Implementation of inner nested-loops join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CCastUtils.h"

#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "gpopt/operators/CPhysicalFullMergeJoin.h"


using namespace gpopt;


// ctor
CPhysicalFullMergeJoin::CPhysicalFullMergeJoin
	(
	IMemoryPool *mp
	)
	:
	CPhysicalJoin(mp)
{}


// dtor
CPhysicalFullMergeJoin::~CPhysicalFullMergeJoin()
{}

CDistributionSpec *
CPhysicalFullMergeJoin::PdsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CDistributionSpec *, //pdsRequired,
	ULONG child_index,
	CDrvdProp2dArray *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
	const
{
	GPOS_ASSERT(2 > child_index);

	// if expression has to execute on a single host then we need a gather
//	if (exprhdl.NeedsSingletonExecution())
//	{
//		return PdsRequireSingleton(mp, exprhdl, pdsRequired, child_index);
//	}

	if (exprhdl.HasOuterRefs())
	{
		// XXX TODO Do something here
//		if (CDistributionSpec::EdtSingleton == pdsRequired->Edt() ||
//			CDistributionSpec::EdtReplicated == pdsRequired->Edt())
//		{
//			return PdsPassThru(mp, exprhdl, pdsRequired, child_index);
//		}
//		return GPOS_NEW(mp) CDistributionSpecReplicated();
	}

	CExpression *pexprScalarChild = exprhdl.PexprScalarChild(2);
	CExpression *pexprLHS = (*pexprScalarChild)[0];
	CExpression *pexprRHS = (*pexprScalarChild)[1];
	GPOS_ASSERT(CPredicateUtils::FComparison(pexprScalarChild, IMDType::EcmptEq));
	GPOS_ASSERT(CUtils::FScalarIdent(pexprLHS));
	GPOS_ASSERT(CUtils::FScalarIdent(pexprRHS));

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	CExpression *pexpr = (*pexprScalarChild)[child_index];
	pexpr->AddRef();
	pdrgpexpr->Append(pexpr);

	CDistributionSpecHashed *pds = GPOS_NEW(mp) CDistributionSpecHashed(pdrgpexpr, true /* fNullsCollocated */);
	return pds;
}


COrderSpec *
CPhysicalFullMergeJoin::PosRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	COrderSpec *, //posInput
	ULONG child_index,
	CDrvdProp2dArray *, //pdrgpdpCtxt
	ULONG //ulOptReq
	)
	const
{
	// Merge joins require their input to be sorted on corresponsing join clauses. Without
	// making dangerous assumptions of the implementation of the merge joins, it is difficult
	// to predict the order of the output of the merge join. (This may not be true). In that
	// case, it is better to not push down any order requests from above.

	// This is complex. We need to sort each side depending on the join filter
	// but also we need to include any order specs from above.

	// For now, ignore required orders
	// For now, not cache cache
	// For now, assume that scalar expr is ident = ident ONLY

	CExpression *pexprScalarChild = exprhdl.PexprScalarChild(2);
	CExpression *pexprLHS = (*pexprScalarChild)[0];
	CExpression *pexprRHS = (*pexprScalarChild)[1];
	GPOS_ASSERT(CPredicateUtils::FComparison(pexprScalarChild, IMDType::EcmptEq));
	GPOS_ASSERT(CUtils::FScalarIdent(pexprLHS));
	GPOS_ASSERT(CUtils::FScalarIdent(pexprRHS));

	COrderSpec *os = GPOS_NEW(mp) COrderSpec(mp);
	CExpression *pexpr = (*pexprScalarChild)[child_index];
	CScalarIdent *popScId = CScalarIdent::PopConvert(pexpr->Pop());
	const CColRef *colref = popScId->Pcr();
	// XXX TODO can be > depending on posInput
	gpmd::IMDId *mdid = colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
	mdid->AddRef();
	
	os->Append(mdid, colref, COrderSpec::EntLast);

	return os;
}

// compute required rewindability of the n-th child
CRewindabilitySpec *
CPhysicalFullMergeJoin::PrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CRewindabilitySpec *prsRequired,
	ULONG child_index,
	CDrvdProp2dArray *, // pdrgpdpCtxt
	ULONG // ulOptReq
	) const
{
	GPOS_ASSERT(child_index < 2 &&
				"Required rewindability can be computed on the relational child only");

	// if there are outer references, then we need a materialize on both children
	if (exprhdl.HasOuterRefs())
	{
		return GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtRewindable, prsRequired->Emht());
	}

	// XXX TODO: this is probably all that's needed
	// pass through requirements to outer child
	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

// return order property enforcing type for this operator
CEnfdProp::EPropEnforcingType
CPhysicalFullMergeJoin::EpetOrder
	(
	CExpressionHandle &,
	const CEnfdOrder *
#ifdef GPOS_DEBUG
	 peo
#endif // GPOS_DEBUG
	) const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	// XXX TODO: Should probably be unnecessary if compatible, like Sort
	return CEnfdProp::EpetRequired;
}

// EOF

