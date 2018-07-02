//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2018 Pivotal, Inc.
//
//	@filename:
//		CStatisticsUtils.cpp
//
//	@doc:
//		Statistics helper routines
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/exception.h"
#include "gpopt/operators/ops.h"
#include "gpopt/operators/CExpressionUtils.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CJoinStatsProcessor.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatsPredUtils.h"
#include "naucrates/statistics/CStatsPredDisj.h"
#include "naucrates/statistics/CStatsPredConj.h"
#include "naucrates/statistics/CStatsPredLike.h"
#include "naucrates/statistics/CScaleFactorUtils.h"
#include "naucrates/statistics/CHistogram.h"

#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeInt2.h"
#include "naucrates/md/IMDTypeInt8.h"
#include "naucrates/md/IMDTypeOid.h"
#include "naucrates/md/CMDIdColStats.h"

#include "naucrates/base/IDatumInt2.h"
#include "naucrates/base/IDatumInt4.h"
#include "naucrates/base/IDatumInt8.h"
#include "naucrates/base/IDatumOid.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::NextPoint
//
//	@doc:
// 		Get the next data point for new bucket boundary
//
//---------------------------------------------------------------------------
CPoint *
CStatisticsUtils::NextPoint(IMemoryPool *memory_pool, CMDAccessor *md_accessor, CPoint *point)
{
	IMDId *mdid = point->GetDatum()->MDId();
	const IMDType *mdtype = md_accessor->RetrieveType(mdid);

	// type has integer mapping
	if (mdtype->GetDatumType() == IMDType::EtiInt2 || mdtype->GetDatumType() == IMDType::EtiInt4 ||
		mdtype->GetDatumType() == IMDType::EtiInt8 || mdtype->GetDatumType() == IMDType::EtiOid)
	{
		IDatum *datum_new = NULL;

		IDatum *datum_old = point->GetDatum();

		if (mdtype->GetDatumType() == IMDType::EtiInt2)
		{
			SINT sValue = (SINT)(dynamic_cast<IDatumInt2 *>(datum_old)->Value() + 1);
			datum_new = dynamic_cast<const IMDTypeInt2 *>(mdtype)->CreateInt2Datum(
				memory_pool, sValue, false);
		}
		else if (mdtype->GetDatumType() == IMDType::EtiInt4)
		{
			INT iValue = dynamic_cast<IDatumInt4 *>(datum_old)->Value() + 1;
			datum_new = dynamic_cast<const IMDTypeInt4 *>(mdtype)->CreateInt4Datum(
				memory_pool, iValue, false);
		}
		else if (mdtype->GetDatumType() == IMDType::EtiInt8)
		{
			LINT value = dynamic_cast<IDatumInt8 *>(datum_old)->Value() + 1;
			datum_new = dynamic_cast<const IMDTypeInt8 *>(mdtype)->CreateInt8Datum(
				memory_pool, value, false);
		}
		else
		{
			OID oidValue = dynamic_cast<IDatumOid *>(datum_old)->OidValue() + 1;
			datum_new = dynamic_cast<const IMDTypeOid *>(mdtype)->CreateOidDatum(
				memory_pool, oidValue, false);
		}

		return GPOS_NEW(memory_pool) CPoint(datum_new);
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::TransformMCVToHist
//
//	@doc:
//		Given MCVs and their frequencies, construct a CHistogram
//		containing MCV singleton buckets
//---------------------------------------------------------------------------
CHistogram *
CStatisticsUtils::TransformMCVToHist(IMemoryPool *memory_pool,
									 const IMDType *,  // mdtype,
									 IDatumArray *mcv_datums,
									 CDoubleArray *freq_array,
									 ULONG num_mcv_values)
{
	GPOS_ASSERT(mcv_datums->Size() == num_mcv_values);

	// put MCV values and their corresponding frequencies
	// into a structure in order to sort
	McvPairPtrArray *mcv_pairs = GPOS_NEW(memory_pool) McvPairPtrArray(memory_pool);
	for (ULONG i = 0; i < num_mcv_values; i++)
	{
		IDatum *datum = (*mcv_datums)[i];
		CDouble mcv_freq = *((*freq_array)[i]);
		datum->AddRef();
		SMcvPair *mcv_pair = GPOS_NEW(memory_pool) SMcvPair(datum, mcv_freq);
		mcv_pairs->Append(mcv_pair);
	}

	// sort the MCV m_bytearray_value-frequency pairs according to m_bytearray_value
	if (1 < num_mcv_values)
	{
		mcv_pairs->Sort(CStatisticsUtils::GetMcvPairCmpFunc);
	}

	// now put MCVs and their frequencies in buckets
	BucketArray *mcv_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);

	for (ULONG i = 0; i < num_mcv_values; i++)
	{
		IDatum *datum = (*mcv_pairs)[i]->m_datum_mcv;
		datum->AddRef();
		datum->AddRef();
		CDouble bucket_freq = (*mcv_pairs)[i]->m_mcv_freq;
		CBucket *bucket = GPOS_NEW(memory_pool) CBucket(GPOS_NEW(memory_pool) CPoint(datum),
														GPOS_NEW(memory_pool) CPoint(datum),
														true /* is_lower_closed */,
														true /* is_upper_closed */,
														bucket_freq,
														CDouble(1.0));
		mcv_buckets->Append(bucket);
	}
	CHistogram *histogram = GPOS_NEW(memory_pool) CHistogram(mcv_buckets);
	GPOS_ASSERT(histogram->IsValid());
	mcv_pairs->Release();

	return histogram;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::MergeMCVHist
//
//	@doc:
//		Given MCVs and histogram in CHistogram, merge them into a single
//		CHistogram
//
//---------------------------------------------------------------------------
CHistogram *
CStatisticsUtils::MergeMCVHist(IMemoryPool *memory_pool,
							   const CHistogram *mcv_histogram,
							   const CHistogram *histogram)
{
	GPOS_ASSERT(NULL != mcv_histogram);
	GPOS_ASSERT(NULL != histogram);
	GPOS_ASSERT(mcv_histogram->IsWellDefined());
	GPOS_ASSERT(histogram->IsWellDefined());
	GPOS_ASSERT(0 < mcv_histogram->Buckets());
	GPOS_ASSERT(0 < histogram->Buckets());

	const BucketArray *mcv_buckets = mcv_histogram->ParseDXLToBucketsArray();
	const BucketArray *histogram_buckets = histogram->ParseDXLToBucketsArray();

	IDatum *datum = (*mcv_buckets)[0]->GetLowerBound()->GetDatum();

	// data types that are not supported in the new optimizer yet
	if (!datum->StatsAreComparable(datum))
	{
		// fall back to the approach that chooses the one having more information
		if (0.5 < mcv_histogram->GetFrequency())
		{
			// have to do deep copy, otherwise mcv_histogram and phistMerge
			// will point to the same object
			return mcv_histogram->CopyHistogram(memory_pool);
		}

		return histogram->CopyHistogram(memory_pool);
	}

	// both MCV and histogram buckets must be sorted
	GPOS_ASSERT(mcv_histogram->IsValid());
	GPOS_ASSERT(histogram->IsValid());

	BucketArray *merged_buckets = MergeMcvHistBucket(memory_pool, mcv_buckets, histogram_buckets);

	CHistogram *merged_histogram = GPOS_NEW(memory_pool) CHistogram(merged_buckets);
	GPOS_ASSERT(merged_histogram->IsValid());

	return merged_histogram;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::PdrgpbucketCreateMergedBuckets
//
//	@doc:
//		Given histogram buckets and MCV buckets, merge them into
//		an array of buckets.
//
//---------------------------------------------------------------------------
BucketArray *
CStatisticsUtils::MergeMcvHistBucket(IMemoryPool *memory_pool,
									 const BucketArray *mcv_buckets,
									 const BucketArray *histogram_buckets)
{
	BucketArray *merged_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	const ULONG mcv = mcv_buckets->Size();
	const ULONG num_histograms = histogram_buckets->Size();
	ULONG mcv_index = 0;
	ULONG histogram_index = 0;

	while (mcv_index < mcv && histogram_index < num_histograms)
	{
		CBucket *mcv_bucket = (*mcv_buckets)[mcv_index];
		CBucket *histogram_bucket = (*histogram_buckets)[histogram_index];

		if (mcv_bucket->IsBefore(histogram_bucket))
		{
			merged_buckets->Append(mcv_bucket->MakeBucketCopy(memory_pool));
			mcv_index++;
		}
		else if (mcv_bucket->IsAfter(histogram_bucket))
		{
			merged_buckets->Append(histogram_bucket->MakeBucketCopy(memory_pool));
			histogram_index++;
		}
		else  // mcv_bucket is contained in histogram_bucket
		{
			GPOS_ASSERT(histogram_bucket->Subsumes(mcv_bucket));
			SplitHistDriver(
				memory_pool, histogram_bucket, mcv_buckets, merged_buckets, &mcv_index, mcv);
			histogram_index++;
		}
	}

	// append leftover buckets from either MCV or histogram
	AddRemainingBuckets(memory_pool, mcv_buckets, merged_buckets, &mcv_index);
	AddRemainingBuckets(memory_pool, histogram_buckets, merged_buckets, &histogram_index);

	return merged_buckets;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::AddRemainingBuckets
//
//	@doc:
//		Add remaining buckets from one array of buckets to the other
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::AddRemainingBuckets(IMemoryPool *memory_pool,
									  const BucketArray *src_buckets,
									  BucketArray *dest_buckets,
									  ULONG *start_val)
{
	const ULONG ulTotal = src_buckets->Size();

	while (*start_val < ulTotal)
	{
		dest_buckets->Append((*src_buckets)[*start_val]->MakeBucketCopy(memory_pool));
		(*start_val)++;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::SplitHistDriver
//
//	@doc:
//		Given an MCV that are contained in a histogram bucket,
//		find the batch of MCVs that fall in the same histogram bucket.
//		Then perform the split for this batch of MCVs.
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::SplitHistDriver(IMemoryPool *memory_pool,
								  const CBucket *histogram_bucket,
								  const BucketArray *mcv_buckets,
								  BucketArray *merged_buckets,
								  ULONG *mcv_index,
								  ULONG mcv)
{
	GPOS_ASSERT(NULL != histogram_bucket);
	GPOS_ASSERT(NULL != mcv_buckets);

	BucketArray *temp_mcv_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);

	// find the MCVs that fall into the same histogram bucket and put them in a temp array
	// E.g. MCV = ..., 6, 8, 12, ... and the current histogram bucket is [5,10)
	// then 6 and 8 will be handled together, i.e. split [5,10) into [5,6) [6,6] (6,8) [8,8] (8,10)
	while ((*mcv_index) < mcv && histogram_bucket->Subsumes((*mcv_buckets)[*mcv_index]))
	{
		CBucket *curr_mcv_bucket = (*mcv_buckets)[*mcv_index];
		temp_mcv_buckets->Append(curr_mcv_bucket->MakeBucketCopy(memory_pool));
		(*mcv_index)++;
	}

	// split histogram_bucket given one or more MCVs it contains
	BucketArray *split_buckets =
		SplitHistBucketGivenMcvBuckets(memory_pool, histogram_bucket, temp_mcv_buckets);
	const ULONG split_bucket_size = split_buckets->Size();

	// copy buckets from pdrgpbucketSplitted to pdrgbucketMerged
	for (ULONG i = 0; i < split_bucket_size; i++)
	{
		CBucket *curr_split_bucket = (*split_buckets)[i];
		merged_buckets->Append(curr_split_bucket->MakeBucketCopy(memory_pool));
	}

	temp_mcv_buckets->Release();
	split_buckets->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::SplitHistBucketGivenMcvBuckets
//
//	@doc:
//		Given an array of MCVs that are contained in a histogram bucket,
//		split the histogram bucket into smaller buckets with the MCVs being
//		the splitting points. The MCVs are returned too, among the smaller
//		buckets.
//
//---------------------------------------------------------------------------
BucketArray *
CStatisticsUtils::SplitHistBucketGivenMcvBuckets(IMemoryPool *memory_pool,
												 const CBucket *histogram_bucket,
												 const BucketArray *mcv_buckets)
{
	GPOS_ASSERT(NULL != histogram_bucket);
	GPOS_ASSERT(NULL != mcv_buckets);

	BucketArray *buckets_after_split = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	const ULONG mcv = mcv_buckets->Size();
	GPOS_ASSERT(0 < mcv);

	// construct first bucket, if any
	CPoint *mcv_point = (*mcv_buckets)[0]->GetLowerBound();
	CBucket *first_bucket = CreateValidBucket(memory_pool,
											  histogram_bucket->GetLowerBound(),
											  mcv_point,
											  histogram_bucket->IsLowerClosed(),
											  false  // is_upper_closed
	);
	if (NULL != first_bucket)
	{
		buckets_after_split->Append(first_bucket);
	}

	// construct middle buckets, if any
	for (ULONG idx = 0; idx < mcv - 1; idx++)
	{
		// first append the MCV itself
		CBucket *mcv_bucket = (*mcv_buckets)[idx];
		buckets_after_split->Append(mcv_bucket->MakeBucketCopy(memory_pool));

		// construct new buckets
		CPoint *point_left = mcv_bucket->GetLowerBound();				 // this MCV
		CPoint *point_right = (*mcv_buckets)[idx + 1]->GetLowerBound();  // next MCV

		CBucket *new_bucket = CreateValidBucket(memory_pool, point_left, point_right, false, false);
		if (NULL != new_bucket)
		{
			buckets_after_split->Append(new_bucket);
		}
	}

	// append last MCV
	CBucket *last_mcv_bucket = (*mcv_buckets)[mcv - 1];
	buckets_after_split->Append(last_mcv_bucket->MakeBucketCopy(memory_pool));
	mcv_point = last_mcv_bucket->GetLowerBound();

	// construct last bucket, if any
	CBucket *last_bucket = CreateValidBucket(memory_pool,
											 mcv_point,
											 histogram_bucket->GetUpperBound(),
											 false,
											 histogram_bucket->IsUpperClosed());
	if (NULL != last_bucket)
	{
		buckets_after_split->Append(last_bucket);
	}

	// re-balance distinct and frequency in pdrgpbucketNew
	CDouble total_distinct_values =
		std::max(CDouble(1.0), histogram_bucket->GetNumDistinct() - mcv);
	DistributeBucketProperties(
		histogram_bucket->GetFrequency(), total_distinct_values, buckets_after_split);

	return buckets_after_split;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::CreateValidBucket
//
//	@doc:
//		Given lower and upper and their closedness, create a bucket if they
//		can form a valid bucket
//
//---------------------------------------------------------------------------
CBucket *
CStatisticsUtils::CreateValidBucket(IMemoryPool *memory_pool,
									CPoint *bucket_lower_bound,
									CPoint *bucket_upper_bound,
									BOOL is_lower_closed,
									BOOL is_upper_closed)
{
	if (!IsValidBucket(bucket_lower_bound, bucket_upper_bound, is_lower_closed, is_upper_closed))
	{
		return NULL;
	}
	bucket_lower_bound->AddRef();
	bucket_upper_bound->AddRef();

	return GPOS_NEW(memory_pool)
		CBucket(bucket_lower_bound,
				bucket_upper_bound,
				is_lower_closed,
				is_upper_closed,
				GPOPT_BUCKET_DEFAULT_FREQ,	 // frequency will be assigned later
				GPOPT_BUCKET_DEFAULT_DISTINCT  // distinct will be assigned later
		);
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::IsValidBucket
//
//	@doc:
//		Given lower and upper and their closedness, test if they
//		can form a valid bucket.
//		E.g. [1,1) (2,3) are not valid integer buckets, (2.0, 3.0) is a
//		valid numeric bucket.
//
//---------------------------------------------------------------------------
BOOL
CStatisticsUtils::IsValidBucket(CPoint *bucket_lower_bound,
								CPoint *bucket_upper_bound,
								BOOL is_lower_closed,
								BOOL is_upper_closed)
{
	if (bucket_lower_bound->IsGreaterThan(bucket_upper_bound))
	{
		return false;
	}

	// e.g. [1.0, 1.0) is not valid
	if (bucket_lower_bound->Equals(bucket_upper_bound) && (!is_lower_closed || !is_upper_closed))
	{
		return false;
	}

	// datum has statsDistance, so must be statsMappable
	const IDatum *datum = bucket_lower_bound->GetDatum();
	const IDatumStatisticsMappable *stats_mappable_datum =
		dynamic_cast<const IDatumStatisticsMappable *>(datum);

	// for types which have integer mapping for stats purposes, e.g. int2,int4, etc.
	if (stats_mappable_datum->IsDatumMappableToLINT())
	{
		// test if this integer bucket is well-defined
		CDouble bound_diff = bucket_upper_bound->Distance(bucket_lower_bound);
		if (!is_lower_closed)
		{
			bound_diff = bound_diff + CDouble(-1.0);
		}
		if (!is_upper_closed)
		{
			bound_diff = bound_diff + CDouble(-1.0);
		}
		if (CDouble(0) > bound_diff)
		{
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::DistributeBucketProperties
//
//	@doc:
//		Set distinct and frequency of the new buckets according to
//		their ranges, based on the assumption that values are uniformly
//		distributed within a bucket.
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::DistributeBucketProperties(CDouble total_frequency,
											 CDouble total_distinct_values,
											 BucketArray *buckets)
{
	GPOS_ASSERT(NULL != buckets);

	CDouble bucket_width = 0.0;
	const ULONG bucket_size = buckets->Size();

	for (ULONG i = 0; i < bucket_size; i++)
	{
		CBucket *bucket = (*buckets)[i];
		if (!bucket->IsSingleton())  // the re-balance should exclude MCVs (singleton bucket)
		{
			bucket_width = bucket_width + bucket->Width();
		}
	}
	for (ULONG i = 0; i < bucket_size; i++)
	{
		CBucket *bucket = (*buckets)[i];
		if (!bucket->IsSingleton())
		{
			CDouble factor = bucket->Width() / bucket_width;
			bucket->SetFrequency(total_frequency * factor);
			// TODO: , Aug 1 2013 - another heuristic may be max(1, dDisinct * factor)
			bucket->SetDistinct(total_distinct_values * factor);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::PrintColStats
//
//	@doc:
//		Utility function to print column stats before/after applying filter
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::PrintColStats(IMemoryPool *memory_pool,
								CStatsPred *pred_stats,
								ULONG cond_colid,
								CHistogram *histogram,
								CDouble last_scale_factor,
								BOOL is_filter_applied_before)
{
	GPOS_ASSERT(NULL != pred_stats);
	ULONG col_id = pred_stats->GetColId();
	if (col_id == cond_colid && NULL != histogram)
	{
		{
			CAutoTrace at(memory_pool);
			if (is_filter_applied_before)
			{
				at.Os() << "BEFORE" << std::endl;
			}
			else
			{
				at.Os() << "AFTER" << std::endl;
			}

			histogram->OsPrint(at.Os());
			at.Os() << "Scale Factor: " << last_scale_factor << std::endl;
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::ExtractUsedColIds
//
//	@doc:
//		Extract all the column identifiers used in the statistics filter
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::ExtractUsedColIds(IMemoryPool *memory_pool,
									CBitSet *col_ids_bitset,
									CStatsPred *pred_stats,
									ULongPtrArray *col_ids)
{
	GPOS_ASSERT(NULL != col_ids_bitset);
	GPOS_ASSERT(NULL != pred_stats);
	GPOS_ASSERT(NULL != col_ids);

	if (gpos::ulong_max != pred_stats->GetColId())
	{
		// the predicate is on a single column

		(void) col_ids_bitset->ExchangeSet(pred_stats->GetColId());
		col_ids->Append(GPOS_NEW(memory_pool) ULONG(pred_stats->GetColId()));

		return;
	}

	if (CStatsPred::EsptUnsupported == pred_stats->GetPredStatsType())
	{
		return;
	}

	GPOS_ASSERT(CStatsPred::EsptConj == pred_stats->GetPredStatsType() ||
				CStatsPred::EsptDisj == pred_stats->GetPredStatsType());

	StatsPredPtrArry *stats_pred_array = NULL;
	if (CStatsPred::EsptConj == pred_stats->GetPredStatsType())
	{
		stats_pred_array = CStatsPredConj::ConvertPredStats(pred_stats)->GetConjPredStatsArray();
	}
	else
	{
		stats_pred_array = CStatsPredDisj::ConvertPredStats(pred_stats)->GetDisjPredStatsArray();
	}

	GPOS_ASSERT(NULL != stats_pred_array);
	const ULONG arity = stats_pred_array->Size();
	for (ULONG i = 0; i < arity; i++)
	{
		CStatsPred *curr_stats_pred = (*stats_pred_array)[i];
		ULONG col_id = curr_stats_pred->GetColId();

		if (gpos::ulong_max != col_id)
		{
			if (!col_ids_bitset->Get(col_id))
			{
				(void) col_ids_bitset->ExchangeSet(col_id);
				col_ids->Append(GPOS_NEW(memory_pool) ULONG(col_id));
			}
		}
		else if (CStatsPred::EsptUnsupported != curr_stats_pred->GetPredStatsType())
		{
			GPOS_ASSERT(CStatsPred::EsptConj == curr_stats_pred->GetPredStatsType() ||
						CStatsPred::EsptDisj == curr_stats_pred->GetPredStatsType());
			ExtractUsedColIds(memory_pool, col_ids_bitset, curr_stats_pred, col_ids);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::UpdateDisjStatistics
//
//	@doc:
//		Given the previously generated histogram, update the intermediate
//		result of the disjunction
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::UpdateDisjStatistics(IMemoryPool *memory_pool,
									   CBitSet *dont_update_stats_bitset,
									   CDouble input_disjunct_rows,
									   CDouble local_rows,
									   CHistogram *previous_histogram,
									   UlongHistogramHashMap *disjunctive_result_histograms,
									   ULONG col_id)
{
	GPOS_ASSERT(NULL != dont_update_stats_bitset);
	GPOS_ASSERT(NULL != disjunctive_result_histograms);

	if (NULL != previous_histogram && gpos::ulong_max != col_id &&
		!dont_update_stats_bitset->Get(col_id))
	{
		// 1. the filter is on the same column because gpos::ulong_max != col_id
		// 2. the histogram of the column can be updated
		CHistogram *result_histogram = disjunctive_result_histograms->Find(&col_id);
		if (NULL != result_histogram)
		{
			// since there is already a histogram for this column,
			// union the previously generated histogram with the newly generated
			// histogram for this column
			CDouble output_rows(0.0);
			CHistogram *new_histogram = previous_histogram->MakeUnionHistogramNormalize(
				memory_pool, input_disjunct_rows, result_histogram, local_rows, &output_rows);

			GPOS_DELETE(previous_histogram);
			previous_histogram = new_histogram;
		}

		AddHistogram(memory_pool,
					 col_id,
					 previous_histogram,
					 disjunctive_result_histograms,
					 true /*fReplaceOldEntries*/
		);
	}

	GPOS_DELETE(previous_histogram);
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::GetColsNonUpdatableHistForDisj
//
//	@doc:
//		Given a disjunction filter, generate a bit set of columns whose
// 		histogram buckets cannot be changed by applying the predicates in the
//		disjunction
//---------------------------------------------------------------------------
CBitSet *
CStatisticsUtils::GetColsNonUpdatableHistForDisj(IMemoryPool *memory_pool,
												 CStatsPredDisj *pred_stats)
{
	GPOS_ASSERT(NULL != pred_stats);

	// Consider the following disjunctive predicates:
	// Case 1: ((x == 1) OR (x == 2 AND y == 2))
	// In such scenarios, predicate y is only operated on by the second child.
	// Therefore the output of the disjunction should not see the effects on
	// y's histogram due to the second child. In other words, DO NOT
	// update histogram buckets for y.

	// Case 2: ((x == 1 AND y== 1) OR (x == 2 AND y == 2))
	// In such scenarios both child predicate operate on both x and y
	// therefore the output of the disjunction for each column should be
	// the union of stats of each predicate being applied separately.
	// In other words, DO update histogram buckets for both x and y.

	CBitSet *non_updateable_bitset = GPOS_NEW(memory_pool) CBitSet(memory_pool);

	const ULONG disj_colid = pred_stats->GetColId();
	if (gpos::ulong_max != disj_colid)
	{
		// disjunction predicate on a single column so all are updatable
		return non_updateable_bitset;
	}

	CBitSet *disj_bitset = GPOS_NEW(memory_pool) CBitSet(memory_pool);
	ULongPtrArray *disjuncts = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	ExtractUsedColIds(memory_pool, disj_bitset, pred_stats, disjuncts);
	const ULONG num_disj_used_col = disjuncts->Size();

	const ULONG arity = pred_stats->GetNumPreds();
	for (ULONG child_index = 0; child_index < arity; child_index++)
	{
		CStatsPred *child_pred_stats = pred_stats->GetPredStats(child_index);
		CBitSet *child_bitset = GPOS_NEW(memory_pool) CBitSet(memory_pool);
		ULongPtrArray *child_col_ids = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
		ExtractUsedColIds(memory_pool, child_bitset, child_pred_stats, child_col_ids);

		const ULONG length = child_col_ids->Size();
		GPOS_ASSERT(length <= num_disj_used_col);
		if (length < num_disj_used_col)
		{
			// the child predicate only operates on a subset of all the columns
			// used in the disjunction
			for (ULONG used_col_idx = 0; used_col_idx < num_disj_used_col; used_col_idx++)
			{
				ULONG col_id = *(*disjuncts)[used_col_idx];
				if (!child_bitset->Get(col_id))
				{
					(void) non_updateable_bitset->ExchangeSet(col_id);
				}
			}
		}

		// clean up
		child_col_ids->Release();
		child_bitset->Release();
	}

	// clean up
	disjuncts->Release();
	disj_bitset->Release();

	return non_updateable_bitset;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::AddHistogram
//
//	@doc:
//		Add histogram to histogram map if not already present.
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::AddHistogram(IMemoryPool *memory_pool,
							   ULONG col_id,
							   const CHistogram *histogram,
							   UlongHistogramHashMap *col_histogram_mapping,
							   BOOL replace_old)
{
	GPOS_ASSERT(NULL != histogram);

	if (NULL == col_histogram_mapping->Find(&col_id))
	{
#ifdef GPOS_DEBUG
		BOOL result =
#endif
			col_histogram_mapping->Insert(GPOS_NEW(memory_pool) ULONG(col_id),
										  histogram->CopyHistogram(memory_pool));
		GPOS_ASSERT(result);
	}
	else if (replace_old)
	{
#ifdef GPOS_DEBUG
		BOOL result =
#endif
			col_histogram_mapping->Replace(&col_id, histogram->CopyHistogram(memory_pool));
		GPOS_ASSERT(result);
	}
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::PrintHistogramMap
//
//	@doc:
//		Helper method to print the hash map of histograms
//
//---------------------------------------------------------------------------
void
CStatisticsUtils::PrintHistogramMap(IOstream &os, UlongHistogramHashMap *col_histogram_mapping)
{
	GPOS_ASSERT(NULL != col_histogram_mapping);

	UlongHistogramHashMapIter col_hist_mapping(col_histogram_mapping);
	while (col_hist_mapping.Advance())
	{
		ULONG column = *(col_hist_mapping.Key());

		os << "Column Id: " << column << std::endl;
		const CHistogram *histogram = col_hist_mapping.Value();
		histogram->OsPrint(os);
	}
}
#endif  // GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::CreateHistHashMapAfterMergingDisjPreds
//
//	@doc:
//		Create a new hash map of histograms after merging
//		the histograms generated by the child of disjunction
//
//---------------------------------------------------------------------------
UlongHistogramHashMap *
CStatisticsUtils::CreateHistHashMapAfterMergingDisjPreds(
	IMemoryPool *memory_pool,
	CBitSet *non_updatable_cols,
	UlongHistogramHashMap *col_histogram_mapping,
	UlongHistogramHashMap *disj_preds_histogram_map,
	CDouble cumulative_rows,
	CDouble num_rows_disj_child)
{
	GPOS_ASSERT(NULL != non_updatable_cols);
	GPOS_ASSERT(NULL != col_histogram_mapping);
	GPOS_ASSERT(NULL != disj_preds_histogram_map);

	BOOL is_empty = (CStatistics::Epsilon >= num_rows_disj_child);
	CDouble output_rows(CStatistics::MinRows.Get());

	UlongHistogramHashMap *merged_histogram =
		GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);

	// iterate over the new hash map of histograms and only add
	// histograms of columns whose output statistics can be updated
	UlongHistogramHashMapIter disj_hist_iter(disj_preds_histogram_map);
	while (disj_hist_iter.Advance())
	{
		ULONG disj_child_colid = *(disj_hist_iter.Key());
		const CHistogram *disj_child_histogram = disj_hist_iter.Value();
		if (!non_updatable_cols->Get(disj_child_colid))
		{
			if (!is_empty)
			{
				AddHistogram(memory_pool, disj_child_colid, disj_child_histogram, merged_histogram);
			}
			else
			{
				// add a dummy statistics object since the estimated number of rows for
				// disjunction child is "0"
				merged_histogram->Insert(
					GPOS_NEW(memory_pool) ULONG(disj_child_colid),
					GPOS_NEW(memory_pool) CHistogram(GPOS_NEW(memory_pool) BucketArray(memory_pool),
													 false /* is_well_defined */));
			}
		}
		GPOS_CHECK_ABORT;
	}

	// iterate over the previously generated histograms and
	// union them with newly created hash map of histograms (if these columns are updatable)
	UlongHistogramHashMapIter col_hist_mapping_iter(col_histogram_mapping);
	while (col_hist_mapping_iter.Advance())
	{
		ULONG col_id = *(col_hist_mapping_iter.Key());
		const CHistogram *histogram = col_hist_mapping_iter.Value();
		if (NULL != histogram && !non_updatable_cols->Get(col_id))
		{
			if (is_empty)
			{
				// since the estimated output of the disjunction child is "0" tuples
				// no point merging histograms.
				AddHistogram(
					memory_pool, col_id, histogram, merged_histogram, true /* replace_old */
				);
			}
			else
			{
				const CHistogram *disj_child_histogram = disj_preds_histogram_map->Find(&col_id);
				CHistogram *normalized_union_histogram =
					histogram->MakeUnionHistogramNormalize(memory_pool,
														   cumulative_rows,
														   disj_child_histogram,
														   num_rows_disj_child,
														   &output_rows);

				AddHistogram(memory_pool,
							 col_id,
							 normalized_union_histogram,
							 merged_histogram,
							 true /* fReplaceOld */
				);

				GPOS_DELETE(normalized_union_histogram);
			}

			GPOS_CHECK_ABORT;
		}
	}

	return merged_histogram;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::CopyHistHashMap
//
//	@doc:
//		Helper method to copy the hash map of histograms
//
//---------------------------------------------------------------------------
UlongHistogramHashMap *
CStatisticsUtils::CopyHistHashMap(IMemoryPool *memory_pool,
								  UlongHistogramHashMap *col_histogram_mapping)
{
	GPOS_ASSERT(NULL != col_histogram_mapping);

	UlongHistogramHashMap *histograms_copy =
		GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);

	UlongHistogramHashMapIter col_hist_mapping_iter(col_histogram_mapping);
	while (col_hist_mapping_iter.Advance())
	{
		ULONG col_id = *(col_hist_mapping_iter.Key());
		const CHistogram *histogram = col_hist_mapping_iter.Value();
		AddHistogram(memory_pool, col_id, histogram, histograms_copy);
		GPOS_CHECK_ABORT;
	}

	return histograms_copy;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::GetColId
//
//	@doc:
//		Return the column identifier of the filter if the predicate is
// 		on a single column else	return gpos::ulong_max
//
//---------------------------------------------------------------------------
ULONG
CStatisticsUtils::GetColId(const StatsPredPtrArry *pred_stats_array)
{
	GPOS_ASSERT(NULL != pred_stats_array);

	ULONG result_colid = gpos::ulong_max;
	BOOL is_same_col = true;

	const ULONG length = pred_stats_array->Size();
	for (ULONG i = 0; i < length && is_same_col; i++)
	{
		CStatsPred *pred_stats = (*pred_stats_array)[i];
		ULONG col_id = pred_stats->GetColId();
		if (gpos::ulong_max == result_colid)
		{
			result_colid = col_id;
		}
		is_same_col = (result_colid == col_id);
	}

	if (is_same_col)
	{
		return result_colid;
	}

	return gpos::ulong_max;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::DatumNull
//
//	@doc:
//		Generate a null datum given a column reference
//
//---------------------------------------------------------------------------
IDatum *
CStatisticsUtils::DatumNull(const CColRef *colref)
{
	const IMDType *mdtype = colref->RetrieveType();

	IDatum *datum = mdtype->DatumNull();
	datum->AddRef();

	return datum;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::DeriveStatsForDynamicScan
//
//	@doc:
//		Derive statistics of dynamic scan based on the stats of corresponding
//		part-selector in the given map,
//
//		for a given part table (R) with part selection predicate (R.pk=T.x),
//		the function assumes a LeftSemiJoin(R,T)_(R.pk=T.x) expression to
//		compute the stats of R after partitions are eliminated based on the
//		condition (R.pk=T.x)
//
//---------------------------------------------------------------------------
IStatistics *
CStatisticsUtils::DeriveStatsForDynamicScan(IMemoryPool *memory_pool,
											CExpressionHandle &expr_handle,
											ULONG part_idx_id,
											CPartFilterMap *part_filter_map)
{
	// extract part table base stats from passed handle
	IStatistics *base_table_stats = expr_handle.Pstats();
	GPOS_ASSERT(NULL != base_table_stats);

	if (!GPOS_FTRACE(EopttraceDeriveStatsForDPE))
	{
		// if stats derivation with dynamic partition elimitaion is disabled, we return base stats
		base_table_stats->AddRef();
		return base_table_stats;
	}

	if (!part_filter_map->FContainsScanId(part_idx_id) ||
		NULL == part_filter_map->Pstats(part_idx_id))
	{
		// no corresponding entry is found in map, return base stats
		base_table_stats->AddRef();
		return base_table_stats;
	}

	IStatistics *part_selector_stats = part_filter_map->Pstats(part_idx_id);
	CExpression *scalar_expr = part_filter_map->Pexpr(part_idx_id);

	ColRefSetArray *output_colrefs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);
	output_colrefs->Append(base_table_stats->GetColRefSet(memory_pool));
	output_colrefs->Append(part_selector_stats->GetColRefSet(memory_pool));

	CColRefSet *outer_refs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	// extract all the conjuncts
	CStatsPred *unsupported_pred_stats = NULL;
	StatsPredJoinArray *join_preds_stats = CStatsPredUtils::ExtractJoinStatsFromJoinPredArray(
		memory_pool, scalar_expr, output_colrefs, outer_refs, &unsupported_pred_stats);

	IStatistics *left_semi_join_stats =
		base_table_stats->CalcLSJoinStats(memory_pool, part_selector_stats, join_preds_stats);

	// TODO:  May 15 2014, handle unsupported predicates for LS joins
	// cleanup
	CRefCount::SafeRelease(unsupported_pred_stats);
	output_colrefs->Release();
	outer_refs->Release();
	join_preds_stats->Release();

	return left_semi_join_stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::DeriveStatsForIndexGet
//
//	@doc:
//		Derive statistics of (dynamic) index get
//
//---------------------------------------------------------------------------
IStatistics *
CStatisticsUtils::DeriveStatsForIndexGet(IMemoryPool *memory_pool,
										 CExpressionHandle &expr_handle,
										 StatsArray *stats_contexts)
{
	COperator::EOperatorId operator_id = expr_handle.Pop()->Eopid();
	GPOS_ASSERT(CLogical::EopLogicalIndexGet == operator_id ||
				CLogical::EopLogicalDynamicIndexGet == operator_id);

	// collect columns used by index conditions and distribution of the table
	// for statistics
	CColRefSet *used_col_refset = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	CTableDescriptor *table_descriptor = NULL;
	if (CLogical::EopLogicalIndexGet == operator_id)
	{
		CLogicalIndexGet *index_get_op = CLogicalIndexGet::PopConvert(expr_handle.Pop());
		table_descriptor = index_get_op->Ptabdesc();
		if (NULL != index_get_op->PcrsDist())
		{
			used_col_refset->Include(index_get_op->PcrsDist());
		}
	}
	else
	{
		CLogicalDynamicIndexGet *dynamic_index_get_op =
			CLogicalDynamicIndexGet::PopConvert(expr_handle.Pop());
		table_descriptor = dynamic_index_get_op->Ptabdesc();
		if (NULL != dynamic_index_get_op->PcrsDist())
		{
			used_col_refset->Include(dynamic_index_get_op->PcrsDist());
		}
	}

	CExpression *scalar_expr = expr_handle.PexprScalarChild(0 /*ulChidIndex*/);
	CExpression *local_expr = NULL;
	CExpression *outer_refs_expr = NULL;

	// get outer references from expression handle
	CColRefSet *outer_col_refset = expr_handle.GetRelationalProperties()->PcrsOuter();

	CPredicateUtils::SeparateOuterRefs(
		memory_pool, scalar_expr, outer_col_refset, &local_expr, &outer_refs_expr);

	used_col_refset->Union(expr_handle.GetDrvdScalarProps(0 /*child_index*/)->PcrsUsed());

	// filter out outer references in used columns
	used_col_refset->Difference(outer_col_refset);

	IStatistics *base_table_stats =
		CLogical::PstatsBaseTable(memory_pool, expr_handle, table_descriptor, used_col_refset);
	used_col_refset->Release();

	IStatistics *stats = CFilterStatsProcessor::MakeStatsFilterForScalarExpr(
		memory_pool, expr_handle, base_table_stats, local_expr, outer_refs_expr, stats_contexts);

	base_table_stats->Release();
	local_expr->Release();
	outer_refs_expr->Release();

	return stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::PstatsBitmapGet
//
//	@doc:
//		Derive statistics of bitmap table get
//
//---------------------------------------------------------------------------
IStatistics *
CStatisticsUtils::DeriveStatsForBitmapTableGet(IMemoryPool *memory_pool,
											   CExpressionHandle &expr_handle,
											   StatsArray *stats_contexts)
{
	GPOS_ASSERT(CLogical::EopLogicalBitmapTableGet == expr_handle.Pop()->Eopid() ||
				CLogical::EopLogicalDynamicBitmapTableGet == expr_handle.Pop()->Eopid());
	CTableDescriptor *table_descriptor = CLogical::PtabdescFromTableGet(expr_handle.Pop());

	// the index of the condition
	ULONG child_cond_index = 0;

	// get outer references from expression handle
	CColRefSet *outer_col_refset = expr_handle.GetRelationalProperties()->PcrsOuter();
	CExpression *local_expr = NULL;
	CExpression *outer_refs_expr = NULL;
	CExpression *scalar_expr = expr_handle.PexprScalarChild(child_cond_index);
	CPredicateUtils::SeparateOuterRefs(
		memory_pool, scalar_expr, outer_col_refset, &local_expr, &outer_refs_expr);

	// collect columns used by the index
	CColRefSet *used_col_refset = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	used_col_refset->Union(expr_handle.GetDrvdScalarProps(child_cond_index)->PcrsUsed());
	used_col_refset->Difference(outer_col_refset);
	IStatistics *base_table_stats =
		CLogical::PstatsBaseTable(memory_pool, expr_handle, table_descriptor, used_col_refset);
	used_col_refset->Release();
	IStatistics *stats = CFilterStatsProcessor::MakeStatsFilterForScalarExpr(
		memory_pool, expr_handle, base_table_stats, local_expr, outer_refs_expr, stats_contexts);

	base_table_stats->Release();
	local_expr->Release();
	outer_refs_expr->Release();

	return stats;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::GetGrpColIdToUpperBoundNDVIdxMap
//
//	@doc:
//		Return the mapping between the table column used for grouping to the
//	    logical operator id where it was defined. If the grouping column is
//      not a table column then the logical op id is initialized to gpos::ulong_max
//---------------------------------------------------------------------------
UlongUlongArrayHashMap *
CStatisticsUtils::GetGrpColIdToUpperBoundNDVIdxMap(
	IMemoryPool *memory_pool,
	CStatistics *stats,
	const CColRefSet *grp_cols_refset,
	CBitSet *keys  // keys derived during optimization
)
{
	GPOS_ASSERT(NULL != grp_cols_refset);
	GPOS_ASSERT(NULL != stats);

	UlongUlongArrayHashMap *grp_colid_upper_bound_ndv_idx_map =
		GPOS_NEW(memory_pool) UlongUlongArrayHashMap(memory_pool);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	// iterate over grouping columns
	CColRefSetIter col_refset_iter(*grp_cols_refset);
	while (col_refset_iter.Advance())
	{
		CColRef *grouping_colref = col_refset_iter.Pcr();
		ULONG col_id = grouping_colref->Id();
		if (NULL == keys || keys->Get(col_id))
		{
			// if keys are available then only consider grouping columns defined as
			// key columns else consider all grouping columns
			const CColRef *grouping_colref = col_factory->LookupColRef(col_id);
			const ULONG upper_bound_ndv_idx = stats->GetIndexUpperBoundNDVs(grouping_colref);
			const ULongPtrArray *ndv_col_id =
				grp_colid_upper_bound_ndv_idx_map->Find(&upper_bound_ndv_idx);
			if (NULL == ndv_col_id)
			{
				ULongPtrArray *col_ids_new = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
				col_ids_new->Append(GPOS_NEW(memory_pool) ULONG(col_id));
#ifdef GPOS_DEBUG
				BOOL fres =
#endif  // GPOS_DEBUG
					grp_colid_upper_bound_ndv_idx_map->Insert(
						GPOS_NEW(memory_pool) ULONG(upper_bound_ndv_idx), col_ids_new);
				GPOS_ASSERT(fres);
			}
			else
			{
				(const_cast<ULongPtrArray *>(ndv_col_id))
					->Append(GPOS_NEW(memory_pool) ULONG(col_id));
			}
		}
	}

	return grp_colid_upper_bound_ndv_idx_map;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::AddNdvForAllGrpCols
//
//	@doc:
//		Add the NDV for all of the grouping columns
//---------------------------------------------------------------------------
void
CStatisticsUtils::AddNdvForAllGrpCols(
	IMemoryPool *memory_pool,
	const CStatistics *input_stats,
	const ULongPtrArray *grouping_columns,  // array of grouping column ids from a source
	CDoubleArray *output_ndvs					// output array of ndvs
)
{
	GPOS_ASSERT(NULL != grouping_columns);
	GPOS_ASSERT(NULL != input_stats);
	GPOS_ASSERT(NULL != output_ndvs);

	const ULONG num_cols = grouping_columns->Size();
	// iterate over grouping columns
	for (ULONG i = 0; i < num_cols; i++)
	{
		ULONG col_id = (*(*grouping_columns)[i]);

		CDouble distinct_vals = CStatisticsUtils::DefaultDistinctVals(input_stats->Rows());
		const CHistogram *histogram = input_stats->GetHistogram(col_id);
		if (NULL != histogram)
		{
			distinct_vals = histogram->GetNumDistinct();
			if (histogram->IsEmpty())
			{
				distinct_vals = DefaultDistinctVals(input_stats->Rows());
			}
		}
		output_ndvs->Append(GPOS_NEW(memory_pool) CDouble(distinct_vals));
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::ExtractNDVForGrpCols
//
//	@doc:
//		Extract NDVs for the given array of grouping columns. If one or more
//      of the grouping columns' NDV have been capped, table has been filtered,
//      then we add the maximum NDV only for computing the cardinality of
//      the group by. The logic is that number of groups cannot be greater
//		than the card of filter table. Else we add the NDVs for all grouping
//      columns as is.
//---------------------------------------------------------------------------
CDoubleArray *
CStatisticsUtils::ExtractNDVForGrpCols(IMemoryPool *memory_pool,
									   const CStatisticsConfig *stats_config,
									   const IStatistics *stats,
									   CColRefSet *grp_cols_refset,
									   CBitSet *keys  // keys derived during optimization
)
{
	GPOS_ASSERT(NULL != stats);
	GPOS_ASSERT(NULL != grp_cols_refset);

	CStatistics *input_stats = CStatistics::CastStats(const_cast<IStatistics *>(stats));

	CDoubleArray *ndvs = GPOS_NEW(memory_pool) CDoubleArray(memory_pool);

	UlongUlongArrayHashMap *grp_colid_upper_bound_ndv_idx_map =
		GetGrpColIdToUpperBoundNDVIdxMap(memory_pool, input_stats, grp_cols_refset, keys);
	UlongUlongArrayHashMapIter map_iter(grp_colid_upper_bound_ndv_idx_map);
	while (map_iter.Advance())
	{
		ULONG source_id = *(map_iter.Key());
		const ULongPtrArray *src_grouping_cols = map_iter.Value();

		if (gpos::ulong_max == source_id)
		{
			// this array of grouping columns represents computed columns.
			// Since we currently do not cap computed columns, we add all of their NDVs as is
			AddNdvForAllGrpCols(memory_pool, input_stats, src_grouping_cols, ndvs);
		}
		else
		{
			// compute the maximum number of groups when aggregated on columns from the given source
			CDouble max_grps_per_src = MaxNumGroupsForGivenSrcGprCols(
				memory_pool, stats_config, input_stats, src_grouping_cols);
			ndvs->Append(GPOS_NEW(memory_pool) CDouble(max_grps_per_src));
		}
	}

	// clean up
	grp_colid_upper_bound_ndv_idx_map->Release();

	return ndvs;
}

//---------------------------------------------------------------------------
//	@function:
//       CStatisticsUtils::CappedGrpColExists
//
//  @doc:
//       Check to see if any one of the grouping columns has been capped
//---------------------------------------------------------------------------
BOOL
CStatisticsUtils::CappedGrpColExists(const CStatistics *stats,
									 const ULongPtrArray *grouping_columns)
{
	GPOS_ASSERT(NULL != stats);
	GPOS_ASSERT(NULL != grouping_columns);

	const ULONG num_cols = grouping_columns->Size();
	for (ULONG i = 0; i < num_cols; i++)
	{
		ULONG col_id = (*(*grouping_columns)[i]);
		const CHistogram *histogram = stats->GetHistogram(col_id);

		if (NULL != histogram && histogram->WereNDVsScaled())
		{
			return true;
		}
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//      CStatisticsUtils::MaxNdv
//
//  @doc:
//      Return the maximum NDV given an array of grouping columns
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::MaxNdv(const CStatistics *stats, const ULongPtrArray *grouping_columns)
{
	GPOS_ASSERT(NULL != stats);
	GPOS_ASSERT(NULL != grouping_columns);

	const ULONG num_grp_cols = grouping_columns->Size();

	CDouble ndv_max(1.0);
	for (ULONG i = 0; i < num_grp_cols; i++)
	{
		CDouble ndv = CStatisticsUtils::DefaultDistinctVals(stats->Rows());

		ULONG col_id = (*(*grouping_columns)[i]);
		const CHistogram *histogram = stats->GetHistogram(col_id);
		if (NULL != histogram)
		{
			ndv = histogram->GetNumDistinct();
			if (histogram->IsEmpty())
			{
				ndv = CStatisticsUtils::DefaultDistinctVals(stats->Rows());
			}
		}

		if (ndv_max < ndv)
		{
			ndv_max = ndv;
		}
	}

	return ndv_max;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::MaxNumGroupsForGivenSrcGprCols
//
//	@doc:
//		Compute max number of groups when grouping on columns from the given source
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::MaxNumGroupsForGivenSrcGprCols(IMemoryPool *memory_pool,
												 const CStatisticsConfig *stats_config,
												 CStatistics *input_stats,
												 const ULongPtrArray *src_grouping_cols)
{
	GPOS_ASSERT(NULL != input_stats);
	GPOS_ASSERT(NULL != src_grouping_cols);
	GPOS_ASSERT(0 < src_grouping_cols->Size());

	CDouble input_rows = input_stats->Rows();

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CColRef *first_colref = col_factory->LookupColRef(*(*src_grouping_cols)[0]);
	CDouble upper_bound_ndvs = input_stats->GetColUpperBoundNDVs(first_colref);

	CDoubleArray *ndvs = GPOS_NEW(memory_pool) CDoubleArray(memory_pool);
	AddNdvForAllGrpCols(memory_pool, input_stats, src_grouping_cols, ndvs);

	// take the minimum of (a) the estimated number of groups from the columns of this source,
	// (b) input rows, and (c) cardinality upper bound for the given source in the
	// input statistics object

	// DNumOfDistVal internally damps the number of columns with our formula.
	// (a) For columns from the same table, they will be damped based on the formula in DNumOfDistVal
	// (b) If the group by has columns from multiple tables, they will again be damped by the formula
	// in DNumOfDistVal when we compute the final group by cardinality
	CDouble groups =
		std::min(std::max(CStatistics::MinRows.Get(), GetCumulativeNDVs(stats_config, ndvs).Get()),
				 std::min(input_rows.Get(), upper_bound_ndvs.Get()));
	ndvs->Release();

	return groups;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::Groups
//
//	@doc:
//		Compute the cumulative number of groups for the given set of grouping columns
//
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::Groups(IMemoryPool *memory_pool,
						 IStatistics *stats,
						 const CStatisticsConfig *stats_config,
						 ULongPtrArray *grouping_cols,
						 CBitSet *keys  // keys derived during optimization
)
{
	GPOS_ASSERT(NULL != stats);
	GPOS_ASSERT(NULL != stats_config);
	GPOS_ASSERT(NULL != grouping_cols);

	CColRefSet *computed_groupby_cols = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	CColRefSet *grp_col_for_stats =
		MakeGroupByColsForStats(memory_pool, grouping_cols, computed_groupby_cols);

	CDoubleArray *ndvs =
		ExtractNDVForGrpCols(memory_pool, stats_config, stats, grp_col_for_stats, keys);
	CDouble groups =
		std::min(std::max(CStatistics::MinRows.Get(), GetCumulativeNDVs(stats_config, ndvs).Get()),
				 stats->Rows().Get());

	// clean up
	grp_col_for_stats->Release();
	computed_groupby_cols->Release();
	ndvs->Release();

	return groups;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::GetCumulativeNDVs
//
//	@doc:
//		Compute the total number of groups of the group by operator
//		from the array of NDVs of the individual grouping columns
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::GetCumulativeNDVs(const CStatisticsConfig *stats_config, CDoubleArray *ndvs)
{
	GPOS_ASSERT(NULL != stats_config);
	GPOS_ASSERT(NULL != ndvs);

	CScaleFactorUtils::SortScalingFactor(ndvs, true /* fDescending */);
	const ULONG ndv_size = ndvs->Size();

	if (0 == ndv_size)
	{
		return CDouble(1.0);
	}

	CDouble cumulative_ndv = *(*ndvs)[0];
	for (ULONG idx = 1; idx < ndv_size; idx++)
	{
		CDouble ndv = *(*ndvs)[idx];
		CDouble ndv_damped =
			std::max(CHistogram::MinDistinct.Get(),
					 (ndv * CScaleFactorUtils::DampedGroupByScaleFactor(stats_config, idx)).Get());
		cumulative_ndv = cumulative_ndv * ndv_damped;
	}

	return cumulative_ndv;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::AddGrpColStats
//
//	@doc:
//		Add the statistics (histogram and width) of the grouping columns
//---------------------------------------------------------------------------
void
CStatisticsUtils::AddGrpColStats(IMemoryPool *memory_pool,
								 const CStatistics *input_stats,
								 CColRefSet *grp_cols_refset,
								 UlongHistogramHashMap *output_histograms,
								 UlongDoubleHashMap *output_col_widths)
{
	GPOS_ASSERT(NULL != input_stats);
	GPOS_ASSERT(NULL != grp_cols_refset);
	GPOS_ASSERT(NULL != output_histograms);
	GPOS_ASSERT(NULL != output_col_widths);

	// iterate over grouping columns
	CColRefSetIter grp_col_refset_iter(*grp_cols_refset);
	while (grp_col_refset_iter.Advance())
	{
		CColRef *colref = grp_col_refset_iter.Pcr();
		ULONG grp_col_id = colref->Id();

		CDouble num_distinct_vals(CHistogram::MinDistinct);
		const CHistogram *histogram = input_stats->GetHistogram(grp_col_id);
		if (NULL != histogram)
		{
			CHistogram *result_histogram = histogram->MakeGroupByHistogramNormalize(
				memory_pool, input_stats->Rows(), &num_distinct_vals);
			if (histogram->WereNDVsScaled())
			{
				result_histogram->SetNDVScaled();
			}
			AddHistogram(memory_pool, grp_col_id, result_histogram, output_histograms);
			GPOS_DELETE(result_histogram);
		}

		const CDouble *width = input_stats->GetWidth(grp_col_id);
		if (NULL != width)
		{
			output_col_widths->Insert(GPOS_NEW(memory_pool) ULONG(grp_col_id),
									  GPOS_NEW(memory_pool) CDouble(*width));
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::MakeGroupByColsForStats
//
//	@doc:
//		Return the set of grouping columns for statistics computation.
//		If the grouping column (c) is a computed column, for example c = f(a,b),
//		then we include columns a and b as the grouping column instead of c.
//---------------------------------------------------------------------------
CColRefSet *
CStatisticsUtils::MakeGroupByColsForStats(IMemoryPool *memory_pool,
										  const ULongPtrArray *grouping_columns,
										  CColRefSet *computed_groupby_cols)
{
	GPOS_ASSERT(NULL != grouping_columns);
	GPOS_ASSERT(NULL != computed_groupby_cols);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	CColRefSet *grp_col_for_stats = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	const ULONG ulGrpCols = grouping_columns->Size();

	// iterate over grouping columns
	for (ULONG i = 0; i < ulGrpCols; i++)
	{
		ULONG col_id = *(*grouping_columns)[i];
		CColRef *grp_col_ref = col_factory->LookupColRef(col_id);
		GPOS_ASSERT(NULL != grp_col_ref);

		// check to see if the grouping column is a computed attribute
		const CColRefSet *used_col_refset = col_factory->PcrsUsedInComputedCol(grp_col_ref);
		if (NULL == used_col_refset || 0 == used_col_refset->Size())
		{
			(void) grp_col_for_stats->Include(grp_col_ref);
		}
		else
		{
			(void) grp_col_for_stats->Union(used_col_refset);
			(void) computed_groupby_cols->Include(grp_col_ref);
		}
	}

	return grp_col_for_stats;
}



//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::GetNumDistinct
//
//	@doc:
//		Sum of number of distinct values in the given array of buckets
//
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::GetNumDistinct(const BucketArray *histogram_buckets)
{
	GPOS_ASSERT(NULL != histogram_buckets);

	CDouble distinct = CDouble(0.0);
	const ULONG num_of_buckets = histogram_buckets->Size();
	for (ULONG bucket_index = 0; bucket_index < num_of_buckets; bucket_index++)
	{
		CBucket *bucket = (*histogram_buckets)[bucket_index];
		distinct = distinct + bucket->GetNumDistinct();
	}

	return distinct;
}


//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::GetFrequency
//
//	@doc:
//		Sum of the frequencies of buckets in the given array of buckets
//
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::GetFrequency(const BucketArray *histogram_buckets)
{
	GPOS_ASSERT(NULL != histogram_buckets);

	CDouble freq = CDouble(0.0);
	const ULONG num_of_buckets = histogram_buckets->Size();
	for (ULONG bucket_index = 0; bucket_index < num_of_buckets; bucket_index++)
	{
		CBucket *bucket = (*histogram_buckets)[bucket_index];
		freq = freq + bucket->GetFrequency();
	}

	return freq;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatisticsUtils::FIncreasesRisk
//
//	@doc:
//		Return true if the given operator increases the risk of cardinality
//		misestimation.
//
//---------------------------------------------------------------------------

BOOL
CStatisticsUtils::IncreasesRisk(CLogical *logical_op)
{
	if (logical_op->FSelectionOp())
	{
		// operators that perform filters (e.g. joins, select) increase the risk
		return true;
	}

	static COperator::EOperatorId grouping_and_semi_join_opid_array[] = {
		COperator::EopLogicalGbAgg,
		COperator::EopLogicalIntersect,
		COperator::EopLogicalIntersectAll,
		COperator::EopLogicalUnion,
		COperator::EopLogicalDifference,
		COperator::EopLogicalDifferenceAll};

	COperator::EOperatorId operator_id = logical_op->Eopid();
	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(grouping_and_semi_join_opid_array); i++)
	{
		if (grouping_and_semi_join_opid_array[i] == operator_id)
		{
			return true;
		}
	}

	return false;
}


//---------------------------------------------------------------------------
//     @function:
//             CStatisticsUtils::DefaultColumnWidth
//
//     @doc:
//             Return the default column width
//
//---------------------------------------------------------------------------
CDouble
CStatisticsUtils::DefaultColumnWidth(const IMDType *mdtype)
{
	CDouble width(CStatistics::DefaultColumnWidth);
	if (mdtype->IsFixedLength())
	{
		width = CDouble(mdtype->Length());
	}

	return width;
}

//	add width information
void
CStatisticsUtils::AddWidthInfo(IMemoryPool *memory_pool,
							   UlongDoubleHashMap *src_width,
							   UlongDoubleHashMap *dest_width)
{
	UlongDoubleHashMapIter col_width_map_iterator(src_width);
	while (col_width_map_iterator.Advance())
	{
		ULONG col_id = *(col_width_map_iterator.Key());
		BOOL is_present = (NULL != dest_width->Find(&col_id));
		if (!is_present)
		{
			const CDouble *width = col_width_map_iterator.Value();
			CDouble *width_copy = GPOS_NEW(memory_pool) CDouble(*width);
			dest_width->Insert(GPOS_NEW(memory_pool) ULONG(col_id), width_copy);
		}

		GPOS_CHECK_ABORT;
	}
}


//	for the output statistics object, compute its upper bound cardinality
// 	mapping based on the bounding method estimated output cardinality
//  and information maintained in the current statistics object
void
CStatisticsUtils::ComputeCardUpperBounds(
	IMemoryPool *memory_pool,
	const CStatistics *input_stats,
	CStatistics *output_stats,  // output statistics object that is to be updated
	CDouble rows_output,		// estimated output cardinality of the operator
	CStatistics::ECardBoundingMethod
		card_bounding_method  // technique used to estimate max source cardinality in the output stats object
)
{
	GPOS_ASSERT(NULL != output_stats);
	GPOS_ASSERT(CStatistics::EcbmSentinel != card_bounding_method);

	const UpperBoundNDVPtrArray *input_stats_ndv_upper_bound = input_stats->GetUpperBoundNDVs();
	ULONG length = input_stats_ndv_upper_bound->Size();
	for (ULONG i = 0; i < length; i++)
	{
		const CUpperBoundNDVs *upper_bound_NDVs = (*input_stats_ndv_upper_bound)[i];
		CDouble upper_bound_ndv_input = upper_bound_NDVs->UpperBoundNDVs();
		CDouble upper_bound_ndv_output = rows_output;
		if (CStatistics::EcbmInputSourceMaxCard == card_bounding_method)
		{
			upper_bound_ndv_output = upper_bound_ndv_input;
		}
		else if (CStatistics::EcbmMin == card_bounding_method)
		{
			upper_bound_ndv_output = std::min(upper_bound_ndv_input.Get(), rows_output.Get());
		}

		CUpperBoundNDVs *upper_bound_NDVs_copy =
			upper_bound_NDVs->CopyUpperBoundNDVs(memory_pool, upper_bound_ndv_output);
		output_stats->AddCardUpperBound(upper_bound_NDVs_copy);
	}
}

// EOF
