//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogical.h
//
//	@doc:
//		Base class for all logical operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogical_H
#define GPOPT_CLogical_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/CReqdProp.h"
#include "gpopt/base/CMaxCard.h"
#include "gpopt/base/CPropConstraint.h"
#include "gpopt/base/CPartInfo.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/xforms/CXform.h"

// fwd declarataion
namespace gpnaucrates
{
	class IStatistics;
}

namespace gpopt
{
	using namespace gpos;
	
	// forward declaration
	class CColRefSet;
	class CKeyCollection;

	//---------------------------------------------------------------------------
	//	@class:
	//		CLogical
	//
	//	@doc:
	//		base class for all logical operators
	//
	//---------------------------------------------------------------------------
	class CLogical : public COperator
	{

		public:

			// statistics derivation promise levels;
			// used for prioritizing operators within the same Memo group for statistics derivation
			enum EStatPromise
			{
				EspNone,	// operator must not be used for stat derivation
				EspLow,		// operator has low priority for stat derivation
				EspMedium,	// operator has medium priority for stat derivation
				EspHigh		// operator has high priority for stat derivation
			};

		private:

			// private copy ctor
			CLogical(const CLogical &);
			

		protected:
		
			// set of locally used columns
			CColRefSet *m_pcrsLocalUsed;

			// output column generation given a list of column descriptors
			ColRefArray *PdrgpcrCreateMapping
					(
					IMemoryPool *memory_pool,
					const ColumnDescrArray *pdrgpcoldesc,
					ULONG ulOpSourceId
					)
					const;

			// initialize the array of partition columns
			ColRefArrays *
			PdrgpdrgpcrCreatePartCols(IMemoryPool *memory_pool, ColRefArray *colref_array, const ULongPtrArray *pdrgpulPart);

			// derive dummy statistics
			IStatistics *PstatsDeriveDummy(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CDouble rows) const;

			// helper for common case of output derivation from outer child
			static
			CColRefSet *PcrsDeriveOutputPassThru(CExpressionHandle &exprhdl);

			// helper for common case of not nullable columns derivation from outer child
			static
			CColRefSet *PcrsDeriveNotNullPassThruOuter(CExpressionHandle &exprhdl);

			// helper for common case of output derivation from all logical children
			static
			CColRefSet *PcrsDeriveOutputCombineLogical(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// helper for common case of combining not nullable columns from all logical children
			static
			CColRefSet *PcrsDeriveNotNullCombineLogical(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// helper for common case of stat columns computation
			static
			CColRefSet *PcrsReqdChildStats
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsInput,
				CColRefSet *pcrsUsed,
				ULONG child_index
				);

			// helper for common case of passing through required stat columns
			static
			CColRefSet *PcrsStatsPassThru(CColRefSet *pcrsInput);

			// helper for common case of passing through derived stats
			static
			IStatistics *PstatsPassThruOuter(CExpressionHandle &exprhdl);

			// shorthand to addref and pass through keys from n-th child
			static
			CKeyCollection *PkcDeriveKeysPassThru(CExpressionHandle &exprhdl, ULONG ulInput);

			// shorthand to combine keys from first n - 1 children
			static
			CKeyCollection *PkcCombineKeys(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);
				
			// helper function for computing the keys in a base relation
			static
			CKeyCollection *PkcKeysBaseTable(IMemoryPool *memory_pool, const BitSetArray *pdrgpbsKeys, const ColRefArray *pdrgpcrOutput);
			
			// helper for the common case of passing through partition consumer info
			static
			CPartInfo *PpartinfoPassThruOuter(CExpressionHandle &exprhdl);
			
			// helper for common case of combining partition consumer info from logical children
			static
			CPartInfo *PpartinfoDeriveCombine(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// derive constraint property from a table/index get
			static
			CPropConstraint *PpcDeriveConstraintFromTable(IMemoryPool *memory_pool, const CTableDescriptor *ptabdesc, const ColRefArray *pdrgpcrOutput);

			// derive constraint property from a table/index get with predicates
			static
			CPropConstraint *PpcDeriveConstraintFromTableWithPredicates
							(
							IMemoryPool *memory_pool,
							CExpressionHandle &exprhdl,
							const CTableDescriptor *ptabdesc,
							const ColRefArray *pdrgpcrOutput
							);

			// shorthand to addref and pass through constraint from a given child
			static
			CPropConstraint *PpcDeriveConstraintPassThru(CExpressionHandle &exprhdl, ULONG ulChild);

			// derive constraint property only on the given columns
			static
			CPropConstraint *PpcDeriveConstraintRestrict(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CColRefSet *pcrsOutput);

			// default max card for join and apply operators
			static
			CMaxCard MaxcardDef(CExpressionHandle &exprhdl);

			// compute max card given scalar child and constraint property
			static
			CMaxCard Maxcard(CExpressionHandle &exprhdl, ULONG ulScalarIndex, CMaxCard maxcard);
			
			// compute order spec based on an index
			static
			COrderSpec *PosFromIndex
				(
				IMemoryPool *memory_pool,
				const IMDIndex *pmdindex,
				ColRefArray *colref_array,
				const CTableDescriptor *ptabdesc
				);

			// derive function properties using data access property of scalar child
			static
			CFunctionProp *PfpDeriveFromScalar(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, ULONG ulScalarIndex);

			// derive outer references
			static
			CColRefSet *PcrsDeriveOuter(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CColRefSet *pcrsUsedAdditional);

		public:
		
			// ctor
			explicit
			CLogical(IMemoryPool *memory_pool);
			
			// dtor
			virtual 
			~CLogical();

			// type of operator
			virtual
			BOOL FLogical() const
			{
				GPOS_ASSERT(!FPhysical() && !FScalar() && !FPattern());
				return true;
			}

			// return the locally used columns
			CColRefSet *PcrsLocalUsed() const
			{
				return m_pcrsLocalUsed;
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// create derived properties container
			virtual
			DrvdPropArray *PdpCreate(IMemoryPool *memory_pool) const;

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) = 0;

			// derive outer references
			virtual
			CColRefSet *PcrsDeriveOuter
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
			{
				return PcrsDeriveOuter(memory_pool, exprhdl, NULL /*pcrsUsedAdditional*/);
			}
			
			// derive outer references for index get and dynamic index get operators
			virtual
			CColRefSet *PcrsDeriveOuterIndexGet(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// derive not nullable output columns
			virtual
			CColRefSet *PcrsDeriveNotNull
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & // exprhdl
				)
				const
			{
				// by default, return an empty set
				return GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			}

			// derive columns from the inner child of a correlated-apply expression that can be used above the apply expression
			virtual
			CColRefSet *PcrsDeriveCorrelatedApply(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive key collections
			virtual
			CKeyCollection *PkcDeriveKeys(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive join depth
			virtual
			ULONG JoinDepth(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive partition information
			virtual
			CPartInfo *PpartinfoDerive(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const = 0;

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const = 0;
			
			// derive function properties
			virtual
			CFunctionProp *PfpDerive(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

			// derive statistics
			virtual
			IStatistics *PstatsDerive(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, StatsArray *stats_ctxt) const = 0;

			// promise level for stat derivation
			virtual
			EStatPromise Esp(CExpressionHandle &) const = 0;

			//-------------------------------------------------------------------------------------
			// Required Relational Properties
			//-------------------------------------------------------------------------------------

			// create required properties container
			virtual
			CReqdProp *PrpCreate(IMemoryPool *memory_pool) const;

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CColRefSet *pcrsInput, ULONG child_index) const = 0;

			// compute partition predicate to pass down to n-th child
			virtual
			CExpression *PexprPartPred(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CExpression *pexprInput, ULONG child_index) const;

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const = 0;
			
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// return true if operator can select a subset of input tuples based on some predicate,
			// for example, a Join is a SelectionOp, but a Project is not
			virtual
			BOOL FSelectionOp() const
			{
				return false;
			}

			// return true if we can pull projections up past this operator from its given child
			virtual
			BOOL FCanPullProjectionsUp
				(
				ULONG //child_index
				) const
			{
				return true;
			}

			// helper for deriving statistics on a base table
			static
			IStatistics *PstatsBaseTable
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CTableDescriptor *ptabdesc,
				CColRefSet *pcrsStatExtra = NULL
				);

			// conversion function
			static
			CLogical *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(pop->FLogical());
				
				return reinterpret_cast<CLogical*>(pop);
			}
		
			// returns the table descriptor for (Dynamic)(BitmapTable)Get operators
			static
			CTableDescriptor *PtabdescFromTableGet(COperator *pop);
			
			// extract the output columns descriptor from a logical get or dynamic get operator
			static
			ColRefArray *PdrgpcrOutputFromLogicalGet(CLogical *pop);
			
			// extract the table name from a logical get or dynamic get operator
			static
			const CName &NameFromLogicalGet(CLogical *pop);

			// return the set of distribution columns
			static
			CColRefSet *PcrsDist(IMemoryPool *memory_pool, const CTableDescriptor *ptabdesc, const ColRefArray *colref_array);

			// derive constraint property when expression has relational children and predicates
			static
			CPropConstraint *PpcDeriveConstraintFromPredicates(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);


	}; // class CLogical

}


#endif // !GPOPT_CLogical_H

// EOF
