//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
#ifndef GPOPT_CXformImplementFullOuterJoin_H
#define GPOPT_CXformImplementFullOuterJoin_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
	using namespace gpos;

	class CXformImplementFullOuterJoin : public CXformExploration
	{

		private:

			// private copy ctor
			CXformImplementFullOuterJoin(const CXformImplementFullOuterJoin &);

		public:

			// ctor
			explicit
			CXformImplementFullOuterJoin(IMemoryPool *mp);

			// dtor
			virtual
			~CXformImplementFullOuterJoin() {}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfImplementFullOuterJoin;
			}

			// return a string for xform name
			virtual
			const CHAR *SzId() const
			{
				return "CXformImplementFullOuterJoin";
			}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp(CExpressionHandle &exprhdl) const;

			// actual transform
			virtual
			void Transform
				(
				CXformContext *pxfctxt,
				CXformResult *pxfres,
				CExpression *pexpr
				)
				const;

	}; // class CXformImplementFullOuterJoin
}

#endif // !GPOPT_CXformImplementFullOuterJoin_H

// EOF
