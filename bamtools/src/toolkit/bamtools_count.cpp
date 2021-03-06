// ***************************************************************************
// bamtools_count.cpp (c) 2010 Derek Barnett, Erik Garrison
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 7 April 2011
// ---------------------------------------------------------------------------
// Prints alignment count for BAM file(s)
// ***************************************************************************

#include "bamtools_count.h"

#include <api/BamMultiReader.h>
#include <utils/bamtools_options.h>
#include <utils/bamtools_utilities.h>
using namespace BamTools;

#include <iostream>
#include <string>
#include <vector>
using namespace std;

// ---------------------------------------------  
// CountSettings implementation

struct CountTool::CountSettings {

    // flags
    bool HasInput;
    bool HasRegion;

    // filenames
    vector<string> InputFiles;
    string Region;
    
    // constructor
    CountSettings(void)
        : HasInput(false)
        , HasRegion(false)
    { }  
}; 
  
// ---------------------------------------------
// CountToolPrivate implementation

struct CountTool::CountToolPrivate {

    // ctor & dtro
    public:
        CountToolPrivate(CountTool::CountSettings* settings)
            : m_settings(settings)
        { }

        ~CountToolPrivate(void) { }

    // interface
    public:
        bool Run(void);

    // data members
    private:
        CountTool::CountSettings* m_settings;
};

bool CountTool::CountToolPrivate::Run(void) {

    // if no '-in' args supplied, default to stdin
    if ( !m_settings->HasInput )
        m_settings->InputFiles.push_back(Options::StandardIn());

    // open reader without index
    BamMultiReader reader;
    if ( !reader.Open(m_settings->InputFiles) ) {
        cerr << "bamtools count ERROR: could not open input BAM file(s)... Aborting." << endl;
        return false;
    }

    // alignment counter
    BamAlignment al;
    int alignmentCount(0);

    // if no region specified, count entire file
    if ( !m_settings->HasRegion ) {
        while ( reader.GetNextAlignmentCore(al) )
            ++alignmentCount;
    }

    // otherwise attempt to use region as constraint
    else {

        // if region string parses OK
        BamRegion region;
        if ( Utilities::ParseRegionString(m_settings->Region, reader, region) ) {

            // attempt to find index files
            reader.LocateIndexes();

            // if index data available for all BAM files, we can use SetRegion
            if ( reader.IsIndexLoaded() ) {

                // attempt to set region on reader
                if ( !reader.SetRegion(region.LeftRefID, region.LeftPosition, region.RightRefID, region.RightPosition) ) {
                    cerr << "bamtools count ERROR: set region failed. Check that REGION describes a valid range" << endl;
                    reader.Close();
                    return false;
                }

                // everything checks out, just iterate through specified region, counting alignments
                while ( reader.GetNextAlignmentCore(al) )
                    ++alignmentCount;
            }

            // no index data available, we have to iterate through until we
            // find overlapping alignments
            else {
                while ( reader.GetNextAlignmentCore(al) ) {
                    if ( (al.RefID >= region.LeftRefID)  && ( (al.Position + al.Length) >= region.LeftPosition ) &&
                          (al.RefID <= region.RightRefID) && ( al.Position <= region.RightPosition) )
                    {
                        ++alignmentCount;
                    }
                }
            }
        }

        // error parsing REGION string
        else {
            cerr << "bamtools count ERROR: could not parse REGION - " << m_settings->Region << endl;
            cerr << "Check that REGION is in valid format (see documentation) and that the coordinates are valid"
                 << endl;
            reader.Close();
            return false;
        }
    }

    // print results
    cout << alignmentCount << endl;

    // clean up & exit
    reader.Close();
    return true;
}

// ---------------------------------------------
// CountTool implementation

CountTool::CountTool(void) 
    : AbstractTool()
    , m_settings(new CountSettings)
    , m_impl(0)
{ 
    // set program details
    Options::SetProgramInfo("bamtools count", "prints number of alignments in BAM file(s)", "[-in <filename> -in <filename> ...] [-region <REGION>]");
    
    // set up options 
    OptionGroup* IO_Opts = Options::CreateOptionGroup("Input & Output");
    Options::AddValueOption("-in",     "BAM filename", "the input BAM file(s)", "", m_settings->HasInput,  m_settings->InputFiles, IO_Opts, Options::StandardIn());
    Options::AddValueOption("-region", "REGION",       "genomic region. Index file is recommended for better performance, and is used automatically if it exists. See \'bamtools help index\' for more details on creating one", "", m_settings->HasRegion, m_settings->Region, IO_Opts);
}

CountTool::~CountTool(void) { 

    delete m_settings;
    m_settings = 0;

    delete m_impl;
    m_impl = 0;
}

int CountTool::Help(void) { 
    Options::DisplayHelp();
    return 0;
} 

int CountTool::Run(int argc, char* argv[]) { 

    // parse command line arguments
    Options::Parse(argc, argv, 1);

    // initialize CountTool with settings
    m_impl = new CountToolPrivate(m_settings);

    // run CountTool, return success/fail
    if ( m_impl->Run() )
        return 0;
    else
        return 1;
}
