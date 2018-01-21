/*The MIT License (MIT)

Copyright (c) 2017 Fan Zhang, Hyun Min Kang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include "ContaminationEstimator.h"
#include "params.h"
#include "SVDcalculator.h"
#include "PhoneHome.h"
#include <stdexcept>

int execute(int argc, char **argv) {

    std::string UDPath("Empty"), PCPath("Empty"), MeanPath("Empty"), BedPath("Empty"), BamFileList("Empty"), BamFile("Empty"), RefPath(
            "Empty"), outputPrefix("result"), PileupFile("Empty"), SVDPrefix("Empty");
    std::string knownAF("Empty");
    std::string RefVCF("Empty");
    std::string fixPC("Empty");
    double fixAlpha(-1.),epsilon(1e-8);
    bool withinAncestry(false),outputPileup(false),verbose(false);
    int nfiles(0),seed(12345),nPC(2),nthread(4);
    paramList pl;
    BEGIN_LONG_PARAMS(longParameters)
                    LONG_PARAM_GROUP("Input/Output Files",
                                     "Input/Output files for the program[Complete Path Recommended]")
                    LONG_STRING_PARAM("BamFile", &BamFile,
                                      "[String] Bam or Cram file for the sample[Required if --PileupFile not specified]")
                    LONG_STRING_PARAM("PileupFile", &PileupFile,
                                      "[String] Pileup file for the sample[Required if --BamFile not specified]")
                    LONG_STRING_PARAM("Reference", &RefPath,
                                      "[String] Reference file[Required]")
                    LONG_STRING_PARAM("SVDPrefix", &SVDPrefix,
                                      "[String] SVD related files prefix(normally shared by .UD, .mu and .bed files)[Required]")
                    LONG_STRING_PARAM("RefVCF", &RefVCF,
                                      "[String] VCF file from which to extract reference panel's genotype matrix[Required if no SVD files available]")
                    LONG_STRING_PARAM("Output", &outputPrefix,
                                      "[String] Prefix of output files[optional]")
                    LONG_PARAM_GROUP("Model Selection Options",
                                     "Options to adjust model selection and parameters")
                    LONG_PARAM("WithinAncestry", &withinAncestry,
                                     "[Bool] Enabling withinAncestry assume target sample and contamination source are from the same populations,[default:BetweenAncestry] otherwise")
                    LONG_INT_PARAM("NumPC", &nPC,
                                     "[Int] Set number of PCs to infer Allele Frequency[optional]")
                    LONG_STRING_PARAM("FixPC", &fixPC, "[String] Input fixed PCs to estimate Alpha[format PC1:PC2:PC3...]")
                    LONG_DOUBLE_PARAM("FixAlpha", &fixAlpha, "[Double] Input fixed Alpha to estimate PC coordinates")
                    LONG_STRING_PARAM("KnownAF", &knownAF, "[String] known allele frequency file (chr\tpos\tfreq)[Optional]")
                    LONG_INT_PARAM("NumThread", &nthread,
                                   "[Int] Set number of threads in likelihood calculation[default:4]")
                    LONG_INT_PARAM("Seed",&seed,"[Int] Random number seed[default:12345]")
                    LONG_DOUBLE_PARAM("Epsilon",&epsilon,"[Double] Minimization procedure convergence threshold, usually a trade-off bettween accuracy and running time[default:1e-10]")
                    LONG_PARAM("OutputPileup", &outputPileup, "[Bool] If output temp pileup file")
                    LONG_PARAM("Verbose", &verbose, "[Bool] If print the progress of the method on the screen")
                    LONG_PARAM_GROUP("Deprecated Options",
                                     "These options still are available but not recommended")
                    LONG_STRING_PARAM("UDPath", &UDPath,
                                      "[String] UD matrix file from SVD result of genotype matrix")
                    LONG_STRING_PARAM("MeanPath", &MeanPath,
                                      "[String] Mean matrix file of genotype matrix")
                    LONG_STRING_PARAM("BedPath", &BedPath,
                                      "[String] Bed file for markers used in this analysis,1 based pos(chr\tpos-1\tpos\trefAllele\taltAllele)[Required]")


    END_LONG_PARAMS();

    pl.Add(new longParams("Available Options", longParameters));
    pl.Read(argc, argv);
    pl.Status();

    if (RefVCF == "Empty") {
        if (SVDPrefix == "Empty") {
            if (UDPath == "Empty") {
                error("--UDPath is required when --RefVCF is absent");
                exit(EXIT_FAILURE);
            }
            if (MeanPath == "Empty") {
                error("--MeanPath is required when --RefVCF is absent");
                exit(EXIT_FAILURE);
            }
            if (BedPath == "Empty") {
                error("--BedPath is required when --RefVCF is absent");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            UDPath = SVDPrefix + ".UD";
            MeanPath = SVDPrefix + ".mu";
            BedPath = SVDPrefix + ".bed";
        }
    } else//SVD on the fly
    {
        notice("Specified reference panel VCF file, doing SVD on the fly...");
        notice("This procedure will generate SVD matrices as [RefVCF path].UD and [RefVCF path].mu");
        notice("You may specify --SVDPrefix [RefVCF path](or --UDPath [RefVCF path].UD and --MeanPath [RefVCF path].mu) in future use");
        SVDcalculator calculator;
        calculator.ProcessRefVCF(RefVCF);
        UDPath = RefVCF+".UD";
        MeanPath = RefVCF+".mu";
        BedPath = RefVCF+".bed";
    }

    ////patch to PC path
    PCPath=UDPath.substr(0,UDPath.size()-3)+".V";
    ////

    if (RefPath == "Empty") {
        error("--Reference is required");
        exit(EXIT_FAILURE);
    }
    if (BamFile != "Empty") {
        nfiles = 1;
    }
//    else if (BamFileList !="Empty")
//    {
//        nfiles=2;
//    }
    else if(PileupFile != "Empty")
    {

    }
    else {
        error("--BamFile or --PileupFile is required");
        exit(EXIT_FAILURE);
    }

    ContaminationEstimator Estimator(nPC, BedPath.c_str(), nthread, epsilon);

    Estimator.verbose=verbose;
    Estimator.seed = seed;
    Estimator.isHeter = !withinAncestry;

    //std::cerr<<"NumMarker:"<<Estimator.NumMarker<<" and UD size:"<<Estimator.UD.size()<<std::endl;
    if(fixPC!="Empty") {// parse --fixPC
        notice("you specified --fixPC, this will overide dynamic estimation of PCs");
        notice("parsing the PCs");
        std::stringstream ss(fixPC);
        std::string token;
        std::vector<PCtype> tmpPC;
        while(std::getline(ss, token, ':')) {
            tmpPC.push_back(atof(token.c_str()));
        }
        if(tmpPC.size() > nPC)
            warning("parameter --fixPC provided larger dimension than parameter --numPC(default value 2) and hence will be truncated");
        if(tmpPC.size() < nPC)
            error("parameter --fixPC provided smaller dimension than parameter --numPC(default value 2)");
        for (int i = 0; i < nPC; ++i) {
            Estimator.PC[1][i]=tmpPC[i];
        }
        Estimator.isPCFixed = true;
    }
    else if(fabs(fixAlpha + 1.)>std::numeric_limits<double>::epsilon()) {
        notice("you specified --fixAlpha, this will overide dynamic estimation of alpha");
        Estimator.alpha = fixAlpha;
        Estimator.isAlphaFixed = true;
    }
    if(knownAF!="Empty") {
        Estimator.isAFknown = true;
        Estimator.isPCFixed = true;
        Estimator.isHeter =false; //under --knownAF condition, we assume WithinAncestry model
        Estimator.ReadAF(knownAF);
    }

    Estimator.ReadSVDMatrix(UDPath, PCPath, MeanPath);

    if(nfiles)
        Estimator.ReadBam(BamFile.c_str(), RefPath.c_str(), BedPath.c_str());
    else
        Estimator.ReadPileup(PileupFile);


    if(outputPileup)
    {
        std::ofstream fout(outputPrefix+".Pileup");
        for(auto item:Estimator.BedVec)
        {
            if(Estimator.viewer.posIndex.find(item.chr)==Estimator.viewer.posIndex.end())//chr existed
                continue;
            else if(Estimator.viewer.posIndex[item.chr].find(item.end)==Estimator.viewer.posIndex[item.chr].end())
                continue;

            fout<<item.chr<<"\t"<<item.end<<"\t"<<Estimator.ChooseBed[item.chr][item.end].first<<"\t"<<Estimator.viewer.GetBaseInfoAt(item.chr,item.end).size()<<"\t";
            for(auto base:Estimator.viewer.GetBaseInfoAt(item.chr,item.end))
                fout<<base;
            fout<<"\t";
            for(auto qual:Estimator.viewer.GetQualInfoAt(item.chr,item.end))
                fout<<qual;
            fout<<std::endl;
        }
        fout.close();
    }

    if(Estimator.IsSanityCheckOK())
        notice("Passing Marker Sanity Check...");
    else {
        warning("Insufficient Available markers, check input bam depth distribution in output pileup file after specifying --OutputPileup");
        exit(EXIT_FAILURE);
    }

    Estimator.OptimizeLLK(outputPrefix);

    const char* headers = "#SEQ_ID\tRG\tCHIP_ID\t#SNPS\t#READS\tAVG_DP\tFREEMIX\tFREELK1\tFREELK0\tFREE_RH\tFREE_RA\tCHIPMIX\tCHIPLK1\tCHIPLK0\tCHIP_RH\tCHIP_RA\tDPREF\tRDPHET\tRDPALT";
    std::ofstream fout(outputPrefix+".selfSM");
    fout<<headers<<std::endl;
    fout<<Estimator.viewer.SEQ_SM<<"\tNA\tNA\t"<<Estimator.NumMarker<<"\t"<<Estimator.viewer.numReads<<"\t"<<Estimator.viewer.avgDepth<<"\t"
        <<((Estimator.fn.globalAlpha < 0.5) ? Estimator.fn.globalAlpha:(1.f-Estimator.fn.globalAlpha))<<"\t"<<-Estimator.fn.llk1<<"\t"<<-Estimator.fn.llk0<<"\t"<<"NA\tNA\t"
        <<"NA\tNA\tNA\tNA\tNA\t"
        <<"NA\tNA\tNA"<<std::endl;


    return 0;
}

// main function of verifyBamID
int main(int argc, char** argv) {
    fprintf(stderr, "VerifyBamID2: A robust tool for DNA contamination estimation from sequence reads using ancestry-agnostic method.\n\n");
    fprintf(stderr, " Copyright (c) 2009-2018 by Hyun Min Kang and Fan Zhang\n");
    fprintf(stderr, " This project is licensed under the terms of the MIT license.\n");

    int returnVal = 0;
    String compStatus;
    PhoneHome::allThinning = 50;
    try
    {
        returnVal = execute(argc, argv);
    }
    catch(std::runtime_error e)
    {
        returnVal = -1;
        compStatus = "Exception";
        PhoneHome::completionStatus(compStatus.c_str(),"VerifyBamID2");
        std::string errorMsg = "Exiting due to ERROR:\n\t";
        errorMsg += e.what();
        std::cerr << errorMsg << std::endl;
        return returnVal;
    }
    compStatus = returnVal;
    PhoneHome::completionStatus(compStatus.c_str(),"VerifyBamID2");
    return(returnVal);
}
