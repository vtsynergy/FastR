/**********************************************************************************
* Copyright (c) 2012 by Virginia Polytechnic Institute and State University.
*
* The local realignment portion of the code is derived from the Indel Realigner
* code of the GATK project, and the I/O of bam files is extended from the
* bamtools package, which is distributed under the MIT license. The licenses of
* GATK and bamtools are included below.
*
* *********** GATK LICENSE ***************
* Copyright (c) 2010, The Broad Institute
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
* of the Software, and to permit persons to whom the Software is furnished to do
* so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* *********** BAMTOOLS LICENSE ***************
* The MIT License
*
* Copyright (c) 2009-2010 Derek Barnett, Erik Garrison, Gabor Marth, Michael Stromberg
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
* of the Software, and to permit persons to whom the Software is furnished to do
* so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
**********************************************************************************/
#include "Fasta.h"
#include <stdlib.h>
#include <getopt.h>
#include "disorder.h"

void printSummary()
{
    cerr << "usage: fastahack [options] <fasta reference>" << endl
         << endl
         << "options:" << endl
         << "    -i, --index          generate fasta index <fasta reference>.fai" << endl
         << "    -r, --region REGION  print the specified region" << endl
         << "    -c, --stdin          read a stream of line-delimited region specifiers on stdin" << endl
         << "                         and print the corresponding sequence for each on stdout" << endl
         //<< "    -l, --length         print the length of the specified region or sequence" << endl
         << "    -e, --entropy        print the shannon entropy of the specified region" << endl
         << endl
         << "REGION is of the form <seq>, <seq>:<start>..<end>, <seq1>:<start>..<seq2>:<end>" << endl
         << "where start and end are 1-based, and the region includes the end position." << endl
         << "Specifying a sequence name alone will return the entire sequence, specifying" << endl
         << "range will return that range, and specifying a single coordinate pair, e.g." << endl
         << "<seq>:<start> will return just that base." << endl
         << endl
         << "author: Erik Garrison <erik.garrison@bc.edu>" << endl;
}

class FastaRegion
{
public:
    string startSeq;
    int startPos;
    int stopPos;

    FastaRegion(string &region) {
        startPos = -1;
        stopPos = -1;
        size_t foundFirstColon = region.find(":");
        // we only have a single string, use the whole sequence as the target
        if(foundFirstColon == string::npos) {
            startSeq = region;
        } else {
            startSeq = region.substr(0, foundFirstColon);
            size_t foundRangeDots = region.find("..", foundFirstColon);
            if(foundRangeDots == string::npos) {
                startPos = atoi(region.substr(foundFirstColon + 1).c_str());
                stopPos = startPos; // just print one base if we don't give an end
            } else {
                startPos = atoi(region.substr(foundFirstColon + 1, foundRangeDots - foundRangeDots - 1).c_str());
                stopPos = atoi(region.substr(foundRangeDots + 2).c_str()); // to the start of this chromosome
            }
        }
    }

    int length(void) {
        if(stopPos > 0) {
            return stopPos - startPos + 1;
        } else {
            return 1;
        }
    }

};


int main(int argc, char **argv)
{

    string command;
    string fastaFileName;
    string seqname;
    string longseqname;
    long long start;
    long long length;

    bool buildIndex = false;  // flag to force index building
    bool printEntropy = false;  // entropy printing
    bool readRegionsFromStdin = false;
    //bool printLength = false;
    string region;

    int c;

    while(true) {
        static struct option long_options[] = {
            /* These options set a flag. */
            //{"verbose", no_argument,       &verbose_flag, 1},
            //{"brief",   no_argument,       &verbose_flag, 0},
            {"help", no_argument, 0, 'h'},
            {"index",  no_argument, 0, 'i'},
            //{"length",  no_argument, &printLength, true},
            {"entropy", no_argument, 0, 'e'},
            {"region", required_argument, 0, 'r'},
            {"stdin", no_argument, 0, 'c'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "hcier:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if(c == -1) {
            break;
        }

        switch(c) {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if(long_options[option_index].flag != 0) {
                break;
            }
            printf("option %s", long_options[option_index].name);
            if(optarg) {
                printf(" with arg %s", optarg);
            }
            printf("\n");
            break;

        case 'e':
            printEntropy = true;
            break;

        case 'c':
            readRegionsFromStdin = true;
            break;

        case 'i':
            buildIndex = true;
            break;

        case 'r':
            region = optarg;
            break;

        case 'h':
            printSummary();
            exit(0);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            printSummary();
            exit(1);
            break;

        default:
            abort();
        }
    }

    /* Print any remaining command line arguments (not options). */
    if(optind < argc) {
        //cerr << "fasta file: " << argv[optind] << endl;
        fastaFileName = argv[optind];
    } else {
        cerr << "please specify a fasta file" << endl;
        printSummary();
        exit(1);
    }

    if(buildIndex) {
        FastaIndex *fai = new FastaIndex();
        //cerr << "generating fasta index file for " << fastaFileName << endl;
        fai->indexReference(fastaFileName);
        fai->writeIndexFile((string) fastaFileName + fai->indexFileExtension());
    }

    string sequence;  // holds sequence so we can optionally process it

    FastaReference *fr = new FastaReference(fastaFileName);
    if(region != "") {
        FastaRegion target(region);

        if(target.startPos == -1) {
            sequence = fr->getSequence(target.startSeq);
        } else {
            sequence = fr->getSubSequence(target.startSeq, target.startPos - 1, target.length());
        }

    }

    if(readRegionsFromStdin) {
        string regionstr;
        while(getline(cin, regionstr)) {
            FastaRegion target(regionstr);
            if(target.startPos == -1) {
                cout << fr->getSequence(target.startSeq) << endl;
            } else {
                cout << fr->getSubSequence(target.startSeq, target.startPos - 1, target.length()) << endl;
            }
        }
    } else {
        if(sequence != "") {
            if(printEntropy) {
                if(sequence.size() > 0) {
                    cout << shannon_H((char *) sequence.c_str(), sequence.size()) << endl;
                } else {
                    cerr << "please specify a region or sequence for which to calculate the shannon entropy" << endl;
                }
            } else {  // if no statistical processing is requested, just print the sequence
                cout << sequence << endl;
            }
        }
    }

    return 0;
}
