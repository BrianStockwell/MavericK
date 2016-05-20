//
//  MavericK
//  writeOut.h
//
//  Created: Bob on 25/10/2015
//
//  Distributed under the MIT software licence - see Notes.c file for details
//
//  Functions for writing output to file.
//
// ---------------------------------------------------------------------------

#ifndef __Maverick1_0__writeOut__
#define __Maverick1_0__writeOut__

#include <iostream>
#include "globals.h"
#include "misc.h"
#include "OSfunctions.h"

//------------------------------------------------
// open file streams that are common to all K
void openFileStreams(globals &globals);

//------------------------------------------------
// write results to Evidence file
void printEvidence(globals &globals, int Kindex);

//------------------------------------------------
// write results to EvidenceDetails file
void printEvidenceDetails(globals &globals, int Kindex);

// write results to EvidenceNormalised file
void printEvidenceNormalised(globals &globals);

//------------------------------------------------
// write max-like allele frequencies to file
void printMaxLike_alleleFreqs(globals &globals, int Kindex);

//------------------------------------------------
// write max-like admixture frequencies to file
void printMaxLike_admixFreqs(globals &globals, int Kindex);

//------------------------------------------------
// write comparison statistics to file
void printComparisonStatistics(globals &globals, int Kindex);

//------------------------------------------------
// write Evanno's delta K to file
void printEvanno(globals &globals, int Kindex);


//------------------------------------------------
// write gene-level Qmatrix file (MCMCobject_admixture only)
void printQmatrix_gene(globals &globals, int Kindex);

//------------------------------------------------
// write standard error of gene-level Qmatrix (MCMCobject_admixture only)
void printQmatrixError_gene(globals &globals, int Kindex);

//------------------------------------------------
// write individual-level Qmatrix file (overloaded for MCMCobject_noAdmixture and MCMCobject_admixture)
void printQmatrix_ind(globals &globals, int Kindex);

//------------------------------------------------
// write standard error of individual-level Qmatrix (overloaded for MCMCobject_noAdmixture and MCMCobject_admixture)
void printQmatrixError_ind(globals &globals, int Kindex);

//------------------------------------------------
// write population-level Qmatrix file (overloaded for MCMCobject_noAdmixture and MCMCobject_admixture)
void printQmatrix_pop(globals &globals, int Kindex);

//------------------------------------------------
// write standard error of population-level Qmatrix (overloaded for MCMCobject_noAdmixture and MCMCobject_admixture)
void printQmatrixError_pop(globals &globals, int Kindex);

#endif
