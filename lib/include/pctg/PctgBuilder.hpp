/*!
 * \file PctgBuilder.hpp
 * \brief Definition of PctgBuilder class.
 * \details This file contains the definition of the class that implements the
 * construction of a paired contig.
 */

#ifndef PCTGBUILDER_HPP
#define	PCTGBUILDER_HPP

#include <iostream>
#include <stdexcept>
#include <pthread.h>

#include "api/BamAux.h"
#include "api/BamMultiReader.h"

#include "types.hpp"
#include "alignment/my_alignment.hpp"
#include "assembly/Block.hpp"
#include "assembly/RefSequence.hpp"
#include "pctg/BestCtgAlignment.hpp"
#include "pctg/ContigInPctgInfo.hpp"
#include "pctg/CtgInPctgInfo.hpp"
#include "pctg/PairedContig.hpp"
#include "pctg/constraints_disattended.hpp"
#include "pctg/ThreadedBuildPctg.hpp"
#include "PartitionFunctions.hpp"

#include "graphs/CompactAssemblyGraph.hpp"
#include "pctg/MergeDescriptor.hpp"

#ifndef MIN_HOMOLOGY
#define MIN_HOMOLOGY 95.0
#endif

#ifndef MIN_HOMOLOGY_2
#define MIN_HOMOLOGY_2 90.0
#endif

#ifndef MIN_ALIGNMENT_LEN
#define MIN_ALIGNMENT_LEN 100
#endif

#define DEFAULT_MAX_GAPS 300
#define DEFAULT_MAX_SEARCHED_ALIGNMENT 400000

#ifndef MIN_ALIGNMENT_QUOTIENT
#define MIN_ALIGNMENT_QUOTIENT 0.001
#endif


//! Class implementing a builder of paired contigs.
class PctgBuilder
{

private:
	ThreadedBuildPctg *_tbp;

	const RefSequence *_masterRef;        //!< Reference to the id->name vector of the master contigs.
	const RefSequence *_slaveRef;         //!< Reference to the id->name vector of the slave contigs.

    UIntType _maxAlignment;                             //!< Maximum alignment size
    UIntType _maxPctgGap;                               //!< Maximum paired contig gaps
    UIntType _maxCtgGap;                                //!< Maximum contig gaps

public:
    //! A constructor.
    /*!
     * Creates a paired contig builder.
     * \param masterPool reference to the master contigs pool
     * \param slavePool reference to the slave contigs pool
     * \param masterRefVector reference to the vector id->name of master contigs
     * \param slaveRefVector reference to the vector id->name of slave contigs
     */
    PctgBuilder(
			ThreadedBuildPctg *tbp = NULL,
            const RefSequence *masterRef = NULL,
            const RefSequence *slaveRef = NULL);

    //! Gets a master contig, given its ID.
    /*!
     * \param ctgId pair consisting in master sequence's assembly and contig identifiers.
     * \return reference to the master contig with ID \c ctgId.
     */
    const Contig& loadMasterContig(const int32_t ctgId) const;

    //! Gets a slave contig, given its ID.
    /*!
     * \param ctgId pair consisting in slave sequence's assembly and contig identifiers.
     * \return reference to the slave contig with ID \c ctgId.
     */
    const Contig& loadSlaveContig(const int32_t ctgId) const;

	void appendMasterToPctg( PairedContig &pctg, MergeStruct &ctgInfo, int64_t start, int64_t end );
	void appendSlaveToPctg( PairedContig &pctg, MergeStruct &ctgInfo, int64_t start, int64_t end );

	void prependMasterToPctg( PairedContig &pctg, MergeStruct &ctgInfo, int64_t start, int64_t end );
	void prependSlaveToPctg( PairedContig &pctg, MergeStruct &ctgInfo, int64_t start, int64_t end );

	void appendBlocksRegionToPctg
	(
		PairedContig &pctg,
		MergeStruct &masterInfo, int64_t masterStart, int64_t masterEnd,
		MergeStruct &slaveInfo, int64_t slaveStart, int64_t slaveEnd
	);

	void prependBlocksRegionToPctg
	(
		PairedContig &pctg,
		MergeStruct &masterInfo, int64_t masterStart, int64_t masterEnd,
		MergeStruct &slaveInfo, int64_t slaveStart, int64_t slaveEnd
	);

	// new merge
	void appendMasterToPctg( PairedContig &pctg, int32_t id, Contig &ctg, int32_t start, int32_t end, bool rev );
	void appendSlaveToPctg( PairedContig &pctg, int32_t id, Contig &ctg, int32_t start, int32_t end, bool rev );
	void appendBlocksRegionToPctg( PairedContig &pctg, int32_t m_id, Contig &m_ctg, int32_t m_start, int32_t m_end, bool m_rev,
								   int32_t s_id, Contig &s_ctg, int32_t s_start, int32_t s_end, bool s_rev );

	void buildPctgs( std::list<PairedContig> &pctgList, MergeBlockLists &mergeLists );
	void buildPctgs( std::list<PairedContig> &pctgList, std::list<MergeBlock> &ml );

	void splitMergeBlocksByInclusions( MergeBlockLists &ml_in );
	void sortMergeBlocksByDirection( MergeBlockLists &ml );
	void splitMergeBlocksByDirection( MergeBlockLists &ml_in );
	void splitMergeBlocksByAlign( MergeBlockLists &ml_in );
	void alignMergeBlock( const CompactAssemblyGraph &graph, MergeBlock &mb ) const;
	void getMergePath( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v, MergeBlockLists &merge_paths,
					   bool prevMisAssembly = false, EdgeKindType prevEdgeKind = BOTH_EDGE, bool sharedFirst = false ) const;
    bool getMergePaths( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v, std::vector<MergeBlock> &mbv, 
                        MergeBlockLists &merge_paths ) const;
    bool solveForks( CompactAssemblyGraph &graph, std::vector<MergeBlock> &mbv ) const;
    void getNextContigs( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v, std::set<int32_t> &masterCtgs, std::set<int32_t> &slaveCtgs ) const;
    void getPrevContigs( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v, std::set<int32_t> &masterCtgs, std::set<int32_t> &slaveCtgs ) const;
	// end new merge

	void mergeContigs( std::list<PairedContig> &pctgList, MergeDescriptorLists &mergeLists );
	void mergeContigs( std::list<PairedContig> &pctgList, std::list<MergeDescriptor> &mergeList );

	void reverseCoordinates( std::pair<int64_t,int64_t> &coords, size_t ctg_size );

	bool isAlignmentFailed( BestCtgAlignment &align, std::pair<uint64_t,uint64_t> masterPos, std::pair<uint64_t,uint64_t> slavePos, bool isMasterRev );

    /*void getMergeLists( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex v, MergeDescriptorLists &mergeLists,
						bool hasMisAssembly = false, EdgeKindType edgeKind = BOTH_EDGE, bool sharedFirst = false ) const;*/

	ForkType getForkType( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v, CompactAssemblyGraph::Vertex &mv, CompactAssemblyGraph::Vertex &sv ) const;
	ForkType getInForkType( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v ) const;
	ForkType getOutForkType( const CompactAssemblyGraph &graph, CompactAssemblyGraph::Vertex &v ) const;

    //! Builds a paired contig with a single (master) contig.
    /*!
     * \param pctgId the paired contig's identifier
     * \param ctgId  a (master) contig identifier
     */
    PairedContig initByContig(const IdType &pctgId, const int32_t ctgId) const;

    //! Returns a paired contig extended using a block on the assemblies.
    /*!
     * The paired contig is expanded with the master or slave contig of the block
     * if it contains exactly one of them.
     * \param pctg a paired contig
     * \param blocks_list list of blocks considered in the merging
     * \return the paired contig \c pctg extended using \c block.
     */
    PairedContig& extendByBlock(PairedContig &pctg, const std::list<Block> &blocks_list) const;

    //! Adds the first block to a paired contig.
    /*!
     * The paired contig must be empty.
     * \param pctg a paired contig.
     * \param firstBlock first block in common between the contigs
     * \param lastBlock last block in common between the contigs
     * \return a paired contig in which the contigs of \c block may have been merged.
     *
     * \throws std::invalid_argument if the \c pctg is not empty.
     */
	PairedContig& addFirstBlockTo(PairedContig &pctg, const std::list<Block> &blocks_list) const;

    //! Adds the first contig to a paired contig.
    /*!
     * The contig must be one of the master assembly.
     * \param pctg a paired contig.
     * \param ctgId a pair consisting in assembly and contig identifiers of the sequence.
     * \return a paired contig consisting of the single (master) contig \c ctgId.
     *
     * \throws std::invalid_argument if the \c pctg is not empty.
     */
    PairedContig& addFirstContigTo(PairedContig &pctg, const int32_t ctgId) const;

	PairedContig& addFirstSlaveContigTo(PairedContig& pctg, const int32_t ctgId) const;

    //! Returns a paired contig extended with a contig.
    /*!
     * \param orig a paired contig
     * \param ctg a contig
     * \param ctgInfo a ContigInPctgInfo object associated to \c ctg
     * \param pos pair of positions in \c orig and \c ctg from which to start the extension
     * \param gaps pair of gaps to fill \c ctgInfo
     * \param isMasterCtg whether \c ctg is a master or a slave contig
     * \return the extended paired contig
     */
    PairedContig& extendPctgWithCtgFrom(
        PairedContig &orig,
        const Contig &ctg,
        ContigInPctgInfo &ctgInfo,
        const std::pair<uint64_t,uint64_t> &pos,
        bool isMasterCtg ) const;

	PairedContig& updatePctgWithCtg(
		PairedContig& pctg,
		const Contig& ctg,
		ContigInPctgInfo &ctgInPctgInfo,
		std::pair<uint64_t,uint64_t>& start_align,
		std::pair<uint64_t,uint64_t>& end_align ) const;

    //! Extends a paired contig adding bases to the left end.
    /*!
     * \param orig a paired contig
     * \param ctg a contig
     * \param ctgInfo a ContigInPctgInfo object associated to \c ctg
     * \param pos pair of first matching positions in \c orig and \c ctg
     * \param pctgShift number of bases to be added to the left end of \c orig
     * \param isMasterCtg whether \c ctg is a master or slave contig
     */
    PairedContig& extendPctgWithCtgUpto(
        PairedContig &orig,
        const Contig &ctg,
		const ContigInPctgInfo &ctgInfo,
        const std::pair<UIntType,UIntType>& pos,
        const UIntType pctgShift,
        bool isMasterCtg ) const;

    //! Shifts a paired contig.
    /*!
     * \param orig a paired contig
     * \param shiftSize shift amount
     * \return the paired contig \c orig which has been shifted of \c shiftSize positions.
     */
    PairedContig& shiftPctgOf( PairedContig &orig, const UIntType shiftSize ) const;

    //! Merge a paired contig with a single contig.
    /*!
     * \param pctg a paired contig
	 * \param blocks_list list of blocks considered in merging
     * \param mergeMasterCtg whether to merge the master or slave contig of \c block
     * \return the paired contig obtained from merging.
     */
	PairedContig& mergeContig( PairedContig &pctg, const std::list<Block> &blocks_list, bool mergeMasterCtg ) const;

    //! Merge a paired contig with a contig, using an alignment.
    /*!
     * \param pctg a paired contig
     * \param ctg a contig
     * \param ctgId \c ctg identifier
     * \param bestAlign best alignment of \c pctg and \c ctg
     * \param mergeMaster whether \c ctg is a master or a slave contig
     * \return the paired contig obtained from merging \c pctg and \c ctg.
     */
    PairedContig& mergeCtgInPos(
        PairedContig &pctg,
        const Contig &ctg,
		const int32_t ctgInPctgId,
        const int32_t ctgId,
        const BestPctgCtgAlignment &bestAlign,
        bool mergeMaster ) const;

    //! Merge a paired contig with a master contig, using an alignment.
    /*!
     * \param pctg a paired contig
     * \param ctg a master contig
     * \param ctgId \c ctg identifier
     * \param bestAlign best alignment of \c pctg and \c ctg
     * \return the paired contig obtained from merging \c pctg and \c ctg.
     */
    /*PairedContig& mergeMasterCtgInPos(
        PairedContig &pctg,
        const Contig &ctg,
        const std::pair<IdType,IdType> &ctgId,
        const BestPctgCtgAlignment &bestAlign ) const;*/

    //! Merge a paired contig with a slave contig, using an alignment.
    /*!
     * \param pctg a paired contig
     * \param ctg a slave contig
     * \param ctgId \c ctg identifier
     * \param bestAlign best alignment of \c pctg and \c ctg
     * \return the paired contig obtained from merging \c pctg and \c ctg.
     */
    /*PairedContig& mergeSlaveCtgInPos(
        PairedContig &pctg,
        const Contig &ctg,
        const std::pair<IdType,IdType> &ctgId,
        const BestPctgCtgAlignment &bestAlign ) const;*/

    //! Computes the best alignment between a paired contig and a contig which may be merged.
    /*!
     * \param pctg a paired contig
     * \param pctgInfo
     * \param startPos position in \c pctg from which to find the alignment (where the first block starts)
	 * \param endPos position in \c pctg where the last block ends
     * \param ctg a contig
	 * \param mergeMasterCtg whether \c ctg is a master or a slave contig
     * \param blocks_list list of blocks between the contigs to be merged
     */
    void findBestAlignment(
        BestCtgAlignment &bestAlign,
        Contig &masterCtg,
		uint64_t masterStart,
		uint64_t masterEnd,
		Contig &slaveCtg,
        uint64_t slaveStart,
        uint64_t slaveEnd,
        const std::list<Block>& blocks_list ) const;

	void alignBlocks(
		const Contig &masterCtg,
		const uint64_t &masterStart,
		const Contig &slaveCtg,
		const uint64_t &slaveStart,
		const std::list<Block> &blocks_list,
		std::vector< MyAlignment > &alignments ) const;

	bool is_good( const std::vector<MyAlignment> &align, uint64_t min_align_len = MIN_ALIGNMENT_LEN ) const;
	bool is_good( const MyAlignment &align, uint64_t min_align_len = MIN_ALIGNMENT_LEN ) const;
};

#endif	/* PCTGBUILDER_HPP */

