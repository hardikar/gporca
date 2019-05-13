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
#include "gpopt/base/CColRefSetIter.h"

#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "gpopt/operators/CPhysicalFullMergeJoin.h"

using namespace gpopt;

#define GPOPT_MAX_HASH_DIST_REQUESTS	6

// ctor
CPhysicalFullMergeJoin::CPhysicalFullMergeJoin
	(
	IMemoryPool *mp,
	CExpressionArray *outer_merge_clauses,
	CExpressionArray *inner_merge_clauses
	)
	:
	CPhysicalJoin(mp),
	m_outer_merge_clauses(outer_merge_clauses),
	m_inner_merge_clauses(inner_merge_clauses)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != outer_merge_clauses);
	GPOS_ASSERT(NULL != inner_merge_clauses);
	GPOS_ASSERT(outer_merge_clauses->Size() == inner_merge_clauses->Size());

	SetDistrRequests(outer_merge_clauses->Size() + 1);
}


// dtor
CPhysicalFullMergeJoin::~CPhysicalFullMergeJoin()
{
	m_outer_merge_clauses->Release();
	m_inner_merge_clauses->Release();
}

CDistributionSpec *
CPhysicalFullMergeJoin::PdsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CDistributionSpec *pdsRequired,
	ULONG child_index,
	CDrvdProp2dArray *, //pdrgpdpCtxt,
	ULONG ulOptReq
	)
	const
{
	GPOS_ASSERT(2 > child_index);

	// if expression has to execute on a single host then we need a gather
	if (exprhdl.NeedsSingletonExecution() || exprhdl.HasOuterRefs())
	{
		return PdsRequireSingleton(mp, exprhdl, pdsRequired, child_index);
	}

	CExpressionArray *clauses = (child_index == 0) ? m_outer_merge_clauses: m_inner_merge_clauses;

	// XXX TODO Handle case when either side is replicated.
	ULONG num_hash_reqs = std::min((ULONG) GPOPT_MAX_HASH_DIST_REQUESTS, clauses->Size());
	if (ulOptReq < num_hash_reqs)
	{
		CExpressionArray *pdrgpexprCurrent = GPOS_NEW(mp) CExpressionArray(mp);
		CExpression *expr = (*clauses)[ulOptReq];
		expr->AddRef();
		pdrgpexprCurrent->Append(expr);

		CDistributionSpecHashed *pds =
			GPOS_NEW(mp) CDistributionSpecHashed(pdrgpexprCurrent, true /* fNullsCollocated */);
		return pds;
	}

	GPOS_ASSERT(ulOptReq == num_hash_reqs);
	clauses->AddRef();
	CDistributionSpecHashed *pds =
		GPOS_NEW(mp) CDistributionSpecHashed(clauses, true /* fNullsCollocated */);
	return pds;
}


COrderSpec *
CPhysicalFullMergeJoin::PosRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &, //exprhdl,
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

	COrderSpec *os = GPOS_NEW(mp) COrderSpec(mp);

	CExpressionArray *clauses;
	if (child_index == 0)
	{
		clauses = m_outer_merge_clauses;
	}
	else
	{
		GPOS_ASSERT(child_index == 1);
		clauses = m_inner_merge_clauses;
	}

	CColRefSet *ordering_cols = GPOS_NEW(mp) CColRefSet(mp);
	for (ULONG ul = 0; ul < clauses->Size(); ++ul)
	{
		CExpression *expr = (*clauses)[ul];

		GPOS_ASSERT(CUtils::FScalarIdent(expr));

		const CColRef *colref = CCastUtils::PcrExtractFromScIdOrCastScId(expr);
		ordering_cols->Include(colref);
	}

	CColRefSetIter iter(*ordering_cols);
	while (iter.Advance())
	{
		CColRef *colref = iter.Pcr();
		gpmd::IMDId *mdid = colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
		mdid->AddRef();
		os->Append(mdid, colref, COrderSpec::EntLast);
	}

	ordering_cols->Release();

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
	if (exprhdl.HasOuterRefs() || child_index == 1)
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

CEnfdDistribution::EDistributionMatching
CPhysicalFullMergeJoin::Edm
	(
	CReqdPropPlan *, // prppInput
	ULONG , // child_index,
	CDrvdProp2dArray *, // pdrgpdpCtxt,
	ULONG // ulOptReq
	)
{
	return CEnfdDistribution::EdmExact;
}

CDistributionSpec *
CPhysicalFullMergeJoin::PdsDerive
(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl
	)
	const
{
	CDistributionSpec *pdsOuter = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
#ifdef GPOS_DEBUG
	CDistributionSpec *pdsInner = exprhdl.Pdpplan(1 /*child_index*/)->Pds();
#endif

	if (CDistributionSpec::EdtHashed == pdsOuter->Edt())
	{
		// Merge join requires both sides to be hashed
		GPOS_ASSERT(CDistributionSpec::EdtHashed == pdsInner->Edt());

		CDistributionSpecHashed *pdshashed = CDistributionSpecHashed::PdsConvert(pdsOuter);
		pdshashed->Pdrgpexpr()->AddRef();
		// XXX TODO We should do something like PdsDeriveFromHashedChildren
		return GPOS_NEW(mp) CDistributionSpecHashed
		(
			pdshashed->Pdrgpexpr(),
			false /* fNullsCollocated */
		);
	}

	// Merge join requires both sides to be singleton if not hashed
	GPOS_ASSERT(CDistributionSpec::EdtSingleton == pdsOuter->Edt() ||
				CDistributionSpec::EdtStrictSingleton == pdsOuter->Edt());

	// otherwise, pass through outer distribution
	pdsOuter->AddRef();
	return pdsOuter;
}

