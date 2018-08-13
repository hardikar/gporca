//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSearchStrategyTest.cpp
//
//	@doc:
//		Test for search strategy
//---------------------------------------------------------------------------
#include "gpopt/exception.h"

#include "gpopt/engine/CEngine.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/search/CSearchStage.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpopt/xforms/CXformFactory.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/engine/CEngineTest.h"
#include "unittest/gpopt/search/CSchedulerTest.h"
#include "unittest/gpopt/search/CSearchStrategyTest.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/parser/CParseHandlerDXL.h"

//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest
//
//	@doc:
//		Unittest for scheduler
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSearchStrategyTest::EresUnittest()
{
	CUnittest rgut[] =
		{
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CSearchStrategyTest::EresUnittest_RecursiveOptimize),
#endif // GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CSearchStrategyTest::EresUnittest_MultiThreadedOptimize),
		GPOS_UNITTEST_FUNC(CSearchStrategyTest::EresUnittest_Parsing),
		GPOS_UNITTEST_FUNC_THROW
			(
			CSearchStrategyTest::EresUnittest_Timeout,
			gpopt::ExmaGPOPT,
			gpopt::ExmiNoPlanFound
			),
		GPOS_UNITTEST_FUNC_THROW
			(
			CSearchStrategyTest::EresUnittest_ParsingWithException,
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLXercesParseError
			),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::Optimize
//
//	@doc:
//		Run optimize function on given expression
//
//---------------------------------------------------------------------------
void
CSearchStrategyTest::Optimize
	(
	IMemoryPool *mp,
	Pfpexpr pfnGenerator,
	SearchStageArray *search_stage_array,
	PfnOptimize pfnOptimize
	)
{
	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	{
		CAutoOptCtxt aoc
						(
						mp,
						&mda,
						NULL,  /* pceeval */
						CTestUtils::GetCostModel(mp)
						);
		CExpression *pexpr = pfnGenerator(mp);
		pfnOptimize(mp, pexpr, search_stage_array);
		pexpr->Release();
	}
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest_RecursiveOptimize
//
//	@doc:
//		Test search strategy with recursive optimization
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSearchStrategyTest::EresUnittest_RecursiveOptimize()
{
	CAutoMemoryPool amp;
	IMemoryPool *mp = amp.Pmp();
	Optimize(mp, CTestUtils::PexprLogicalSelectOnOuterJoin, PdrgpssRandom(mp), CEngineTest::BuildMemoRecursive);

	return GPOS_OK;
}
#endif // GPOS_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest_MultiThreadedOptimize
//
//	@doc:
//		Test search strategy with multi-threaded optimization
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSearchStrategyTest::EresUnittest_MultiThreadedOptimize()
{
	CAutoMemoryPool amp;
	IMemoryPool *mp = amp.Pmp();
	Optimize(mp, CTestUtils::PexprLogicalSelectOnOuterJoin, PdrgpssRandom(mp), CSchedulerTest::BuildMemoMultiThreaded);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest_Parsing
//
//	@doc:
//		Test search strategy with multi-threaded optimization
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSearchStrategyTest::EresUnittest_Parsing()
{
	CAutoMemoryPool amp;
	IMemoryPool *mp = amp.Pmp();
	CParseHandlerDXL *pphDXL = CDXLUtils::GetParseHandlerForDXLFile(mp,"../data/dxl/search/strategy0.xml", NULL);
	SearchStageArray *search_stage_array = pphDXL->GetSearchStageArray();
	const ULONG size = search_stage_array->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CAutoTrace at(mp);
		(*search_stage_array)[ul]->OsPrint(at.Os());
	}
	search_stage_array->AddRef();
	Optimize(mp, CTestUtils::PexprLogicalSelectOnOuterJoin, search_stage_array, CSchedulerTest::BuildMemoMultiThreaded);

	GPOS_DELETE(pphDXL);

	return GPOS_OK;
}



//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest_Timeout
//
//	@doc:
//		Test search strategy that times out
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSearchStrategyTest::EresUnittest_Timeout()
{
	CAutoMemoryPool amp;
	IMemoryPool *mp = amp.Pmp();
	CAutoTraceFlag atf(EopttracePrintOptimizationStatistics, true);
	CParseHandlerDXL *pphDXL = CDXLUtils::GetParseHandlerForDXLFile(mp,"../data/dxl/search/timeout-strategy.xml", NULL);
	SearchStageArray *search_stage_array = pphDXL->GetSearchStageArray();
	search_stage_array->AddRef();
	Optimize(mp, CTestUtils::PexprLogicalNAryJoin, search_stage_array, CSchedulerTest::BuildMemoMultiThreaded);

	GPOS_DELETE(pphDXL);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest_ParsingWithException
//
//	@doc:
//		Test exception handling when search strategy
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSearchStrategyTest::EresUnittest_ParsingWithException()
{

	CAutoMemoryPool amp;
	IMemoryPool *mp = amp.Pmp();
	CParseHandlerDXL *pphDXL = CDXLUtils::GetParseHandlerForDXLFile(mp,"../data/dxl/search/wrong-strategy.xml", NULL);
	GPOS_DELETE(pphDXL);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::PdrgpssRandom
//
//	@doc:
//		Generate a search strategy with random xform allocation
//
//---------------------------------------------------------------------------
SearchStageArray *
CSearchStrategyTest::PdrgpssRandom
	(
	IMemoryPool *mp
	)
{
	SearchStageArray *search_stage_array = GPOS_NEW(mp) SearchStageArray(mp);
	CXformSet *pxfsFst = GPOS_NEW(mp) CXformSet(mp);
	CXformSet *pxfsSnd = GPOS_NEW(mp) CXformSet(mp);

	// first xforms set contains essential rules to produce simple equality join plan
	(void) pxfsFst->ExchangeSet(CXform::ExfGet2TableScan);
	(void) pxfsFst->ExchangeSet(CXform::ExfSelect2Filter);
	(void) pxfsFst->ExchangeSet(CXform::ExfInnerJoin2HashJoin);

	// second xforms set contains all other rules
	pxfsSnd->Union(CXformFactory::Pxff()->PxfsExploration());
	pxfsSnd->Union(CXformFactory::Pxff()->PxfsImplementation());
	pxfsSnd->Difference(pxfsFst);

	search_stage_array->Append(GPOS_NEW(mp) CSearchStage(pxfsFst, 1000 /*ulTimeThreshold*/, CCost(10E4) /*costThreshold*/));
	search_stage_array->Append(GPOS_NEW(mp) CSearchStage(pxfsSnd, 10000 /*ulTimeThreshold*/, CCost(10E8) /*costThreshold*/));

	return search_stage_array;
}


// EOF
