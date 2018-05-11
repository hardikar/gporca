//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalMotionBroadcast.h
//
//	@doc:
//		Physical Broadcast motion operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalMotionBroadcast_H
#define GPOPT_CPhysicalMotionBroadcast_H

#include "gpos/base.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/operators/CPhysicalMotion.h"

namespace gpopt
{
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalMotionBroadcast
	//
	//	@doc:
	//		Broadcast motion operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalMotionBroadcast : public CPhysicalMotion
	{
		private:

			// output distribution
			CDistributionSpecReplicated *m_pdsReplicated;

			// private copy ctor
			CPhysicalMotionBroadcast(const CPhysicalMotionBroadcast &);

		public:
		
			// ctor
			explicit
			CPhysicalMotionBroadcast(IMemoryPool *memory_pool);
			
			// dtor
			virtual 
			~CPhysicalMotionBroadcast();

			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopPhysicalMotionBroadcast;
			}
			
			virtual 
			const CHAR *SzId() const
			{
				return "CPhysicalMotionBroadcast";
			}
			
			// output distribution accessor
			virtual
			CDistributionSpec *Pds() const
			{
				return m_pdsReplicated;
			}

			// match function
			virtual
			BOOL Matches(COperator *) const;

			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required output columns of the n-th child
			virtual
			CColRefSet *PcrsRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsInput,
				ULONG child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				);

			// compute required sort order of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				COrderSpec *posInput,
				ULONG child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// check if required columns are included in output columns
			virtual
			BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired, ULONG ulOptReq) const;


			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive sort order
			virtual
			COrderSpec *PosDerive(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				) const;

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// print
			virtual 
			IOstream &OsPrint(IOstream &) const;
			
			// conversion function
			static
			CPhysicalMotionBroadcast *PopConvert(COperator *pop);			
					
	}; // class CPhysicalMotionBroadcast

}

#endif // !GPOPT_CPhysicalMotionBroadcast_H

// EOF
