#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>

#include "tester.h"
#include "performanceTestClasses.h"

using namespace std;

/**
 * @brief Provides a binary to run performance tests
 * @param argc Expects at least two arguments
 * @param argv 1st: # of test repetitions; 2nd and forth: Names of performance tests to be carried out followed by test parameters
 */
int main ( int argc, char **argv )
{
    cout << "Starting performance tests" << endl;
    cout << "Called with: ";
    for ( int i = 0; i < argc; ++i ) {
        cout << argv[i] << " ";
    }
    cout << endl << endl;

    int i = 1;
    if ( i >= argc ) {
        cerr << "Not enough arguments supplied, expected number of repetitions followed by test directives, exiting" << endl;
        return 2;
    }

    int repetitions = atoi ( argv[i++] );

    while ( i < argc ) {
        cout << "Attempting to run " << argv[i] << endl;
        tester myTester ( argv[i] );
        int j;
        for ( j = i + 1; j < argc; ++j ) {
            myTester.addParameter ( argv[j] );
        }

        ++i;
        bool success = performanceTest<>::runRespectiveTest ( argv[i - 1], myTester, repetitions, argv, i, argc );

        if ( success ) {
            myTester.writeToFile();
        }
    }

    cout << "Performance tests done" << endl;
    return 0;
}
