//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CDynamicArrayTest.cpp
//
//	@doc:
//		Test for CDynamicPtrArray
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CDynamicArray.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CDynamicArrayTest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDynamicArrayTest::EresUnittest
//
//	@doc:
//		Unittest for ref-counting
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicArrayTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CDynamicArrayTest::EresUnittest_Basic),
//		GPOS_UNITTEST_FUNC(CDynamicArrayTest::EresUnittest_Ownership),
//		GPOS_UNITTEST_FUNC(CDynamicArrayTest::EresUnittest_ArrayAppend),
//		GPOS_UNITTEST_FUNC(CDynamicArrayTest::EresUnittest_ArrayAppendExactFit),
//		GPOS_UNITTEST_FUNC(CDynamicArrayTest::EresUnittest_PdrgpulSubsequenceIndexes),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CDynamicArrayTest::EresUnittest_Basic
//
//	@doc:
//		Basic array allocation test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicArrayTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *pmp = amp.Pmp();

	// test with CHAR array

	CHAR rgsz[][9] = {"abc", "def", "ghi", "qwe", "wer", "wert", "dfg", "xcv", "zxc"};
	CHAR szMissingElem[] = "missing";
	
	CDynamicArray<CHAR*> *pdrg =
		GPOS_NEW(pmp) CDynamicArray<CHAR*> (pmp, 2);

	// add elements incl trigger resize of array
	for (ULONG i = 0; i < 9; i++)
	{
		pdrg->Append(rgsz[i]);
		GPOS_ASSERT(i + 1 == pdrg->UlLength());
		GPOS_ASSERT(rgsz[i] == (*pdrg)[i]);
	}

	// lookup tests
#ifdef GPOS_DEBUG
	const CHAR *szElem = 
#endif // GPOS_DEBUG
	pdrg->PtLookup(rgsz[0]);
	GPOS_ASSERT(NULL != szElem);
	
#ifdef GPOS_DEBUG
	ULONG ulPos = 
#endif // GPOS_DEBUG
	pdrg->UlPos(rgsz[0]);
	GPOS_ASSERT(0 == ulPos);
	
#ifdef GPOS_DEBUG
	ULONG ulPosMissing = 
#endif // GPOS_DEBUG
	pdrg->UlPos(szMissingElem);
	GPOS_ASSERT(ULONG_MAX == ulPosMissing);
	
	// all elements were inserted in ascending order
	GPOS_ASSERT(pdrg->FSorted());

	pdrg->Release();


	// test with ULONG array

	typedef CDynamicArray<ULONG> DrgULONG;
	DrgULONG *pdrgULONG = GPOS_NEW(pmp) DrgULONG(pmp, 1);
	ULONG c = 256;

	// add elements incl trigger resize of array
	for (ULONG_PTR ulpK = c; ulpK > 0; ulpK--)
	{
		pdrgULONG->Append(ulpK - 1);
	}

	GPOS_ASSERT(c == pdrgULONG->UlLength());

	// all elements were inserted in descending order
	GPOS_ASSERT(!pdrgULONG->FSorted());

	pdrgULONG->Sort();
	GPOS_ASSERT(pdrgULONG->FSorted());

	// test that all positions got copied and sorted properly
	for (ULONG_PTR ulpJ = 0; ulpJ < c; ulpJ++)
	{
		GPOS_ASSERT(ulpJ == (*pdrgULONG)[ulpJ]);
	}
	pdrgULONG->Release();


	return GPOS_OK;
}

//
////---------------------------------------------------------------------------
////	@function:
////		CDynamicArrayTest::EresUnittest_Ownership
////
////	@doc:
////		Basic array test with ownership
////
////---------------------------------------------------------------------------
//GPOS_RESULT
//CDynamicArrayTest::EresUnittest_Ownership()
//{
//	// create memory pool
//	CAutoMemoryPool amp;
//	IMemoryPool *pmp = amp.Pmp();
//
//	// test with ULONGs
//
//	typedef CDynamicPtrArray<ULONG, CleanupDelete<ULONG> > DrgULONG;
//	DrgULONG *pdrgULONG = GPOS_NEW(pmp) DrgULONG(pmp, 1);
//
//	// add elements incl trigger resize of array
//	for (ULONG k = 0; k < 256; k++)
//	{
//		ULONG *pul = GPOS_NEW(pmp) ULONG;
//		pdrgULONG->Append(pul);
//		GPOS_ASSERT(k + 1 == pdrgULONG->UlLength());
//		GPOS_ASSERT(pul == (*pdrgULONG)[k]);
//	}
//	pdrgULONG->Release();
//
//	// test with CHAR array
//
//	typedef CDynamicPtrArray<CHAR, CleanupDeleteRg<CHAR> > DrgCHAR;
//	DrgCHAR *pdrgCHAR = GPOS_NEW(pmp) DrgCHAR(pmp, 2);
//
//	// add elements incl trigger resize of array
//	for (ULONG i = 0; i < 3; i++)
//	{
//		CHAR *sz = GPOS_NEW_ARRAY(pmp, CHAR, 5);
//		pdrgCHAR->Append(sz);
//		GPOS_ASSERT(i + 1 == pdrgCHAR->UlLength());
//		GPOS_ASSERT(sz == (*pdrgCHAR)[i]);
//	}
//
//	pdrgCHAR->Clear();
//	GPOS_ASSERT(0 == pdrgCHAR->UlLength());
//
//	pdrgCHAR->Release();
//
//	return GPOS_OK;
//}
//
//
////---------------------------------------------------------------------------
////	@function:
////		CDynamicArrayTest::EresUnittest_ArrayAppend
////
////	@doc:
////		Appending arrays
////
////---------------------------------------------------------------------------
//GPOS_RESULT
//CDynamicArrayTest::EresUnittest_ArrayAppend()
//{
//	// create memory pool
//	CAutoMemoryPool amp;
//	IMemoryPool *pmp = amp.Pmp();
//
//	typedef CDynamicPtrArray<ULONG, CleanupNULL<ULONG> > DrgULONG;
//
//	ULONG cVal = 0;
//
//	// array with 1 element
//	DrgULONG *pdrgULONG1 = GPOS_NEW(pmp) DrgULONG(pmp, 1);
//	pdrgULONG1->Append(&cVal);
//	GPOS_ASSERT(1 == pdrgULONG1->UlLength());
//
//	// array with x elements
//	ULONG cX = 1000;
//	DrgULONG *pdrgULONG2 = GPOS_NEW(pmp) DrgULONG(pmp, 1);
//	for(ULONG i = 0; i < cX; i++)
//	{
//		pdrgULONG2->Append(&cX);
//	}
//	GPOS_ASSERT(cX == pdrgULONG2->UlLength());
//
//	// add one to another
//	pdrgULONG1->AppendArray(pdrgULONG2);
//	GPOS_ASSERT(cX + 1 == pdrgULONG1->UlLength());
//	for (ULONG j = 0; j < pdrgULONG2->UlLength(); j++)
//	{
//		GPOS_ASSERT((*pdrgULONG1)[j + 1] == (*pdrgULONG2)[j]);
//	}
//
//	pdrgULONG1->Release();
//	pdrgULONG2->Release();
//
//	return GPOS_OK;
//}
//
//
//
////---------------------------------------------------------------------------
////	@function:
////		CDynamicArrayTest::EresUnittest_ArrayAppendExactFit
////
////	@doc:
////		Appending arrays when there is enough memory in first
////
////---------------------------------------------------------------------------
//GPOS_RESULT
//CDynamicArrayTest::EresUnittest_ArrayAppendExactFit()
//{
//	// create memory pool
//	CAutoMemoryPool amp;
//	IMemoryPool *pmp = amp.Pmp();
//
//	typedef CDynamicPtrArray<ULONG, CleanupNULL<ULONG> > DrgULONG;
//
//	ULONG cVal = 0;
//
//	// array with 1 element
//	DrgULONG *pdrgULONG1 = GPOS_NEW(pmp) DrgULONG(pmp, 10);
//	pdrgULONG1->Append(&cVal);
//	GPOS_ASSERT(1 == pdrgULONG1->UlLength());
//
//	// array with x elements
//	ULONG cX = 9;
//	DrgULONG *pdrgULONG2 = GPOS_NEW(pmp) DrgULONG(pmp, 15);
//	for(ULONG i = 0; i < cX; i++)
//	{
//		pdrgULONG2->Append(&cX);
//	}
//	GPOS_ASSERT(cX == pdrgULONG2->UlLength());
//
//	// add one to another
//	pdrgULONG1->AppendArray(pdrgULONG2);
//	GPOS_ASSERT(cX + 1 == pdrgULONG1->UlLength());
//	for (ULONG j = 0; j < pdrgULONG2->UlLength(); j++)
//	{
//		GPOS_ASSERT((*pdrgULONG1)[j + 1] == (*pdrgULONG2)[j]);
//	}
//
//	DrgULONG *pdrgULONG3 = GPOS_NEW(pmp) DrgULONG(pmp, 15);
//	pdrgULONG1->AppendArray(pdrgULONG3);
//	GPOS_ASSERT(cX + 1 == pdrgULONG1->UlLength());
//
//	pdrgULONG1->Release();
//	pdrgULONG2->Release();
//	pdrgULONG3->Release();
//
//	return GPOS_OK;
//}
//
////---------------------------------------------------------------------------
////	@function:
////		CDynamicArrayTest::EresUnittest_PdrgpulSubsequenceIndexes
////
////	@doc:
////		Finding the first occurrences of the elements of the first array
////		in the second one.
////
////---------------------------------------------------------------------------
//GPOS_RESULT
//CDynamicArrayTest::EresUnittest_PdrgpulSubsequenceIndexes()
//{
//	typedef CDynamicPtrArray<ULONG, CleanupNULL<ULONG> > DrgULONG;
//
//	CAutoMemoryPool amp;
//	IMemoryPool *pmp = amp.Pmp();
//
//	// the array containing elements to look up
//	DrgULONG *pdrgULONGLookup = GPOS_NEW(pmp) DrgULONG(pmp);
//
//	// the array containing the target elements that will give the positions
//	DrgULONG *pdrgULONGTarget = GPOS_NEW(pmp) DrgULONG(pmp);
//
//	ULONG *pul1 = GPOS_NEW(pmp) ULONG(10);
//	ULONG *pul2 = GPOS_NEW(pmp) ULONG(20);
//	ULONG *pul3 = GPOS_NEW(pmp) ULONG(30);
//
//	pdrgULONGLookup->Append(pul1);
//	pdrgULONGLookup->Append(pul2);
//	pdrgULONGLookup->Append(pul3);
//	pdrgULONGLookup->Append(pul3);
//
//	// since target is empty, there are elements in lookup with no match, so the function
//	// should return NULL
//	GPOS_ASSERT(NULL ==
//			CDynamicPtrArrayUtils::PdrgpulSubsequenceIndexes(pmp, pdrgULONGLookup, pdrgULONGTarget));
//
//	pdrgULONGTarget->Append(pul1);
//	pdrgULONGTarget->Append(pul3);
//	pdrgULONGTarget->Append(pul3);
//	pdrgULONGTarget->Append(pul3);
//	pdrgULONGTarget->Append(pul2);
//
//	DrgPul *pdrgpulIndexes =
//			CDynamicPtrArrayUtils::PdrgpulSubsequenceIndexes(pmp, pdrgULONGLookup, pdrgULONGTarget);
//	GPOS_ASSERT(NULL != pdrgpulIndexes);
//	GPOS_ASSERT(4 == pdrgpulIndexes->UlLength());
//	GPOS_ASSERT(0 == *(*pdrgpulIndexes)[0]);
//	GPOS_ASSERT(4 == *(*pdrgpulIndexes)[1]);
//	GPOS_ASSERT(1 == *(*pdrgpulIndexes)[2]);
//	GPOS_ASSERT(1 == *(*pdrgpulIndexes)[3]);
//
//	GPOS_DELETE(pul1);
//	GPOS_DELETE(pul2);
//	GPOS_DELETE(pul3);
//	pdrgpulIndexes->Release();
//	pdrgULONGTarget->Release();
//	pdrgULONGLookup->Release();
//
//	return GPOS_OK;
//}
//
//// EOF
//
