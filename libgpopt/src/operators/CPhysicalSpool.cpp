//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalSpool.cpp
//
//	@doc:
//		Implementation of spool operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalSpool.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::CPhysicalSpool
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalSpool::CPhysicalSpool
	(
	IMemoryPool *mp,
	BOOL eager
	)
	:
	CPhysical(mp),
	m_eager(eager)
{}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::~CPhysicalSpool
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalSpool::~CPhysicalSpool()
{}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PcrsRequired
//
//	@doc:
//		Compute required output columns of the n-th child
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalSpool::PcrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG child_index,
	CDrvdProp2dArray *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);

	return PcrsChildReqd(mp, exprhdl, pcrsRequired, child_index,
						 gpos::ulong_max);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalSpool::PosRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	COrderSpec *posRequired,
	ULONG child_index,
	CDrvdProp2dArray *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	return PosPassThru(mp, exprhdl, posRequired, child_index);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSpool::PdsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CDistributionSpec *pdsRequired,
	ULONG child_index,
	CDrvdProp2dArray *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	return PdsPassThru(mp, exprhdl, pdsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PppsRequired
//
//	@doc:
//		Compute required partition propagation of the n-th child
//
//---------------------------------------------------------------------------
CPartitionPropagationSpec *
CPhysicalSpool::PppsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CPartitionPropagationSpec *pppsRequired,
	ULONG child_index,
	CDrvdProp2dArray *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);
	GPOS_ASSERT(NULL != pppsRequired);
	
	return CPhysical::PppsRequiredPushThru(mp, exprhdl, pppsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalSpool::PcteRequired
	(
	IMemoryPool *, //mp,
	CExpressionHandle &, //exprhdl,
	CCTEReq *pcter,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif
	,
	CDrvdProp2dArray *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);
	return PcterPushThru(pcter);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalSpool::PrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CRewindabilitySpec *prsRequired,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif // GPOS_DEBUG
	,
	CDrvdProp2dArray *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	// spool establishes rewindability on its own
	CRewindabilitySpec::EMotionHazardType motion_hazard = (prsRequired->HasMotionHazard() && !FEager()) ?
														  CRewindabilitySpec::EmhtMotion :
														  CRewindabilitySpec::EmhtNoMotion;

	if (exprhdl.HasOuterRefs())
		return GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtRewindable, motion_hazard);
	return GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtNotRewindable, motion_hazard);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PosDerive
//
//	@doc:
//		Derive sort order
//
//--------------------------------------------------------------------------
COrderSpec *
CPhysicalSpool::PosDerive
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PosDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PdsDerive
//
//	@doc:
//		Derive distribution
//
//--------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSpool::PdsDerive
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PdsDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//--------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalSpool::PrsDerive
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl
	)
	const
{
	CRewindabilitySpec *prsChild = PrsDerivePassThruOuter(exprhdl);
	CRewindabilitySpec::EMotionHazardType motion_hazard = (!FEager() && prsChild->HasMotionHazard()) ?
														  CRewindabilitySpec::EmhtMotion :
														  CRewindabilitySpec::EmhtNoMotion;
	prsChild->Release();

	return GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtRewindable, motion_hazard);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalSpool::Matches
	(
	COperator *pop
	)
	const
{
	if(Eopid() == pop->Eopid())
	{
		CPhysicalSpool *popSpool = CPhysicalSpool::PopConvert(pop);
		return m_eager == popSpool->FEager();
	}

	return false;
}

ULONG
CPhysicalSpool::HashValue() const
{
	ULONG hash = COperator::HashValue();
	return  gpos::CombineHashes(hash, gpos::HashValue<BOOL>(&m_eager));
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalSpool::FProvidesReqdCols
	(
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG // ulOptReq
	)
	const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSpool::EpetOrder
	(
	CExpressionHandle &, // exprhdl
	const CEnfdOrder *
#ifdef GPOS_DEBUG
	peo
#endif // GPOS_DEBUG
	)
	const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	// spool is order-preserving, sort enforcers have already been added
	return CEnfdProp::EpetUnnecessary;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::EpetDistribution
//
//	@doc:
//		Return the enforcing type for distribution property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSpool::EpetDistribution
	(
	CExpressionHandle &/*exprhdl*/,
	const CEnfdDistribution *
#ifdef GPOS_DEBUG
	ped
#endif // GPOS_DEBUG
	)
	const
{
	GPOS_ASSERT(NULL != ped);

	// spool is distribution-preserving,
	// distribution enforcers have already been added
	return CEnfdProp::EpetUnnecessary;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSpool::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSpool::EpetRewindability
	(
	CExpressionHandle &, // exprhdl
	const CEnfdRewindability * // per
	)
	const
{
	// no need for enforcing rewindability on output
	return CEnfdProp::EpetUnnecessary;
}


BOOL
CPhysicalSpool::FValidContext
	(
	IMemoryPool *,
	COptimizationContext *poc,
	COptimizationContextArray *pdrgpocChild
	)
	const
{
	GPOS_ASSERT(NULL != pdrgpocChild);
	GPOS_ASSERT(1 == pdrgpocChild->Size());

	COptimizationContext *pocChild = (*pdrgpocChild)[0];
	CCostContext *pccBest = pocChild->PccBest();
	GPOS_ASSERT(NULL != pccBest);

	// partition selections that happen outside of a physical spool does not do
	// any good on rescan: a physical spool blocks the rescan from the entire
	// subtree (in particular, any dynamic scan) underneath it. That means when
	// we have a dynamic scan under a spool, and a corresponding partition
	// selector outside the spool, we run the risk of materializing the wrong
	// results.

	// For example, the following plan is invalid because the partition selector
	// won't be able to influence inner side of the nested loop join as intended
	// ("blocked" by the spool):

	// +--CPhysicalMotionGather(master)
	//    +--CPhysicalInnerNLJoin
	//       |--CPhysicalPartitionSelector
	//       |  +--CPhysicalMotionBroadcast
	//       |     +--CPhysicalTableScan "foo" ("foo")
	//       |--CPhysicalSpool
	//       |  +--CPhysicalLeftOuterHashJoin
	//       |     |--CPhysicalDynamicTableScan "pt" ("pt")
	//       |     |--CPhysicalMotionHashDistribute
	//       |     |  +--CPhysicalTableScan "bar" ("bar")
	//       |     +--CScalarCmp (=)
	//       |        |--CScalarIdent "d" (19)
	//       |        +--CScalarIdent "dk" (9)
	//       +--CScalarCmp (<)
	//          |--CScalarIdent "a" (0)
	//          +--CScalarIdent "partkey" (10)

	CDrvdPropPlan *pdpplanChild = pccBest->Pdpplan();
	if (pdpplanChild->Ppim()->FContainsUnresolved())
	{
		return false;
	}

	// Discard any context that is requesting for rewindability with motion hazard handling and
	// the physical spool is streaming with a motion underneath it.
	// We do not want to add a blocking spool over a spool as spooling twice will be expensive,
	// hence invalidate this context.
	CEnfdRewindability *per = poc->Prpp()->Per();
	if(per->PrsRequired()->HasMotionHazard() &&
	   pdpplanChild->Prs()->HasMotionHazard())
	{
		return FEager();
	}

	return true;
}

IOstream &
CPhysicalSpool::OsPrint
(
	IOstream &os
)
const
{
	os << SzId() << " (";
	if(FEager())
	{
		os << "Blocking)";
	}
	else
	{
		os	<< "Streaming)";
	}

	return os;
}

// EOF

