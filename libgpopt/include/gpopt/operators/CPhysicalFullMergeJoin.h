//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.

#ifndef GPOPT_CPhysicalFullMergeJoin_H
#define GPOPT_CPhysicalFullMergeJoin_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalJoin.h"

namespace gpopt
{

	class CPhysicalFullMergeJoin : public CPhysicalJoin
	{

		private:

			// private copy ctor
			CPhysicalFullMergeJoin(const CPhysicalFullMergeJoin &);

			CExpressionArray *m_outer_merge_clauses;

			CExpressionArray *m_inner_merge_clauses;

		public:

			// ctor
			explicit
			CPhysicalFullMergeJoin(IMemoryPool *mp,
								   CExpressionArray *outer_merge_clauses,
								   CExpressionArray *inner_merge_clauses);

			// dtor
			virtual
			~CPhysicalFullMergeJoin();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopPhysicalFullMergeJoin;
			}

			 // return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CPhysicalFullMergeJoin";
			}

			// conversion function
			static
			CPhysicalFullMergeJoin *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(EopPhysicalFullMergeJoin == pop->Eopid());

				return dynamic_cast<CPhysicalFullMergeJoin*>(pop);
			}

			virtual
			CDistributionSpec *PdsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired,
				ULONG child_index,
				CDrvdProp2dArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				COrderSpec *posInput,
				ULONG child_index,
				CDrvdProp2dArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// compute required rewindability of the n-th child
			virtual
			CRewindabilitySpec *PrsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired,
				ULONG child_index,
				CDrvdProp2dArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				) const;

			virtual
			CEnfdDistribution::EDistributionMatching Edm
				(
				CReqdPropPlan *, // prppInput
				ULONG , //child_index,
				CDrvdProp2dArray *, // pdrgpdpCtxt,
				ULONG // ulOptReq
				);

			virtual
			CDistributionSpec *PdsDerive
				(
					IMemoryPool *mp,
					CExpressionHandle &exprhdl
				) const;

	}; // class CPhysicalFullMergeJoin

}

#endif // !GPOPT_CPhysicalFullMergeJoin_H

// EOF
