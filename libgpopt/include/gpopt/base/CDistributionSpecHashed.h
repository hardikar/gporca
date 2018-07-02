//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecHashed.h
//
//	@doc:
//		Description of a hashed distribution; 
//		Can be used as required or derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecHashed_H
#define GPOPT_CDistributionSpecHashed_H

#include "gpos/base.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDistributionSpecRandom.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDistributionSpecHashed
	//
	//	@doc:
	//		Class for representing hashed distribution specification.
	//
	//---------------------------------------------------------------------------
	class CDistributionSpecHashed : public CDistributionSpecRandom
	{
		private:

			// array of distribution expressions
			ExpressionArray *m_pdrgpexpr;
			
			// are NULLS consistently distributed
			BOOL m_fNullsColocated;

			// equivalent hashed distribution introduced by a hash join
			CDistributionSpecHashed *m_pdshashedEquiv;

			// check if specs are compatible wrt to co-location of nulls;
			// HD1 satisfies HD2 if:
			//	* HD1 colocates NULLs or
			//  * HD2 doesn't care about NULLs
			BOOL FNullsColocated(const CDistributionSpecHashed *pds) const
			{
				return (m_fNullsColocated || !pds->m_fNullsColocated);
			}
			
			// check if specs are compatible wrt to duplicate sensitivity
			// HD1 satisfies HD2 if:
			//	* HD1 is duplicate sensitive, or
			//  * HD2 doesn't care about duplicates
			BOOL FDuplicateSensitiveCompatible
				(
				const CDistributionSpecHashed *pds
				)
				const
			{
				return (m_is_duplicate_sensitive || !pds->m_is_duplicate_sensitive);
			}

			// exact match against given hashed distribution
			BOOL FMatchHashedDistribution(const CDistributionSpecHashed *pdshashed) const;

			// private copy ctor
			CDistributionSpecHashed(const CDistributionSpecHashed &);
			
		public:
		
			// ctor
			CDistributionSpecHashed(ExpressionArray *pdrgpexpr, BOOL fNullsColocated);
			
			// ctor
			CDistributionSpecHashed(ExpressionArray *pdrgpexpr, BOOL fNullsColocated, CDistributionSpecHashed *pdshashedEquiv);

			// dtor
			virtual 
			~CDistributionSpecHashed();
			
			// distribution type accessor
			virtual 
			EDistributionType Edt() const
			{
				return CDistributionSpec::EdtHashed;
			}

			virtual
			const CHAR *SzId() const
			{
				return "HASHED";
			}

			// expression array accessor
			ExpressionArray *Pdrgpexpr() const
			{
				return m_pdrgpexpr;
			}
			
			// is distribution nulls colocated
			BOOL FNullsColocated() const
			{
				return m_fNullsColocated;
			}

			// equivalent hashed distribution
			CDistributionSpecHashed *PdshashedEquiv() const
			{
				return m_pdshashedEquiv;
			}

			// columns used by distribution expressions
			virtual
			CColRefSet *PcrsUsed(IMemoryPool *memory_pool) const;

			// return a copy of the distribution spec after excluding the given columns
			virtual
			CDistributionSpecHashed *PdshashedExcludeColumns(IMemoryPool *memory_pool, CColRefSet *pcrs);

			// does this distribution match the given one
			virtual 
			BOOL Matches(const CDistributionSpec *pds) const;

			// does this distribution satisfy the given one
			virtual 
			BOOL FSatisfies(const CDistributionSpec *pds) const;
			
			// check if the columns of passed spec match a subset of the
			// object's columns
			BOOL FMatchSubset(const CDistributionSpecHashed *pds) const;

			// equality function
			BOOL Equals(const CDistributionSpecHashed *pds) const;

			// return a copy of the distribution spec with remapped columns
			virtual
			CDistributionSpec *PdsCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			// append enforcers to dynamic array for the given plan properties
			virtual
			void AppendEnforcers(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CReqdPropPlan *prpp, ExpressionArray *pdrgpexpr, CExpression *pexpr);

			// hash function for hashed distribution spec
			virtual
			ULONG HashValue() const;

			// return distribution partitioning type
			virtual
			EDistributionPartitioningType Edpt() const
			{
				return EdptPartitioned;
			}

			// print
			virtual
			IOstream &OsPrint(IOstream &os) const;

			// return a hashed distribution on the maximal hashable subset of given columns
			static
			CDistributionSpecHashed *PdshashedMaximal
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				BOOL fNullsColocated
				);

			// conversion function
			static
			CDistributionSpecHashed *PdsConvert
				(
				CDistributionSpec *pds
				)
			{
				GPOS_ASSERT(NULL != pds);
				CDistributionSpecHashed *pdsHashed = dynamic_cast<CDistributionSpecHashed*>(pds);
				GPOS_ASSERT(NULL != pdsHashed);

				return pdsHashed;
			}

			// conversion function: const argument
			static
			const CDistributionSpecHashed *PdsConvert
				(
				const CDistributionSpec *pds
				)
			{
				GPOS_ASSERT(NULL != pds);
				const CDistributionSpecHashed *pdsHashed = dynamic_cast<const CDistributionSpecHashed*>(pds);
				GPOS_ASSERT(NULL != pdsHashed);

				return pdsHashed;
			}

	}; // class CDistributionSpecHashed

}

#endif // !GPOPT_CDistributionSpecHashed_H

// EOF
