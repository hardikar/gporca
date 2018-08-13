//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CLeftSemiJoinStatsProcessor.h
//
//	@doc:
//		Processor for computing statistics for Left Semi Join
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CLeftSemiJoinStatsProcessor_H
#define GPNAUCRATES_CLeftSemiJoinStatsProcessor_H

#include "naucrates/statistics/CJoinStatsProcessor.h"

namespace gpnaucrates
{
	class CLeftSemiJoinStatsProcessor : public CJoinStatsProcessor
	{
		public:
			static
			CStatistics *CalcLSJoinStatsStatic
					(
					IMemoryPool *mp,
					const IStatistics *outer_stats,
					const IStatistics *inner_side_stats,
					StatsPredJoinArray *join_preds_stats
					);
	};
}

#endif // !GPNAUCRATES_CLeftSemiJoinStatsProcessor_H

// EOF

