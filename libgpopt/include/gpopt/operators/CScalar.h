//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CScalar.h
//
//	@doc:
//		Base class for all scalar operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalar_H
#define GPOPT_CScalar_H

#include "gpos/base.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/CPartInfo.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/operators/COperator.h"


#include "naucrates/md/IMDId.h"

namespace gpopt
{
	using namespace gpos;
	using namespace gpmd;

	// forward declaration
	class CColRefSet;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CScalar
	//
	//	@doc:
	//		base class for all scalar operators
	//
	//---------------------------------------------------------------------------
	class CScalar : public COperator
	{
		public:

			// possible results of Boolean evaluation of a scalar expression
			enum EBoolEvalResult
				{
				EberTrue = 1,	// TRUE
				EberFalse,		// FALSE
				EberNull,		// NULL
				EberUnknown,	// Unknown

				EerSentinel
				};

		private:

			// private copy ctor
			CScalar(const CScalar &);
			
			// helper for combining partition consumer arrays of scalar children
			static
			CPartInfo *PpartinfoDeriveCombineScalar(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

		protected:

			// perform conjunction of child boolean evaluation results
			static
			EBoolEvalResult EberConjunction(ULongPtrArray *pdrgpulChildren);

			// perform disjunction of child boolean evaluation results
			static
			EBoolEvalResult EberDisjunction(ULongPtrArray *pdrgpulChildren);

			// return Null if any child is Null
			static
			EBoolEvalResult EberNullOnAnyNullChild(ULongPtrArray *pdrgpulChildren);

			// return Null if all children are Null
			static
			EBoolEvalResult EberNullOnAllNullChildren(ULongPtrArray *pdrgpulChildren);

		public:
		
			// ctor
			explicit
			CScalar
				(
				IMemoryPool *memory_pool
				)
				: 
				COperator(memory_pool)
			{}

			// dtor
			virtual 
			~CScalar() {}

			// type of operator
			virtual
			BOOL FScalar() const
			{
				GPOS_ASSERT(!FPhysical() && !FLogical() && !FPattern());
				return true;
			}

			// create derived properties container
			virtual
			CDrvdProp *PdpCreate(IMemoryPool *memory_pool) const;

			// create required properties container
			virtual
			CReqdProp *PrpCreate(IMemoryPool *memory_pool) const;

			// return locally defined columns
			virtual
			CColRefSet *PcrsDefined
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & // exprhdl
				)
			{
				// return an empty set of column refs
				return GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			}

			// return columns containing set-returning function
			virtual
			CColRefSet *PcrsSetReturningFunction
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & // exprhdl
				)
			{
				// return an empty set of column refs
				return GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			}

			// return locally used columns
			virtual
			CColRefSet *PcrsUsed
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & // exprhdl
				)
			{
				// return an empty set of column refs
				return GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			}

			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *memory_pool, 
				CExpressionHandle &exprhdl
				) 
				const
			{
				return PpartinfoDeriveCombineScalar(memory_pool, exprhdl);
			}

			// derive function properties
			virtual
			CFunctionProp *PfpDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PfpDeriveFromChildren
							(
							memory_pool,
							exprhdl,
							IMDFunction::EfsImmutable, // efsDefault
							IMDFunction::EfdaNoSQL, // efdaDefault
							false, // fHasVolatileFunctionScan
							false // fScan
							);
			}

			// derive subquery existence
			virtual
			BOOL FHasSubquery(CExpressionHandle &exprhdl);

			// derive non-scalar function existence
			virtual
			BOOL FHasNonScalarFunction(CExpressionHandle &exprhdl);

			virtual
			BOOL FHasScalarArrayCmp(CExpressionHandle &exprhdl);

			// boolean expression evaluation
			virtual
			EBoolEvalResult Eber
				(
				ULongPtrArray * // pdrgpulChildren
				)
				const
			{
				// by default, evaluation result is unknown
				return EberUnknown;
			}

			// perform boolean evaluation of the given expression tree
			static
			EBoolEvalResult EberEvaluate(IMemoryPool *memory_pool, CExpression *pexprScalar);

			// conversion function
			static
			CScalar *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(pop->FScalar());
				
				return reinterpret_cast<CScalar*>(pop);
			}
		
			// the type of the scalar expression
			virtual 
			IMDId *MDIdType() const = 0;

			// the type modifier of the scalar expression
			virtual
			INT TypeModifier() const
			{
				return default_type_modifier;
			}
			
	}; // class CScalar

}


#endif // !GPOPT_CScalar_H

// EOF
