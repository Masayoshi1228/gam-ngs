/* 
 * File:   samgam.cc
 * Author: RiccardoVicedomini
 *
 * Created on 21 maggio 2011, 14.53
 */

#include <iostream>

#include <map>
#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/graph/graphviz.hpp>

#include "api/BamAux.h"
#include "api/BamReader.h"
#include "api/BamAlignment.h"

#include "types.hpp"
#include "assembly/Read.hpp"
#include "assembly/Block.hpp"
#include "graphs/PairingEvidencesGraph.hpp"
#include "pctg/PairedContig.hpp"
#include "pool/HashContigMemPool.hpp"
#include "OrderingFunctions.hpp"
#include "PartitionFunctions.hpp"
#include "ThreadedBuildPctg.code.hpp"
#include "UtilityFunctions.hpp"

using namespace BamTools;
using google::sparse_hash_map;

void printGamCmdError( char** argv )
{
    std::cerr << std::endl << "Usage:   " << getPathBaseName(argv[0]) << " <command> [options]\n";
    std::cerr << "\n";
    std::cerr << "Command: " << "blocks" << "\t" << "generate blocks file\n"
              << "         " << "merge" << "\t" << "merge two assemblies given a blocks file\n"
              << std::endl;
}

void printBlocksCmdError( char** argv )
{
    std::cerr << "Usage: " << getPathBaseName(argv[0]) << " blocks "
            << "[--readsorted-bams <master.readname-sorted.bam> <slave.readname-sorted.bam>] "
            << "<master.coordinate-sorted.bam> " 
            << "<slave.coordinate-sorted.bam> "
            << "<min-block-size> <output.filename>"
            << std::endl;
}

/*
 * 
 */
int main(int argc, char** argv) 
{
    std::string action;                 // command
    std::string outFilePrefix;          // output file prefix
    std::string inBlocksFile;           // input blocks file
    
    std::string bamFileM;               // file BAM (Master)
    std::string bamMasterSorted;        // file BAM (Master) sorted by read name
    std::string bamFileS;               // file BAM (Slave)
    std::string bamSlaveSorted;         // file BAM (Slave) sorted by read name
    
    int minBlockSize = 1;               // dimensione minima (# reads) per i blocchi
    int minEvidence = 1;
    
    std::string masterFasta;            // FASTA file of the master assembly
    std::string slaveFasta;             // FASTA file of the slave assembly
    
    int threadsNum = 1;              // Numero di thread da usare per il weaving
    
    time_t t1,t2;
    t1 = time(NULL);
    
    int i=1; // parameters index
        
    if( argc < 2 )
    {
        printGamCmdError(argv);       
        return 1;
    }
    
    action = argv[i]; // get command string
    i++;
    
    if(action.compare("blocks") != 0 && action.compare("merge") != 0 )
    {
        printGamCmdError(argv);
        return 1;
    }   
    
    /* Command to build the blocks */
    if( action.compare("blocks") == 0 )
    {
        if( argc < 3 ){ printBlocksCmdError(argv); return 1; }
        
        if( std::string(argv[i]) == "--readsorted-bams" )
        {
            if( i+3 > argc ){ printBlocksCmdError(argv); return 1; }
            bamMasterSorted = argv[i+1];
            bamSlaveSorted = argv[i+2];
            i += 3;
        }
        
        if( argc - i != 4 ){ printBlocksCmdError(argv); return 1; }
        
        bamFileM = argv[i++];
        bamFileS = argv[i++];
        minBlockSize = atoi(argv[i++]);
        outFilePrefix = argv[i++];
        
        if( minBlockSize < 1 ) minBlockSize = 1;
        
        struct stat st;
        
        /* Check for master bam files existence */
        
        if( stat(bamFileM.c_str(),&st) != 0 )
        {
            std::cerr << "File " << bamFileM << " (Master BAM CoordinateSorted) doesn't exist." << std::endl;
            return 1;
        }
        
        if( bamMasterSorted != "" && stat(bamMasterSorted.c_str(),&st) != 0 )
        {
            std::cerr << "File " << bamMasterSorted << " (Master BAM ReadNameSorted) doesn't exist." << std::endl;
            return 1;
        }
        
        /* Check for slave bam files existence */
        
        if( stat(bamFileS.c_str(),&st) != 0 )
        {
            std::cerr << "File " << bamFileS << " (Slave BAM CoordinateSorted) doesn't exist." << std::endl;
            return 1;
        }
        
        if( bamSlaveSorted != "" && stat(bamSlaveSorted.c_str(),&st) != 0 )
        {
            std::cerr << "File " << bamSlaveSorted << " (Slave BAM ReadNameSorted) doesn't exist." << std::endl;
            return 1;
        }
            
        BamReader inBamM, inBamS, inBamMasterSorted, inBamSlaveSorted;
        
        inBamM.Open( bamFileM );
        if( stat((bamFileM + ".bai").c_str(),&st) == 0 ) inBamM.OpenIndex( bamFileM + ".bai" );

        inBamS.Open( bamFileS );
        if( stat((bamFileS + ".bai").c_str(),&st) == 0 ) inBamM.OpenIndex( bamFileS + ".bai" );
        
        std::cout << "[loading reads in memory]" << std::endl;
        
        /* load only useful reads of the slave assembly */
        sparse_hash_map< std::string, Read > readMap;
	readMap.set_deleted_key("");
        
        // if read-name sorted bams are not provided, load each mapped read.
        if( bamMasterSorted == "" || bamSlaveSorted == "" )
        {
            Read::loadReadsMap(inBamM,readMap);
        }
        else
        {
            inBamMasterSorted.Open( bamMasterSorted );
            inBamSlaveSorted.Open( bamSlaveSorted );
            
            Read::loadMasterReadsMap(inBamMasterSorted,inBamSlaveSorted,readMap);
            
            inBamMasterSorted.Close();
            inBamSlaveSorted.Close();
        }
        
        std::cout << "[finding blocks]" << std::endl;
        std::vector<Block> blocks;
        Block::findBlocks( blocks, inBamM, inBamS, minBlockSize, readMap );
        
	std::cout << "[blocks found: " << blocks.size() << "]" << std::endl;

	inBamM.Close();
        inBamS.Close();

        std::cout << "[writing blocks on file: " << getPathBaseName( outFilePrefix + ".blocks" ) << "]" << std::endl;
        Block::writeCoreBlocks( outFilePrefix + ".blocks" , blocks ); // write blocks to file
    }
    
    /* Command to merge assemblies */
    if( action.compare("merge") == 0 )
    {
        if( argc != 10 )
        {
            std::cerr << "Usage: " << getPathBaseName(argv[0]) << " merge "
                      << "<Input.blocks> <Min Block Size> "
                      << "<BAM Master CoordinateSorted> <BAM Slave CoordinateSorted> "
                      << "<Master FASTA> <Slave FASTA> <Output FileName> "
                      << "<Threads>"
                      << std::endl;
            return 1;
        }
        
        inBlocksFile = argv[2];
        minBlockSize = atoi(argv[3]);
        bamFileM = argv[4];
        bamFileS = argv[5];
        masterFasta = argv[6];
        slaveFasta = argv[7];
        outFilePrefix = argv[8];
        threadsNum = atoi(argv[9]);
        
        if( threadsNum < 1 ) threadsNum = 1;
        
        struct stat st;
        
        /* Check blocks file existence */
        
        if( stat(inBlocksFile.c_str(),&st) != 0 )
        {
            std::cerr << "Blocks file " << inBlocksFile << " doesn't exist." << std::endl;
            return 1;
        }
        
        /* Check BAM files existence */
        
        if( stat(bamFileM.c_str(),&st) != 0 )
        {
            std::cerr << "File " << bamFileM << " (BAM Master CoordinateSorted) doesn't exist." << std::endl;
            return 1;
        }
        
        if( stat(bamFileS.c_str(),&st) != 0 )
        {
            std::cerr << "File " << bamFileS << " (BAM Slave CoordinateSorted) doesn't exist." << std::endl;
            return 1;
        }
        
        /* Check FASTA files existence */
        
        if( stat(masterFasta.c_str(),&st) != 0 )
        {
            std::cerr << "File " << masterFasta << " (Master FASTA) doesn't exist." << std::endl;
            return 1;
        }
        
        if( stat(slaveFasta.c_str(),&st) != 0 )
        {
            std::cerr << "File " << slaveFasta << " (Slave FASTA) doesn't exist." << std::endl;
            return 1;
        }
        
        BamReader inBamM, inBamS;
        inBamM.Open( bamFileM );
        inBamS.Open( bamFileS );
        
        std::cout << "[loading blocks]" << std::endl << std::flush;
        std::vector<Block> blocks = Block::readCoreBlocks( inBlocksFile, minBlockSize );
        std::cout << "[blocks loaded: " << blocks.size() << "]" << std::endl << std::flush;
        
        // keep only blocks between contigs that share at least minEvidence blocks.
        //std::vector< Block > outBlocks = filterBlocksByPairingEvidences( blocks, minEvidence );
        
        std::cout << "[filtering blocks by frames overlap]" << std::endl << std::flush;
        // remove adjacent blocks if their frames overlap
        // blocks = Block::filterBlocksByOverlaps( blocks );
        blocks = Block::filterBlocksByCoverage( blocks );
        std::cout << "[blocks filtered: " << blocks.size() << "]" << std::endl << std::flush;
        
        // create the graph of assemblies, remove cycles and keep remaining blocks.
        std::cout << "[removing cycles from assemblies graph]" << std::endl << std::flush;
        std::list< std::vector<Block> > pcblocks = partitionBlocks( blocks );
        
        std::cout << "[loading reference data]" << std::endl << std::flush;
        // load reference data (index => contig name)
        BamTools::RefVector mcRef = inBamM.GetReferenceData();
        std::vector<BamTools::RefVector> scRef(1); scRef[0] = inBamS.GetReferenceData();
        
        std::map< std::string, int32_t > masterContigs;
        std::vector< std::map<std::string,int32_t> > slaveCtgsVect(1);
        
        BamTools::RefVector::const_iterator ref_iter;
        
        for( ref_iter = mcRef.begin(); ref_iter != mcRef.end(); ref_iter++ ) 
            masterContigs[ ref_iter->RefName ] = ref_iter->RefLength;
        
        for( int i=0; i < slaveCtgsVect.size(); i++ )
        {
            for( ref_iter = scRef[i].begin(); ref_iter != scRef[i].end(); ref_iter++ )
                slaveCtgsVect[i][ ref_iter->RefName ] = ref_iter->RefLength;
        }
        
        std::cout << "[loading contigs in memory]" << std::endl << std::flush;
        
        ExtContigMemPool masterPool, slavePool;
        HashContigMemPool pctgPool;
        // load master and slave contigs in memory
        masterPool.loadPool( 0, masterFasta, masterContigs );
        slavePool.loadPool( 0, slaveFasta, slaveCtgsVect[0] );
        
        std::cout << "[building paired contigs]" << std::endl << std::flush;
        // build paired contigs (threaded)
        ThreadedBuildPctg tbp( pcblocks, &pctgPool, &masterPool, &slavePool, &mcRef, &scRef );
        std::pair< std::list<PairedContig>, std::vector<bool> > result = tbp.run((size_t)threadsNum);
        
        IdType pctgNum((result.first).size());
        std::cout << "[paired contigs built: " << pctgNum << "]" << std::endl << std::flush;
        
        slavePool.clear(); // slave contigs pool is no more needed

        std::list<IdType> ctgIds;
                
        for(IdType i = 0; i < result.second.size(); i++) 
            if( !result.second.at(i) ) ctgIds.push_back(i);

        // add unused contigs in paired contigs pool
        generateSingleCtgPctgs( result.first, ctgIds, &masterPool, &mcRef, pctgNum);

        // save paired contig pool to disk
        std::cout << "[writing paired contigs on file: " << ( outFilePrefix + ".fasta" ) << "]" << std::endl << std::flush;
        //pctgPool.savePool( outFilePrefix + ".fasta" );
        result.first.sort( orderPctgsByName );
        std::ofstream outFasta((outFilePrefix + ".fasta").c_str(),std::ios::out);
        std::list< PairedContig >::const_iterator i;
        for( i = result.first.begin(); i != result.first.end(); i++ ) outFasta << *i << std::endl;
        outFasta.close();
        
        // save paired contigs descriptors to file
        std::cout << "[writing paired contigs descriptors on file: " << ( outFilePrefix + ".pctgs" ) << "]" << std::endl << std::flush;
        std::fstream pctgDescFile( (outFilePrefix + ".pctgs").c_str(), std::fstream::out );
        writePctgDescriptors( pctgDescFile, result.first, mcRef, scRef );
        pctgDescFile.close();
        
        // save IDs of (slave) contigs NOT merged
        std::cout << "[writing unused slave contigs on file: " << ( outFilePrefix + ".unused" ) << "]" << std::endl << std::flush;
        std::fstream unusedCtgsFile( (outFilePrefix + ".unused").c_str(), std::fstream::out );
        std::vector< std::vector<bool> > usedCtgs;
        for( size_t i=0; i < scRef.size(); i++ ) usedCtgs.push_back( std::vector<bool>( scRef[i].size(), false ) );
        
        for( std::list< PairedContig >::const_iterator pctg = result.first.begin(); pctg != result.first.end(); pctg++ ) 
        {
            typedef std::map< std::pair<IdType,IdType>, ContigInPctgInfo > ContigInfoMap;
            ContigInfoMap slaveCtgs = pctg->getSlaveCtgMap();
            
            for( ContigInfoMap::const_iterator ctg = slaveCtgs.begin(); ctg != slaveCtgs.end(); ctg++ ) 
                usedCtgs[(ctg->first).first][(ctg->first).second] = true;
        }
        
        for( unsigned int i = 0; i < usedCtgs.size(); i++ )
        {
            for( size_t j=0; j < usedCtgs[i].size(); j++ )
                if( !usedCtgs[i][j] ){ unusedCtgsFile << i << "\t" << scRef[i][j].RefName << "\n"; }
        }
        
        unusedCtgsFile.close();
        
        inBamM.Close();
        inBamS.Close();
    }
    
    t2 = time(NULL);
    std::cout << "[execution time: " << formatTime(t2-t1) << "]" << std::endl;
    
    return 0;
}
