//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2018 Pivotal, Inc.
//
//	@filename:
//		CStatisticsUtils.h
//
//	@doc:
//		Utility functions for statistics
//---------------------------------------------------------------------------
#ifndef GPOPT_CStatisticsUtils_H
#define GPOPT_CStatisticsUtils_H

#include "gpos/base.h"
#include "gpopt/base/CColRef.h"
#include "gpopt/base/CPartFilterMap.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CLogical.h"
#include "gpopt/operators/CScalarBoolOp.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/engine/CStatisticsConfig.h"

#include "naucrates/statistics/CStatsPredUtils.h"
#include "naucrates/statistics/CStatsPredDisj.h"
#include "naucrates/statistics/CStatsPredUnsupported.h"

#include "naucrates/base/IDatum.h"

namespace gpopt
{
	class CTableDescriptor;
}

namespace gpnaucrates
{
	using namespace gpos;
	using namespace gpopt;

	//---------------------------------------------------------------------------
	//	@class:
	//		CStatisticsUtils
	//
	//	@doc:
	//		Utility functions for statistics
	//
	//---------------------------------------------------------------------------
	class CStatisticsUtils
	{
	private:
		// MCV m_bytearray_value and its corresponding frequency, used for sorting MCVs
		struct SMcvPair
		{
			// MCV datum
			IDatum *m_datum_mcv;

			// the frequency of this MCV
			CDouble m_mcv_freq;

			// ctor
			SMcvPair(IDatum *datum_mcv, CDouble mcv_freq)
				: m_datum_mcv(datum_mcv), m_mcv_freq(mcv_freq)
			{
			}

			// dtor
			~SMcvPair()
			{
				m_datum_mcv->Release();
			}
		};

		// array of SMcvVPairs
		typedef CDynamicPtrArray<SMcvPair, CleanupDelete> McvPairPtrArray;

		// private ctor
		CStatisticsUtils();

		// private dtor
		virtual ~CStatisticsUtils();

		// private copy ctor
		CStatisticsUtils(const CStatisticsUtils &);

		// given MCVs and histogram buckets, merge them into buckets of a single histogram
		static BucketArray *MergeMcvHistBucket(IMemoryPool *memory_pool,
											   const BucketArray *mcv_buckets,
											   const BucketArray *histogram_buckets);

		// split a histogram bucket given an MCV bucket
		static BucketArray *SplitHistBucketGivenMcvBuckets(IMemoryPool *memory_pool,
														   const CBucket *histogram_bucket,
														   const BucketArray *mcv_buckets);

		// given lower and upper bound information and their closedness, return a bucket if they can form a valid bucket
		static CBucket *CreateValidBucket(IMemoryPool *memory_pool,
										  CPoint *bucket_lower_bound,
										  CPoint *bucket_upper_bound,
										  BOOL is_lower_closed,
										  BOOL is_upper_closed);

		// given lower and upper bound information and their closedness, test if they can form a valid bucket
		static BOOL IsValidBucket(CPoint *bucket_lower_bound,
								  CPoint *bucket_upper_bound,
								  BOOL is_lower_closed,
								  BOOL is_upper_closed);

		// find the MCVs that fall within the same histogram bucket and perform the split
		static void SplitHistDriver(IMemoryPool *memory_pool,
									const CBucket *histogram_bucket,
									const BucketArray *mcv_buckets,
									BucketArray *merged_buckets,
									ULONG *mcv_index,
									ULONG mcv);

		// distribute total distinct and frequency of the histogram bucket into the new buckets
		static void DistributeBucketProperties(CDouble total_frequency,
											   CDouble total_distinct_values,
											   BucketArray *buckets);

		// add the NDVs for all of the grouping columns
		static void AddNdvForAllGrpCols(IMemoryPool *memory_pool,
										const CStatistics *input_stats,
										const ULongPtrArray *grouping_columns,
										CDoubleArray *output_ndvs  // output array of NDV
		);

		// compute max number of groups when grouping on columns from the given source
		static CDouble MaxNumGroupsForGivenSrcGprCols(IMemoryPool *memory_pool,
													  const CStatisticsConfig *stats_config,
													  CStatistics *input_stats,
													  const ULongPtrArray *src_grouping_cols);

		// check to see if any one of the grouping columns has been capped
		static BOOL CappedGrpColExists(const CStatistics *stats,
									   const ULongPtrArray *grouping_columns);

		// return the maximum NDV given an array of grouping columns
		static CDouble MaxNdv(const CStatistics *stats, const ULongPtrArray *grouping_columns);

	public:
		// get the next data point for generating new bucket boundary
		static CPoint *NextPoint(IMemoryPool *memory_pool, CMDAccessor *md_accessor, CPoint *point);

		// transform mcv information to optimizer's histogram structure
		static CHistogram *TransformMCVToHist(IMemoryPool *memory_pool,
											  const IMDType *mdtype,
											  IDatumArray *mcv_datums,
											  CDoubleArray *freq_array,
											  ULONG num_mcv_values);

		// merge MCVs and histogram
		static CHistogram *MergeMCVHist(IMemoryPool *memory_pool,
										const CHistogram *mcv_histogram,
										const CHistogram *histogram);

		// comparison function for sorting MCVs
		static inline INT GetMcvPairCmpFunc(const void *val1, const void *val2);

		// utility function to print column stats before/after applying filter
		static void PrintColStats(IMemoryPool *memory_pool,
								  CStatsPred *pred_stats,
								  ULONG cond_col_id,
								  CHistogram *histogram,
								  CDouble last_scale_factor,
								  BOOL is_filter_applied_before);

		// extract all the column identifiers used in the statistics filter
		static void ExtractUsedColIds(IMemoryPool *memory_pool,
									  CBitSet *col_ids_bitset,
									  CStatsPred *pred_stats,
									  ULongPtrArray *col_ids);

		// given the previously generated histogram, update the intermediate
		// result of the disjunction
		static void UpdateDisjStatistics(IMemoryPool *memory_pool,
										 CBitSet *non_updatable_cols,
										 CDouble input_disjunct_rows,
										 CDouble local_rows,
										 CHistogram *previous_histogram,
										 UlongHistogramHashMap *disjunctive_result_histograms,
										 ULONG last_col_id);

		// given a disjunction filter, generate a bit set of columns whose
		// histogram buckets cannot be changed by applying the predicates in the
		// disjunction
		static CBitSet *GetColsNonUpdatableHistForDisj(IMemoryPool *memory_pool,
													   CStatsPredDisj *pred_stats);

		// helper method to add a histogram to a map
		static void AddHistogram(IMemoryPool *memory_pool,
								 ULONG col_id,
								 const CHistogram *histogram,
								 UlongHistogramHashMap *col_histogram_mapping,
								 BOOL replace_old = false);

		// create a new hash map of histograms after merging
		// the histograms generated by the child of disjunctive predicate
		static UlongHistogramHashMap *CreateHistHashMapAfterMergingDisjPreds(
			IMemoryPool *memory_pool,
			CBitSet *non_updatable_cols,
			UlongHistogramHashMap *prev_histogram_map,
			UlongHistogramHashMap *disj_preds_histogram_map,
			CDouble cummulative_rows,
			CDouble num_rows_disj_child);

		// helper method to copy the hash map of histograms
		static UlongHistogramHashMap *CopyHistHashMap(IMemoryPool *memory_pool,
													  UlongHistogramHashMap *col_histogram_mapping);

		// return the column identifier of the filter if the predicate is
		// on a single column else	return gpos::ulong_max
		static ULONG GetColId(const StatsPredPtrArry *stats_preds_array);

		// add remaining buckets from one array of buckets to the other
		static void AddRemainingBuckets(IMemoryPool *memory_pool,
										const BucketArray *src_buckets,
										BucketArray *dest_buckets,
										ULONG *start_val);

		// generate a null datum with the type of passed colref
		static IDatum *DatumNull(const CColRef *colref);

#ifdef GPOS_DEBUG
		// helper method to print the hash map of histograms
		static void PrintHistogramMap(IOstream &os, UlongHistogramHashMap *col_histogram_mapping);
#endif  // GPOS_DEBUG

		// derive statistics of dynamic scan based on part-selector stats in the given map
		static IStatistics *DeriveStatsForDynamicScan(IMemoryPool *memory_pool,
													  CExpressionHandle &expr_handle,
													  ULONG part_idx_id,
													  CPartFilterMap *part_filter_map);

		// derive statistics of (dynamic) index-get
		static IStatistics *DeriveStatsForIndexGet(IMemoryPool *memory_pool,
												   CExpressionHandle &exprhdl,
												   StatsArray *stats_contexts);

		// derive statistics of bitmap table-get
		static IStatistics *DeriveStatsForBitmapTableGet(IMemoryPool *memory_pool,
														 CExpressionHandle &exprhdl,
														 StatsArray *stats_contexts);

		// compute the cumulative number of distinct values (NDV) of the group by operator
		// from the array of NDV of the individual grouping columns
		static CDouble GetCumulativeNDVs(const CStatisticsConfig *stats_config,
										 CDoubleArray *ndv_array);

		// return the mapping between the table column used for grouping to the logical operator id where it was defined.
		// If the grouping column is not a table column then the logical op id is initialized to gpos::ulong_max
		static UlongUlongArrayHashMap *GetGrpColIdToUpperBoundNDVIdxMap(
			IMemoryPool *memory_pool,
			CStatistics *stats,
			const CColRefSet *grouping_cols_refset,
			CBitSet *keys);

		// extract NDVs for the given array of grouping columns
		static CDoubleArray *ExtractNDVForGrpCols(IMemoryPool *memory_pool,
												const CStatisticsConfig *stats_config,
												const IStatistics *stats,
												CColRefSet *grp_cols_refset,  // grouping columns
												CBitSet *keys  // keys derived during optimization
		);

		// compute the cumulative number of groups for the given set of grouping columns
		static CDouble Groups(IMemoryPool *memory_pool,
							  IStatistics *stats,
							  const CStatisticsConfig *stats_config,
							  ULongPtrArray *grouping_cols,
							  CBitSet *keys);

		// return the default number of distinct values
		static CDouble
		DefaultDistinctVals(CDouble rows)
		{
			return std::min(CStatistics::DefaultDistinctValues.Get(), rows.Get());
		}

		// add the statistics (histogram and width) of the grouping columns
		static void AddGrpColStats(IMemoryPool *memory_pool,
								   const CStatistics *input_stats,
								   CColRefSet *grp_cols_refset,
								   UlongHistogramHashMap *output_histograms,
								   UlongDoubleHashMap *output_col_widths);

		// return the set of grouping columns for statistics computation;
		static CColRefSet *MakeGroupByColsForStats(
			IMemoryPool *memory_pool,
			const ULongPtrArray *grouping_columns,
			CColRefSet *
				computed_groupby_cols  // output set of grouping columns that are computed attributes
		);



		// return the total number of distinct values in the given array of buckets
		static CDouble GetNumDistinct(const BucketArray *histogram_buckets);

		// return the cumulative frequency in the given array of buckets
		static CDouble GetFrequency(const BucketArray *histogram_buckets);

		// true if the given operator increases risk of cardinality misestimation
		static BOOL IncreasesRisk(CLogical *logical_op);

		// return the default column width
		static CDouble DefaultColumnWidth(const IMDType *mdtype);

		// helper method to add width information
		static void AddWidthInfo(IMemoryPool *memory_pool,
								 UlongDoubleHashMap *src_width,
								 UlongDoubleHashMap *dest_width);


		// for the output stats object, compute its upper bound cardinality mapping based on the bounding method
		// estimated output cardinality and information maintained in the current stats object
		static void ComputeCardUpperBounds(
			IMemoryPool *memory_pool,  // memory pool
			const CStatistics *input_stats,
			CStatistics *output_stats,  // output statistics object that is to be updated
			CDouble rows_output,		// estimated output cardinality of the operator
			CStatistics::ECardBoundingMethod
				card_bounding_method  // technique used to estimate max source cardinality in the output stats object
		);

	};  // class CStatisticsUtils

	// comparison function for sorting MCVs
	INT
	CStatisticsUtils::GetMcvPairCmpFunc(const void *val1, const void *val2)
	{
		GPOS_ASSERT(NULL != val1);
		GPOS_ASSERT(NULL != val2);
		const SMcvPair *mcv_pair1 = *(const SMcvPair **) (val1);
		const SMcvPair *mcv_pair2 = *(const SMcvPair **) (val2);

		const IDatum *datum1 = mcv_pair1->m_datum_mcv;
		const IDatum *datum2 = mcv_pair2->m_datum_mcv;

		if (datum1->StatsAreEqual(datum2))
		{
			return 0;
		}

		if (datum1->StatsAreComparable(datum2) && datum1->StatsAreLessThan(datum2))
		{
			return -1;
		}

		return 1;
	}
}  // namespace gpnaucrates


#endif  // !GPOPT_CStatisticsUtils_H

// EOF
