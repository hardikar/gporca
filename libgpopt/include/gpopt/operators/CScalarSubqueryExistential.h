//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryExistential.h
//
//	@doc:
//		Parent class for existential subquery operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSubqueryExistential_H
#define GPOPT_CScalarSubqueryExistential_H

#include "gpos/base.h"

#include "gpopt/operators/CScalar.h"

namespace gpopt
{

	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CScalarSubqueryExistential
	//
	//	@doc:
	//		Parent class for EXISTS/NOT EXISTS subquery operators
	//
	//---------------------------------------------------------------------------
	class CScalarSubqueryExistential : public CScalar
	{
		private:

			// private copy ctor
			CScalarSubqueryExistential(const CScalarSubqueryExistential &);

		public:

			// ctor
			CScalarSubqueryExistential(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CScalarSubqueryExistential();

			// return the type of the scalar expression
			virtual 
			IMDId *MDIdType() const;

			// match function
			BOOL Matches(COperator *pop) const;

			// sensitivity to order of inputs
			BOOL FInputOrderSensitive() const
			{
				return true;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns
						(
						IMemoryPool *, //memory_pool,
						UlongColRefHashMap *, //colref_mapping,
						BOOL //must_exist
						)
			{
				return PopCopyDefault();
			}
			
			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *memory_pool, 
				CExpressionHandle &exprhdl
				) 
				const;

			// conversion function
			static
			CScalarSubqueryExistential *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopScalarSubqueryExists == pop->Eopid() || EopScalarSubqueryNotExists == pop->Eopid());

				return dynamic_cast<CScalarSubqueryExistential*>(pop);
			}

	}; // class CScalarSubqueryExistential
}

#endif // !GPOPT_CScalarSubqueryExistential_H

// EOF
