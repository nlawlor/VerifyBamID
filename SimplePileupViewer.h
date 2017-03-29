//
// Created by Fan Zhang on 2/5/16.
//

#ifndef CONTAMINATIONFINDER_SIMPLEPILEUP_H
#define CONTAMINATIONFINDER_SIMPLEPILEUP_H
#include "sam_opts.h"
#include "htslib/faidx.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

typedef struct {
    int min_mq, flag, min_baseQ, capQ_thres, max_depth, max_indel_depth, fmt_flag;
    int rflag_require, rflag_filter;
    int openQ, extQ, tandemQ, min_support; // for indels
    double min_frac; // for indels
    char *reg, *pl_list, *fai_fname, *output_fname;
    faidx_t *fai;
    void *bed, *rghash;
    int argc;
    char **argv;
    sam_global_args ga;
} mplp_conf_t;


typedef std::vector<std::vector<char> >BaseInfo;
typedef std::vector<std::vector<char> >QualInfo;

class ContaminationEstimator;
class SVDcalculator;
struct region_t{
    std::string chr;
    int beg;
    int end;
    region_t(std::string chr0, int beg0, int end0)
    {
        chr=chr0;
        beg=beg0;
        end=end0;
    }
    region_t()
    {
        chr="";
        beg=0;
        end=0;
    }
} ;


class SimplePileupViewer {

public:

    int regIndex;
    std::vector<region_t>* BedVec;

    mplp_conf_t mplp;

    BaseInfo baseInfo;
    QualInfo qualInfo;

    std::string SEQ_SM;
    int numReads;
    float avgDepth;


    std::unordered_map<std::string,std::unordered_map<int32_t,int32_t> > posIndex;

    SimplePileupViewer();

    SimplePileupViewer(std::vector<region_t>* A,const char *bamFile, const char* faiFile, const char* bedFile,int nfiles=1);

    int SIMPLEmpileup(mplp_conf_t *conf, int n, char **fn);

    region_t GetNextRegion(bool& BedEOF)
    {
        BedEOF=false;
        if(regIndex<BedVec->size()) {
            if(regIndex==(BedVec->size()-1)) BedEOF=true;
            return BedVec->at(regIndex++);
        }
        else {
            std::cerr<<"Region number out of bound!\n"<<std::endl;
            exit(EXIT_FAILURE);
        }
    }

    int GetNumMarker()
    {
        return BedVec->size();
    }

    inline std::vector<char>& GetBaseInfoAt(std::string& chr, int32_t pos)
    {
        return baseInfo[posIndex[chr][pos]];
    }
    inline std::vector<char>& GetQualInfoAt(std::string& chr, int32_t pos)
    {
        return qualInfo[posIndex[chr][pos]];
    }

    virtual ~SimplePileupViewer();
};


#endif //CONTAMINATIONFINDER_SIMPLEPILEUP_H
