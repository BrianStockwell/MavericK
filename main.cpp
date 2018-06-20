//
//  MavericK
//  main.cpp
//
//  Created by Bob on 24/11/2015.
//  Distributed under the MIT software licence - see Notes.c file for details
//
//  MavericK is a program for carring out analysis of population structure using Bayesian mixture modelling. What separates MavericK from other similar programs (such as STRUCTURE) is the ability to estimate the evidence for K via thermodynamic integration. There are also some differences in terms of implementation of the core algorithm, and available inputs and outputs.
//
//  See the Notes.c file for details of overall program structure
//
// ---------------------------------------------------------------------------
/*
 Ideas for how to speed up and reorganise program:
 
 - data and groupings start from index 0
 - think about listing order of e.g. alleleCounts[k][l][j]. Could this order be rearranged to make faster inside likelihood calculation?
 - fancy mixture modelling techniques, such as linking over K
 - do I need alleleCountsTotals at all? (Yes, I think so). What about admixCountsTotals?
 - currently I am temporarily adding and subtracting elements when calculating likelihood, then adding again once K chosen. Avoid add-subtract-add step if K does not change? Under admixture model?
*/
// NOTE - now using shortcut method of label fixing, in which condition probabilities within an iteration are not recalculated pior to using Stephens' method.

// include standard library header files
#include <iostream>
#include <ctime>

// include MavericK header files
#include "exhaustive.h"
#include "globals.h"
#include "Hungarian.h"
#include "misc.h"
#include "OSfunctions.h"
#include "probability.h"
#include "run_MCMC.h"
#include "readIn.h"
#include "writeOut.h"

using namespace std;

// create global objects
vector< vector<double> > log_lookup;
vector<double> log_lookup_0;

// main function
int main(int argc, const char * argv[]) {
    
    // start program
    time_t ctt = time(0); // current time
    cout << "------------------------------------------\n";
    cout << "               MAVERICK\n";
    cout << "by Robert Verity and Richard A. Nichols\n";
    cout << "      Version 1.1.0 (22 June 2016)\n";
    cout << "accessed " << ctime(&ctt);
    cout << "------------------------------------------\n\n";
    
    // start timing program
    time_t tstart, tend;
    time(&tstart);
    
    //---------------------------------------------------------------------------------------------------
    
    // The logic of importing parameter values from file proceeds in the following steps:
    //  1. Define default values for all parameters in the form of pair<string,int> objects. The first element stores the default value of the parameter as a string, while the second element is used to keep track of where the parameter is defined (1=default, 2=parameters file, 3=command line). Note that all parameter values are stored as strings at this stage.
    //  2. Look through command line arguments. Read just those arguments needed to locate the parameters file.
    //  3. Read in the parameters file (all values are read in as strings). For all parameters in this file that match valid parameter names, overwrite the pair<string,int> with new values, and set the int element to 2 to indicate that these have been read in from file.
    //  4. Look through all remaining command line arguments. For all arguments that match valid parameter names, overwrite the pair<string,int> with new values, and set the int element to 3 to indicate that these have been defined on the command line.
    // At this stage we have all parameters defined as strings, and a record of where they were defined. No checking of parameter values has occurred yet.
    
    // initialise global object; to contain all file paths, parameter values and data.
    globals globals;
    
    // parse input arguments required to open parameters file
    for (int i=1; i<argc; i++) {
        readPath("-masterRoot", globals.masterRoot_filePath, argc, argv, i);
        readPath("-inputRoot", globals.inputRoot_fileName, argc, argv, i);
        readPath("-parameters", globals.parameters_fileName, argc, argv, i);
    }
    globals.inputRoot_filePath = globals.masterRoot_filePath + globals.inputRoot_fileName;
    globals.parameters_filePath = globals.inputRoot_filePath + globals.parameters_fileName;
    
    // read in parameters file
    readParameters(globals);
    
    // read command line arguments
    readCommandLine(globals, argc, argv);
    
    //---------------------------------------------------------------------------------------------------
    
    // Open log file if needed. Loop through all parameters in turn, checking values and printing errors if necessary.
    
    // check that outputLog_on parameter is valid
    checkBoolean(globals.parameterStrings["outputLog_on"].first, globals.outputLog_on, "outputLog_on", false, globals.outputLog_fileStream);
    
    // open log if necessary
    if (globals.outputLog_on) {
        
        globals.outputLog_fileStream = safe_ofstream(globals.outputLog_filePath, false, globals.outputLog_fileStream);
        
        globals.outputLog_fileStream << "------------------------------------------\n";
        globals.outputLog_fileStream << "               MAVERICK\n";
        globals.outputLog_fileStream << "by Robert Verity and Richard A. Nichols\n";
        globals.outputLog_fileStream << "      Version 1.1.0 (22 June 2016)\n";
		globals.outputLog_fileStream << "accessed " << ctime(&ctt);
        globals.outputLog_fileStream << "------------------------------------------\n\n";
        
        globals.outputLog_fileStream << "Parameters file: " << globals.parameters_filePath << "\n\n";
        globals.outputLog_fileStream << "Data file: " << globals.data_filePath << "\n";
        
        globals.outputLog_fileStream.flush();
    }
    
    // check parameters that are set to default values and print to log
    writeToFile("\nParameters taking default values\n", globals.outputLog_on, globals.outputLog_fileStream);
    checkParameters(globals, 0);
    
    // check parameters that are read in from file and print to log
    writeToFile("\nParameters read in from file\n", globals.outputLog_on, globals.outputLog_fileStream);
    checkParameters(globals, 1);
    
    // check parameters that are defined as command line arguments and print to log
    writeToFile("\nParameters defined on command line\n", globals.outputLog_on, globals.outputLog_fileStream);
    checkParameters(globals, 2);
    coutAndLog("\n", globals.outputLog_on, globals.outputLog_fileStream);
    
    //---------------------------------------------------------------------------------------------------
    
    // Read in data file. Ensure that data is formatted correctly, and that the chosen combination of data and parameters makes sense. Define lookup tables.
    
    // read in data and check format
    readData(globals);
    
    // create lookup table for log(i+(j+1)*lambda) function. The object log_lookup_0 does the same thing for j=0 only (it is slightly faster to index this vector rather than the first element of an array, as in log_lookup[i][0]).
    int Jmax = *max_element(begin(globals.J),end(globals.J));
    log_lookup = vector< vector<double> >(int(1e4),vector<double>(Jmax+1));
    log_lookup_0 = vector<double>(int(1e4));
    for (int i=0; i<int(1e4); i++) {
        for (int j=0; j<Jmax; j++) {
            log_lookup[i][j] = log(double(i+(j+1)*globals.lambda));
        }
        log_lookup_0[i] = log_lookup[i][0];
    }
    
    // check that chosen options make sense
    checkOptions(globals);
    
    //---------------------------------------------------------------------------------------------------
    
    // Loop through defined range of K, deploying various statistical methods.
    
    // initialise objects for storing results
    initialiseGlobals(globals);
    
    // open file streams that are common to all K
    openFileStreams(globals);
    
    // loop through range of K
    for (int Kindex=0; Kindex<(globals.Kmax-globals.Kmin+1); Kindex++) {
        int K = globals.Kmin+Kindex;
        
        coutAndLog("-- K="+to_string((long long)K)+" ----------------\n\n", globals.outputLog_on, globals.outputLog_fileStream);
        
        //#### Carry out various estimation methods
        
        // exhaustive analysis
        if (globals.exhaustive_on || K==1) {
            coutAndLog("Running exhaustive approach...\n", globals.outputLog_on, globals.outputLog_fileStream);
            if (!globals.admix_on) {
                exhaustive_noAdmix(globals, Kindex);
            } else {
                exhaustive_admix(globals, Kindex);
            }
            coutAndLog("  complete\n\n", globals.outputLog_on, globals.outputLog_fileStream);
        }
        
        // main MCMC (including thermodynamic integration)
        coutAndLog("Carrying out thermodynamic integration...\n", globals.outputLog_on, globals.outputLog_fileStream);
        if (!globals.admix_on) {
            run_MCMC_noAdmixture(globals, Kindex);
        } else {
            run_MCMC_admixture(globals, Kindex);
        }
        coutAndLog("  complete\n\n", globals.outputLog_on, globals.outputLog_fileStream);
        
        
        //#### Print results to file
        
        // output Qmatrix files for this K
        if (globals.outputQmatrix_gene_on)
            printQmatrix_gene(globals, Kindex);
        if (globals.outputQmatrix_ind_on)
            printQmatrix_ind(globals, Kindex);
        if (globals.outputQmatrix_pop_on)
            printQmatrix_pop(globals, Kindex);
        
        // output Qmatrix error files for this K
        if (globals.outputQmatrixError_gene_on)
            printQmatrixError_gene(globals, Kindex);
        if (globals.outputQmatrixError_ind_on)
            printQmatrixError_ind(globals, Kindex);
        if (globals.outputQmatrixError_pop_on)
            printQmatrixError_pop(globals, Kindex);
        
        // print evidence to file
        printEvidence(globals, Kindex);
        
        // print evidence details to file
        if (globals.outputEvidenceDetails_on)
            printEvidenceDetails(globals, Kindex);
        
        //#### Report answers from various estimation methods to console and to log
        
        coutAndLog("Estimates of (log) model evidence...\n\n", globals.outputLog_on, globals.outputLog_fileStream);
        
        // exhaustive
        if (globals.exhaustive_on)
            coutAndLog("Exhaustive\n  exact value: "+process_nan(globals.logEvidence_exhaustive[Kindex])+"\n\n", globals.outputLog_on, globals.outputLog_fileStream);
        
        // thermodynamic integral estimator
        coutAndLog("Thermodynamic integral estimator\n", globals.outputLog_on, globals.outputLog_fileStream);
        coutAndLog("  estimate: "+process_nan(globals.logEvidence_TI[Kindex])+"\n", globals.outputLog_on, globals.outputLog_fileStream);
        coutAndLog("  standard error: "+process_nan(globals.logEvidence_TI_SE[Kindex])+"\n\n", globals.outputLog_on, globals.outputLog_fileStream);
        
		fflush(stdout);
        
    } // end loop through K
    
    // print normalised evidence to file
    if (globals.outputEvidenceNormalised_on) {
        printEvidenceNormalised(globals);
    }
    
    // end program
    time(&tend);
    double duration = difftime(tend, tstart);
    ostringstream duration_ss;
    duration_ss << "Program completed in approximately " << duration << " seconds\n";
    string duration_s = duration_ss.str();
    if (duration<0) {
        coutAndLog("Program completed in less than 1 second\n", globals.outputLog_on, globals.outputLog_fileStream);
    } else {
        coutAndLog(duration_s, globals.outputLog_on, globals.outputLog_fileStream);
    }
    coutAndLog("Output written to: "+globals.outputRoot_filePath+string("\n"), globals.outputLog_on, globals.outputLog_fileStream);
    coutAndLog("------------------------------------------\n", globals.outputLog_on, globals.outputLog_fileStream);

    //pauseExit();
    return(0);
}

