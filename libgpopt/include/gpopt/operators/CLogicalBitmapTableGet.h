//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CLogicalBitmapTableGet.h
//
//	@doc:
//		Logical operator for table access via bitmap indexes.
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CLogicalBitmapTableGet_H
#define GPOPT_CLogicalBitmapTableGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
	// fwd declarations
	class CColRefSet;
	class CTableDescriptor;

	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalBitmapTableGet
	//
	//	@doc:
	//		Logical operator for table access via bitmap indexes.
	//
	//---------------------------------------------------------------------------
	class CLogicalBitmapTableGet : public CLogical
	{
		private:
			// table descriptor
			CTableDescriptor *m_ptabdesc;

			// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
			ULONG m_ulOriginOpId;

			// alias for table
			const CName *m_pnameTableAlias;

			// output columns
			DrgPcr *m_pdrgpcrOutput;

			// private copy ctor
			CLogicalBitmapTableGet(const CLogicalBitmapTableGet &);

		public:
			// ctor
			CLogicalBitmapTableGet
				(
				IMemoryPool *memory_pool,
				CTableDescriptor *ptabdesc,
				ULONG ulOriginOpId,
				const CName *pnameTableAlias,
				DrgPcr *pdrgpcrOutput
				);

			// ctor
			// only for transformations
			explicit
			CLogicalBitmapTableGet(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CLogicalBitmapTableGet();

			// table descriptor
			CTableDescriptor *Ptabdesc() const
			{
				return m_ptabdesc;
			}

			// table alias
			const CName *PnameTableAlias()
			{
				return m_pnameTableAlias;
			}

			// array of output column references
			DrgPcr *PdrgpcrOutput() const
			{
				return m_pdrgpcrOutput;
			}

			// identifier
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalBitmapTableGet;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalBitmapTableGet";
			}

			// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
			ULONG UlOriginOpId() const
			{
				return m_ulOriginOpId;
			}

			// operator specific hash function
			virtual
			ULONG HashValue() const;

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const
			{
				return true;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// derive outer references
			virtual
			CColRefSet *PcrsDeriveOuter(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & //exprhdl
				)
				const
			{
				return GPOS_NEW(memory_pool) CPartInfo(memory_pool);
			}

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive join depth
			virtual
			ULONG JoinDepth
				(
				IMemoryPool *, // memory_pool
				CExpressionHandle & // exprhdl
				)
				const
			{
				return 1;
			}

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &, // exprhdl
				CColRefSet *, //pcrsInput
				ULONG // child_index
				)
				const
			{
				return GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			}

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// derive statistics
			virtual
			IStatistics *PstatsDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				StatsArray *stats_ctxt
				)
				const;

			// stat promise
			virtual
			EStatPromise Esp(CExpressionHandle &) const
			{
				return CLogical::EspHigh;
			}

			// debug print
			virtual
			IOstream &OsPrint(IOstream &) const;

			// conversion
			static
			CLogicalBitmapTableGet *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalBitmapTableGet == pop->Eopid());

				return dynamic_cast<CLogicalBitmapTableGet *>(pop);
			}

	};  // class CLogicalBitmapTableGet
}

#endif // !GPOPT_CLogicalBitmapTableGet_H

// EOF
