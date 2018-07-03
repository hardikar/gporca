//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CPhysicalScan.h
//
//	@doc:
//		Base class for physical scan operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalScan_H
#define GPOPT_CPhysicalScan_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysical.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/base/CCTEMap.h"
#include "gpopt/base/CDistributionSpecHashed.h"

namespace gpopt
{
	
	// fwd declarations
	class CTableDescriptor;
	class CName;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalScan
	//
	//	@doc:
	//		Base class for physical scan operators
	//
	//---------------------------------------------------------------------------
	class CPhysicalScan : public CPhysical
	{

		protected:

			// alias for table
			const CName *m_pnameAlias;

			// table descriptor
			CTableDescriptor *m_ptabdesc;
			
			// output columns
			ColRefArray *m_pdrgpcrOutput;
			
			// distribution
			CDistributionSpec *m_pds;
			
			// stats of base table -- used for costing
			// if operator is index scan, this is the stats of table on which index is created
			IStatistics *m_pstatsBaseTable;

			// derive part index map from a dynamic scan operator
			static
			CPartIndexMap *PpimDeriveFromDynamicScan
				(
				IMemoryPool *mp,
				ULONG part_idx_id,
				IMDId *rel_mdid,
				ColRefArrays *pdrgpdrgpcrPart,
				ULONG ulSecondaryPartIndexId,
				CPartConstraint *ppartcnstr,
				CPartConstraint *ppartcnstrRel,
				ULONG ulExpectedPropagators
				);

		private:
			
			// compute stats of underlying table
			void ComputeTableStats(IMemoryPool *mp);

			// derive hashed distribution when index conditions have outer references
			CDistributionSpecHashed *PdshashedDeriveWithOuterRefs
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl
				) const;

			// search the given array of predicates for an equality predicate that has
			// one side equal to given expression
			static
			CExpression *PexprMatchEqualitySide
				(
				CExpression *pexprToMatch,
				ExpressionArray *pdrgpexpr // array of predicates to inspect
				);

			// private copy ctor
			CPhysicalScan(const CPhysicalScan&);

		public:
		
			// ctors
			CPhysicalScan(IMemoryPool *mp, const CName *pname, CTableDescriptor *, ColRefArray *colref_array);

			// dtor
			virtual 
			~CPhysicalScan();

			// return table descriptor
			virtual
			CTableDescriptor *Ptabdesc() const
			{
				return m_ptabdesc;
			}
		
			// output columns
			virtual
			ColRefArray *PdrgpcrOutput() const
			{
				return m_pdrgpcrOutput;
			}
					
			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const;

			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required output columns of the n-th child
			virtual
			CColRefSet *PcrsRequired
				(
				IMemoryPool *, // mp
				CExpressionHandle &, // exprhdl
				CColRefSet *, // pcrsRequired
				ULONG, // child_index
				DrgPdp *, // pdrgpdpCtxt
				ULONG // ulOptReq
				)
			{
				GPOS_ASSERT(!"CPhysicalScan has no children");
				return NULL;
			}

			// compute required ctes of the n-th child
			virtual
			CCTEReq *PcteRequired
				(
				IMemoryPool *, //mp,
				CExpressionHandle &, //exprhdl,
				CCTEReq *, //pcter,
				ULONG , //child_index,
				DrgPdp *, //pdrgpdpCtxt,
				ULONG //ulOptReq
				)
				const
			{
				GPOS_ASSERT(!"CPhysicalScan has no children");
				return NULL;
			}

			// compute required sort columns of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *, // mp
				CExpressionHandle &, // exprhdl
				COrderSpec *, // posRequired
				ULONG, // child_index
				DrgPdp *, // pdrgpdpCtxt
				ULONG // ulOptReq
				)
				const
			{
				GPOS_ASSERT(!"CPhysicalScan has no children");
				return NULL;
			}

			// compute required distribution of the n-th child
			virtual
			CDistributionSpec *PdsRequired
				(
				IMemoryPool *, // mp
				CExpressionHandle &, // exprhdl
				CDistributionSpec *, // pdsRequired
				ULONG, // child_index
				DrgPdp *, // pdrgpdpCtxt
				ULONG // ulOptReq
				)
				const
			{
				GPOS_ASSERT(!"CPhysicalScan has no children");
				return NULL;
			}

			// compute required rewindability of the n-th child
			virtual
			CRewindabilitySpec *PrsRequired
				(
				IMemoryPool *, //mp
				CExpressionHandle &, //exprhdl
				CRewindabilitySpec *, //prsRequired
				ULONG, // child_index
				DrgPdp *, // pdrgpdpCtxt
				ULONG // ulOptReq
				)
				const
			{
				GPOS_ASSERT(!"CPhysicalScan has no children");
				return NULL;
			}

			
			// compute required partition propagation of the n-th child
			virtual
			CPartitionPropagationSpec *PppsRequired
				(
				IMemoryPool *, //mp,
				CExpressionHandle &, //exprhdl,
				CPartitionPropagationSpec *, //pppsRequired,
				ULONG , //child_index,
				DrgPdp *, //pdrgpdpCtxt,
				ULONG // ulOptReq
				)
			{
				GPOS_ASSERT(!"CPhysicalScan has no children");
				return NULL;
			}
			
			// check if required columns are included in output columns
			virtual
			BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired, ULONG ulOptReq) const;

			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive sort order
			virtual
			COrderSpec *PosDerive
				(
				IMemoryPool *mp,
				CExpressionHandle & // exprhdl
				)
				const
			{
				// return empty sort order
				return GPOS_NEW(mp) COrderSpec(mp);
			}

			// derive distribution
			virtual
			CDistributionSpec *PdsDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive cte map
			virtual
			CCTEMap *PcmDerive
				(
				IMemoryPool *mp,
				CExpressionHandle & //exprhdl
				)
				const
			{
				return GPOS_NEW(mp) CCTEMap(mp);
			}

			// derive rewindability
			virtual
			CRewindabilitySpec *PrsDerive
				(
				IMemoryPool *mp,
				CExpressionHandle & // exprhdl
				)
				const
			{
				// rewindability of output is always true
				return GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtGeneral /*ert*/);
			}

			// derive partition filter map
			virtual
			CPartFilterMap *PpfmDerive
				(
				IMemoryPool *mp,
				CExpressionHandle & // exprhdl
				)
				const
			{
				// return empty part filter map
				return GPOS_NEW(mp) CPartFilterMap(mp);
			}

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				)
				const;

			
			// return distribution property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetDistribution
				(
				CExpressionHandle &exprhdl,
				const CEnfdDistribution *ped
				)
				const;

			// return rewindability property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetRewindability
				(
				CExpressionHandle &, // exprhdl
				const CEnfdRewindability * // per
				)
				const
			{
				// no need for enforcing rewindability on output
				return CEnfdProp::EpetUnnecessary;
			}

			// return partition propagation property enforcing type for this operator
			virtual 
			CEnfdProp::EPropEnforcingType EpetPartitionPropagation
				(
				CExpressionHandle &, // exprhdl,
				const CEnfdPartitionPropagation *pepp
				) 
				const
			{
				if (!pepp->PppsRequired()->Ppim()->FContainsUnresolvedZeroPropagators())
				{
					return CEnfdProp::EpetUnnecessary;
				}
				return CEnfdProp::EpetRequired;
			}

			// return true if operator passes through stats obtained from children,
			// this is used when computing stats during costing
			virtual
			BOOL FPassThruStats() const
			{
				return false;
			}

			// return true if operator is dynamic scan
			virtual
			BOOL FDynamicScan() const
			{
				return false;
			}

			// stats of underlying table
			IStatistics *PstatsBaseTable() const
			{
			 	return m_pstatsBaseTable;
			}

			// statistics derivation during costing
			virtual
			IStatistics *PstatsDerive(IMemoryPool *mp, CExpressionHandle &exprhdl, CReqdPropPlan *prpplan, StatsArray *stats_ctxt) const = 0;
			
			// conversion function
			static
			CPhysicalScan *PopConvert(COperator *pop);

	}; // class CPhysicalScan

}

#endif // !GPOPT_CPhysicalScan_H

// EOF
