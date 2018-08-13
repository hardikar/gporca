//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		IStatistics.h
//
//	@doc:
//		Abstract statistics API
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_IStatistics_H
#define GPNAUCRATES_IStatistics_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"
#include "gpos/common/CHashMapIter.h"

#include "naucrates/statistics/CStatsPred.h"
#include "naucrates/statistics/CStatsPredPoint.h"
#include "naucrates/statistics/CStatsPredJoin.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/md/CDXLStatsDerivedRelation.h"

#include "gpopt/base/CColRef.h"

namespace gpopt
{
	class CMDAccessor;
	class CReqdPropRelational;
	class CColRefSet;
}

namespace gpnaucrates
{
	using namespace gpos;
	using namespace gpmd;
	using namespace gpopt;

	// fwd declarations
	class IStatistics;

	// hash map from column id to a histogram
	typedef CHashMap<ULONG, CHistogram, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupDelete<CHistogram> > UlongHistogramHashMap;

	// iterator
	typedef CHashMapIter<ULONG, CHistogram, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupDelete<CHistogram> > UlongHistogramHashMapIter;

	// hash map from column ULONG to CDouble
	typedef CHashMap<ULONG, CDouble, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupDelete<CDouble> > UlongDoubleHashMap;

	// iterator
	typedef CHashMapIter<ULONG, CDouble, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupDelete<CDouble> > UlongDoubleHashMapIter;

	typedef CHashMap<ULONG, ULONG, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupDelete<ULONG> > UlongUlongHashMap;

	// hash maps mapping INT -> ULONG
	typedef CHashMap<INT, ULONG, gpos::HashValue<INT>, gpos::Equals<INT>,
					CleanupDelete<INT>, CleanupDelete<ULONG> > IntUlongHashMap;

	//---------------------------------------------------------------------------
	//	@class:
	//		IStatistics
	//
	//	@doc:
	//		Abstract statistics API
	//
	//---------------------------------------------------------------------------
	class IStatistics: public CRefCount
	{
		private:

			// private copy ctor
			IStatistics(const IStatistics &);

			// private assignment operator
			IStatistics& operator=(IStatistics &);

		public:

			enum EStatsJoinType
					{
					EsjtInnerJoin,
					EsjtLeftOuterJoin,
					EsjtLeftSemiJoin,
					EsjtLeftAntiSemiJoin,
					EstiSentinel // should be the last in this enum
					};

			// ctor
			IStatistics()
			{}

			// dtor
			virtual
			~IStatistics()
			{}

			// how many rows
			virtual
			CDouble Rows() const = 0;

			// is statistics on an empty input
			virtual
			BOOL IsEmpty() const = 0;

			// statistics could be computed using predicates with external parameters (outer references),
			// this is the total number of external parameters' values
			virtual
			CDouble NumRebinds() const = 0;

			// skew estimate for given column
			virtual
			CDouble GetSkew(ULONG col_id) const = 0;

			// what is the width in bytes
			virtual
			CDouble Width() const = 0;

			// what is the width in bytes of set of column id's
			virtual
			CDouble Width(ULongPtrArray *col_ids) const = 0;

			// what is the width in bytes of set of column references
			virtual
			CDouble Width(IMemoryPool *mp, CColRefSet *colrefs) const = 0;

			// the risk of errors in cardinality estimation
			virtual
			ULONG StatsEstimationRisk() const = 0;

			// update the risk of errors in cardinality estimation
			virtual
			void SetStatsEstimationRisk(ULONG risk) = 0;

			// look up the number of distinct values of a particular column
			virtual
			CDouble GetNDVs(const CColRef *colref) = 0;

			virtual
			ULONG GetNumberOfPredicates() const = 0;

			// inner join with another stats structure
			virtual
			IStatistics *CalcInnerJoinStats
						(
						IMemoryPool *mp,
						const IStatistics *other_stats,
						StatsPredJoinArray *join_preds_stats
						)
						const = 0;

			// LOJ with another stats structure
			virtual
			IStatistics *CalcLOJoinStats
						(
						IMemoryPool *mp,
						const IStatistics *other_stats,
						StatsPredJoinArray *join_preds_stats
						)
						const = 0;

			// semi join stats computation
			virtual
			IStatistics *CalcLSJoinStats
						(
						IMemoryPool *mp,
						const IStatistics *inner_side_stats,
						StatsPredJoinArray *join_preds_stats
						)
						const = 0;

			// anti semi join
			virtual
			IStatistics *CalcLASJoinStats
						(
						IMemoryPool *mp,
						const IStatistics *other_stats,
						StatsPredJoinArray *join_preds_stats,
						BOOL DoIgnoreLASJHistComputation
						)
						const = 0;

			// return required props associated with stats object
			virtual
			CReqdPropRelational *GetReqdRelationalProps(IMemoryPool *mp) const = 0;

			// append given stats to current object
			virtual
			void AppendStats(IMemoryPool *mp, IStatistics *stats) = 0;

			// set number of rebinds
			virtual
			void SetRebinds(CDouble num_rebinds) = 0;

			// copy stats
			virtual
			IStatistics *CopyStats(IMemoryPool *mp) const = 0;

			// return a copy of this stats object scaled by a given factor
			virtual
			IStatistics *ScaleStats(IMemoryPool *mp, CDouble factor) const = 0;

			// copy stats with remapped column ids
			virtual
			IStatistics *CopyStatsWithRemap(IMemoryPool *mp, UlongColRefHashMap *colref_mapping, BOOL must_exist = true) const = 0;

			// return a set of column references we have stats for
			virtual
			CColRefSet *GetColRefSet(IMemoryPool *mp) const = 0;

			// print function
			virtual
			IOstream &OsPrint(IOstream &os) const = 0;

			// generate the DXL representation of the statistics object
			virtual
			CDXLStatsDerivedRelation *GetDxlStatsDrvdRelation(IMemoryPool *mp, CMDAccessor *md_accessor) const = 0;

			// is the join type either a left semi join or left anti-semi join
			static
			BOOL IsSemiJoin
					(
					IStatistics::EStatsJoinType join_type
					)
			{
				return (IStatistics::EsjtLeftAntiSemiJoin == join_type) || (IStatistics::EsjtLeftSemiJoin == join_type);
			}
	}; // class IStatistics

	// shorthand for printing
	inline
	IOstream &operator << (IOstream &os, IStatistics &stats)
	{
		return stats.OsPrint(os);
	}
	// release istats
	inline void CleanupStats(IStatistics *stats)
	{
		if (NULL != stats)
		{
			(dynamic_cast<CRefCount*>(stats))->Release();
		}
	}

	// dynamic array for derived stats
	typedef CDynamicPtrArray<IStatistics, CleanupStats> StatsArray;
}

#endif // !GPNAUCRATES_IStatistics_H

// EOF
