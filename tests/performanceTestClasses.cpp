#include "performanceTestClasses.h"

void performanceTest<>::runTests ( unsigned int repetitions )
{
    cout << "Running test case " << name << std::endl;
    for ( int param = parameters.size() - 1; param >= 0; --param ) {
        unsigned int steps = getStepsForParam ( param );
        ofstream temp ( "temp.dat" );

        for ( unsigned int step = 0; step < steps; ++step ) {
            string params = getParamsString ( param, step );
            stringstream call;
            call << "./membrain-performancetests " << repetitions << " " << name << " " << params;
            cout << "Calling: " << call.str() << endl;
            system ( call.str().c_str() );

            resultToTempFile ( param, step, temp );
            temp << endl;
        }

        temp.close();
        ofstream gnutemp ( "temp.gnuplot" );
        stringstream outname;
        outname << name << param << ".eps";
        cout << "Generating output file " << outname.str() << endl;
        gnutemp << generateGnuplotScript ( outname.str(), parameters[param]->name, "Execution time [ms]", name, parameters[param]->deltaLog );
        gnutemp.close();

        cout << "Calling gnuplot and displaying result" << endl;
        system ( "gnuplot temp.gnuplot" );
        system ( ( "convert -density 300 -resize 1920x " + outname.str() + ".eps -flatten " + outname.str() + ".png" ).c_str() );
        system ( ( "display " + outname.str() + ".png &" ).c_str() );
    }
}

string performanceTest<>::getParamsString ( int varryParam, unsigned int step, const string &delimiter )
{
    stringstream ss;
    for ( int i = parameters.size() - 1; i >= 0; --i ) {
        if ( i == varryParam ) {
            ss << parameters[i]->valueAsString ( step );
        } else {
            ss << parameters[i]->valueAsString();
        }
        ss << delimiter;
    }
    return ss.str();
}

string performanceTest<>::getTestOutfile ( int varryParam, unsigned int step )
{
    stringstream ss;
    ss << "perftest_" << name;
    for ( int i = parameters.size() - 1; i >= 0; --i ) {
        if ( i == varryParam ) {
            ss << "#" << parameters[i]->valueAsString ( step );
        } else {
            ss << "#" << parameters[i]->valueAsString();
        }
    }
    return ss.str();
}

void performanceTest<>::resultToTempFile ( int varryParam, unsigned int step, ofstream &file )
{
    file << getParamsString ( varryParam, step, "\t" );
    ifstream test ( getTestOutfile ( varryParam, step ) );
    string line;
    while ( getline ( test, line ) ) {
        if ( line.find ( '#' ) == string::npos ) {
            stringstream ss ( line );
            vector<string> parts;
            string part;
            while ( getline ( ss, part, '\t' ) ) {
                parts.push_back ( part );
            }
            file << parts[parts.size() - 2] << '\t';
        }
    }
}

string performanceTest<>::generateGnuplotScript ( const string &name, const string &xlabel, const string &ylabel, const string &title, bool log )
{
    stringstream ss;
    ss << "set terminal postscript eps enhanced color 'Helvetica,10'" << endl;
    ss << "set output \"" << name << ".eps\"" << endl;
    ss << "set xlabel \"" << xlabel << "\"" << endl;
    ss << "set ylabel \"" << ylabel << "\"" << endl;
    ss << "set title \"" << title << "\"" << endl;
    if ( log ) {
        ss << "set log xy" << endl;
    } else {
        ss << "set log y" << endl;
    }
    ss << generateMyGnuplotPlotPart ( "temp.dat" );
    return ss.str();
}


string matrixTransposeTest::comment = "Measurements of allocation and definition, transposition, deletion times";
matrixTransposeTest matrixTransposeTestInstance;

matrixTransposeTest::matrixTransposeTest() : performanceTest<int, int> ( "MatrixTranspose" )
{
    parameter1.min = 10;
    parameter1.max = 10000;
    parameter1.steps = 20;
    parameter1.deltaLog = true;
    parameter1.mean = 8000;
    parameter1.name = "Matrix size per dimension";

    parameter2.min = 10;
    parameter2.max = 10000;
    parameter2.steps = 20;
    parameter2.deltaLog = true;
    parameter2.mean = 2000;
    parameter2.name = "Matrix rows in main memory";
}

void matrixTransposeTest::actualTestMethod ( tester &test, int param1, int param2 )
{
    test.addComment ( comment.c_str() );

    const global_bytesize size = param1;
    const global_bytesize memlines = param2;
    const global_bytesize mem = size * sizeof ( double ) *  memlines;
    const global_bytesize swapmem = size * size * sizeof ( double ) * 2;

    membrainglobals::config.resizeMemory ( mem );
    membrainglobals::config.resizeSwap ( swapmem );

    test.addTimeMeasurement();

    // Allocate and set
    managedPtr<double> *rows[size];
    for ( unsigned int i = 0; i < size; ++i ) {
        rows[i] = new managedPtr<double> ( size );
        adhereTo<double> rowloc ( *rows[i] );
        double *rowdbl =  rowloc;
        for ( unsigned int j = 0; j < size; ++j ) {
            rowdbl[j] = i * size + j;
        }
    }

    test.addTimeMeasurement();

    // Transpose
    for ( unsigned int i = 0; i < size; ++i ) {
        adhereTo<double> rowloc1 ( *rows[i] );
        double *rowdbl1 =  rowloc1;
        for ( unsigned int j = i + 1; j < size; ++j ) {
            adhereTo<double> rowloc2 ( *rows[j] );
            double *rowdbl2 =  rowloc2;

            double buffer = rowdbl1[j];
            rowdbl1[j] = rowdbl2[i];
            rowdbl2[i] = buffer;
        }
    }

    test.addTimeMeasurement();


    // Delete
    for ( unsigned int i = 0; i < size; ++i ) {
        delete rows[i];
    }

    test.addTimeMeasurement();
}

string matrixTransposeTest::generateMyGnuplotPlotPart ( const string &file )
{
    stringstream ss;
    ss << "plot '" << file << "' using 1:3 with lines title \"Allocation & Definition\", \\" << endl;
    ss << "'" << file << "' using 1:4 with lines title \"Transposition\", \\" << endl;
    ss << "'" << file << "' using 1:5 with lines title \"Deletion\", \\" << endl;
    ss << "'" << file << "' using 1:($3+$4+$5) with lines title \"Total\"" << endl;
    return ss.str();
}

string matrixCleverTransposeTest::comment = "Measurements of allocation and definition, transposition, deletion times, but with a clever transposition algorithm";
matrixCleverTransposeTest matrixCleverTransposeTestInstance;

matrixCleverTransposeTest::matrixCleverTransposeTest() : performanceTest<int, int> ( "MatrixCleverTranspose" )
{
    parameter1.min = 10;
    parameter1.max = 10000;
    parameter1.steps = 20;
    parameter1.deltaLog = true;
    parameter1.mean = 8000;
    parameter1.name = "Matrix size per dimension";

    parameter2.min = 10;
    parameter2.max = 10000;
    parameter2.steps = 20;
    parameter2.deltaLog = true;
    parameter2.mean = 2000;
    parameter2.name = "Matrix rows in main memory";
}

void matrixCleverTransposeTest::actualTestMethod ( tester &test, int param1, int param2 )
{
    test.addComment ( comment.c_str() );

    const global_bytesize size = param1;
    const global_bytesize memlines = param2;
    const global_bytesize mem = size * sizeof ( double ) *  memlines;
    const global_bytesize swapmem = size * size * sizeof ( double ) * 2;

    membrainglobals::config.resizeMemory ( mem );
    membrainglobals::config.resizeSwap ( swapmem );

    test.addTimeMeasurement();

    // Allocate and set
    managedPtr<double> *rows[size];
    for ( unsigned int i = 0; i < size; ++i ) {
        rows[i] = new managedPtr<double> ( size );
        adhereTo<double> rowloc ( *rows[i] );
        double *rowdbl =  rowloc;
        for ( unsigned int j = 0; j < size; ++j ) {
            rowdbl[j] = i * size + j;
        }
    }

    test.addTimeMeasurement();

    // Transpose blockwise
    unsigned int rows_fetch = memlines / 2 > size ? size : memlines / 2;
    unsigned int n_blocks = size / rows_fetch + ( size % rows_fetch == 0 ? 0 : 1 );

    adhereTo<double> *Arows[rows_fetch];
    adhereTo<double> *Brows[rows_fetch];

    for ( unsigned int jj = 0; jj < n_blocks; jj++ ) {
        for ( unsigned int ii = 0; ii <= jj; ii++ ) {
            //A_iijj <-> B_jjii

            //Reserve rows ii and jj
            unsigned int i_lim = ( ii + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, vertical limit
            unsigned int j_lim = ( jj + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, horizontal limit
            unsigned int i_off = ii * rows_fetch; // Block A, vertical index
            unsigned int j_off = jj * rows_fetch; // Block A, horizontal index

            //Get rows A_ii** and B_jj** into memory:
            for ( unsigned int i = 0; i < i_lim; ++i ) {
                Arows[i] = new adhereTo<double> ( *rows[i + i_off] );
            }
            for ( unsigned int j = 0; j < j_lim; ++j ) {
                Brows[j] = new adhereTo<double> ( *rows[j + j_off] );
            }

            for ( unsigned int j = 0; j < j_lim; j++ ) {
                for ( unsigned int i = 0; i < ( jj == ii ? j : i_lim ); i++ ) { //Inner block matrix transpose, vertical index in A
                    //Inner block matrxi transpose, horizontal index in A
                    double *Arowdb = *Arows[i]; //Fetch pointer for Element of A_ii+i
                    double *Browdb = *Brows[j];

                    double inter = Arowdb[j_off + j]; //Store inner element A_ij
                    Arowdb[j + j_off] = Browdb[ i + i_off]; //Override with element of B_ji
                    Browdb[i + i_off] = inter; //set B_ji to former val of A_ij
                }
            }

            for ( unsigned int i = 0; i < i_lim; ++i ) {
                delete ( Arows[i] );
            }
            for ( unsigned int j = 0; j < j_lim; ++j ) {
                delete ( Brows[j] );
            }
        }
    }

    test.addTimeMeasurement();

//     for ( unsigned int i = 0; i < size; ++i ) {
//         adhereTo<double> rowloc ( *rows[i] );
//         double *rowdbl =  rowloc;
//         for ( unsigned int j = 0; j < size; ++j ) {
//              if(rowdbl[j] != j * size + i)
//                  printf("Failed check!");
//         }
//     }

    // Delete
    for ( unsigned int i = 0; i < size; ++i ) {
        delete rows[i];
    }

    test.addTimeMeasurement();
}

string matrixCleverTransposeTest::generateMyGnuplotPlotPart ( const string &file )
{
    stringstream ss;
    ss << "plot '" << file << "' using 1:3 with lines title \"Allocation & Definition\", \\";
    ss << "'" << file << "' using 1:4 with lines title \"Transposition\", \\";
    ss << "'" << file << "' using 1:5 with lines title \"Deletion\", \\";
    ss << "'" << file << "' using 1:($3+$4+$5) with lines title \"Total\"";
    return ss.str();
}

string matrixCleverTransposeOpenMPTest::comment = "Same as cleverTranspose, but with OpenMP";
matrixCleverTransposeOpenMPTest matrixCleverTransposeOpenMPTestInstance;

matrixCleverTransposeOpenMPTest::matrixCleverTransposeOpenMPTest() : performanceTest<int, int> ( "MatrixCleverTransposeOpenMP" )
{
    parameter1.min = 10;
    parameter1.max = 10000;
    parameter1.steps = 20;
    parameter1.deltaLog = true;
    parameter1.mean = 8000;
    parameter1.name = "Matrix size per dimension";

    parameter2.min = 10;
    parameter2.max = 10000;
    parameter2.steps = 20;
    parameter2.deltaLog = true;
    parameter2.mean = 2000;
    parameter2.name = "Matrix rows in main memory";
}

void matrixCleverTransposeOpenMPTest::actualTestMethod ( tester &test, int param1, int param2 )
{
    test.addComment ( comment.c_str() );

    const global_bytesize size = param1;
    const global_bytesize memlines = param2;
    const global_bytesize mem = size * sizeof ( double ) *  memlines;
    const global_bytesize swapmem = size * size * sizeof ( double ) * 4;

    //managedFileSwap swap ( swapmem, "membrainswap-%d" );
    //cyclicManagedMemory manager ( &swap, mem );
    membrainglobals::config.resizeMemory ( mem );
    membrainglobals::config.resizeSwap ( swapmem );

    test.addTimeMeasurement();

    // Allocate and set
    managedPtr<double> *rows[size];
    #pragma omp parallel for
    for ( unsigned int i = 0; i < size; ++i ) {
        rows[i] = new managedPtr<double> ( size );
        adhereTo<double> rowloc ( *rows[i] );
        double *rowdbl =  rowloc;
        for ( unsigned int j = 0; j < size; ++j ) {
            rowdbl[j] = i * size + j;
        }
    }

    test.addTimeMeasurement();

    // Transpose blockwise, leave a bit free space, if not, we're stuck in the process...

    unsigned int rows_fetch = memlines / ( 4 ) > size ? size : memlines / ( 4 );
    unsigned int n_blocks = size / rows_fetch + ( size % rows_fetch == 0 ? 0 : 1 );

    adhereTo<double> *Arows[rows_fetch];
    adhereTo<double> *Brows[rows_fetch];

    for ( unsigned int jj = 0; jj < n_blocks; jj++ ) {
        for ( unsigned int ii = 0; ii <= jj; ii++ ) {
            //A_iijj <-> B_jjii

            //Reserve rows ii and jj
            unsigned int i_lim = ( ii + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, vertical limit
            unsigned int j_lim = ( jj + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, horizontal limit
            unsigned int i_off = ii * rows_fetch; // Block A, vertical index
            unsigned int j_off = jj * rows_fetch; // Block A, horizontal index

            //Get rows A_ii** and B_jj** into memory:
            #pragma omp parallel for
            for ( unsigned int i = 0; i < i_lim; ++i ) {
                Arows[i] = new adhereTo<double> ( *rows[i + i_off], true );
            }
            #pragma omp parallel for
            for ( unsigned int j = 0; j < j_lim; ++j ) {
                Brows[j] = new adhereTo<double> ( *rows[j + j_off], true );
            }
            #pragma omp parallel for
            for ( unsigned int j = 0; j < j_lim; j++ ) {
                for ( unsigned int i = 0; i < ( jj == ii ? j : i_lim ); i++ ) { //Inner block matrix transpose, vertical index in A
                    //Inner block matrxi transpose, horizontal index in A
                    double *Arowdb = *Arows[i]; //Fetch pointer for Element of A_ii+i
                    double *Browdb = *Brows[j];

                    double inter = Arowdb[j_off + j]; //Store inner element A_ij
                    Arowdb[j + j_off] = Browdb[ i + i_off]; //Override with element of B_ji
                    Browdb[i + i_off] = inter; //set B_ji to former val of A_ij
                }
            }
            #pragma omp parallel for
            for ( unsigned int i = 0; i < i_lim; ++i ) {
                delete ( Arows[i] );
            }
            #pragma omp parallel for
            for ( unsigned int j = 0; j < j_lim; ++j ) {
                delete ( Brows[j] );
            }
        }
    }


    test.addTimeMeasurement();

    //     for ( unsigned int i = 0; i < size; ++i ) {
    //         adhereTo<double> rowloc ( *rows[i] );
    //         double *rowdbl =  rowloc;
    //         for ( unsigned int j = 0; j < size; ++j ) {
    //              if(rowdbl[j] != j * size + i)
    //                  printf("Failed check!");
    //         }
    //     }

    // Delete
    #pragma omp parallel for
    for ( unsigned int i = 0; i < size; ++i ) {
        delete rows[i];
    }
    test.addTimeMeasurement();
}

string matrixCleverTransposeOpenMPTest::generateMyGnuplotPlotPart ( const string &file )
{
    stringstream ss;
    ss << "plot '" << file << "' using 1:3 with lines title \"Allocation & Definition\", \\";
    ss << "'" << file << "' using 1:4 with lines title \"Transposition\", \\";
    ss << "'" << file << "' using 1:5 with lines title \"Deletion\", \\";
    ss << "'" << file << "' using 1:($3+$4+$5) with lines title \"Total\"";
    return ss.str();
}

string matrixCleverBlockTransposeOpenMPTest::comment = "Same as cleverTranspose, but with OpenMP and blockwise multiplication";
matrixCleverBlockTransposeOpenMPTest matrixCleverBlockTransposeOpenMPTestInstance;

matrixCleverBlockTransposeOpenMPTest::matrixCleverBlockTransposeOpenMPTest() : performanceTest<int, int> ( "MatrixCleverBlockTransposeOpenMP" )
{
    parameter1.min = 10;
    parameter1.max = 10000;
    parameter1.steps = 20;
    parameter1.deltaLog = true;
    parameter1.mean = 8000;
    parameter1.name = "Matrix size per dimension";

    parameter2.min = 10;
    parameter2.max = 10000;
    parameter2.steps = 20;
    parameter2.deltaLog = true;
    parameter2.mean = 2000;
    parameter2.name = "Matrix rows in main memory";
}

void matrixCleverBlockTransposeOpenMPTest::actualTestMethod ( tester &test, int param1, int param2 )
{
    test.addComment ( comment.c_str() );

    const global_bytesize size = param1;
    const global_bytesize memlines = param2;
    const global_bytesize mem = size * sizeof ( double ) *  memlines;
    const global_bytesize swapmem = size * size * sizeof ( double ) * 4;

    //managedFileSwap swap ( swapmem, "membrainswap-%d" );
    //cyclicManagedMemory manager ( &swap, mem );
    membrainglobals::config.resizeMemory ( mem );
    membrainglobals::config.resizeSwap ( swapmem );


    test.addTimeMeasurement();

    // Transpose blockwise, leave a bit free space, if not, we're stuck in the process...

    unsigned int rows_fetch = sqrt ( memlines * size / 2 );
    unsigned int blocksize = rows_fetch * rows_fetch;

    unsigned int n_blocks = size / rows_fetch + ( size % rows_fetch == 0 ? 0 : 1 );

    //Blocks are rows_fetch² matrices stored in n_blocks² blocks.

#define blockIdx(x,y) ((x/rows_fetch)*n_blocks+y/rows_fetch)
#define inBlockX(x,y) (x%rows_fetch)
#define inBlockY(x,y) (y%rows_fetch)
#define inBlockIdx(x,y) (inBlockX(x,y)*rows_fetch+inBlockY(x,y))
    // Allocate and set
    managedPtr<double> *rows[n_blocks * n_blocks];
    for ( unsigned int jj = 0; jj < n_blocks; jj++ ) {
        for ( unsigned int ii = 0; ii < n_blocks; ii++ ) {
            rows[ii * n_blocks + jj] = new managedPtr<double> ( blocksize );
            adhereTo<double> adh ( *rows[ii * n_blocks + jj] );
            double *locPtr = adh;
            unsigned int i_lim = ( ii + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, vertical limit
            unsigned int j_lim = rows_fetch;//( jj + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, horizontal limit
            #pragma omp parallel for
            for ( unsigned int i = 0; i < i_lim; i++ ) {
                for ( unsigned int j = 0; j < j_lim; j++ ) {
                    locPtr[i * rows_fetch + j] = ( ii * rows_fetch + i ) * size + ( j + rows_fetch * jj );
                }
            }
        }
    }
    test.addTimeMeasurement();
//     for ( unsigned int i = 0; i < size; ++i ) {
//         for ( unsigned int j = 0; j < size; ++j ) {
//             unsigned int blckidx = blockIdx(i,j);
//             unsigned int inblck = inBlockIdx(i,j);
//             adhereTo<double> adh(*rows[blckidx]);
//             double * loc = adh;
//             //             if(loc[inblck] != j * size + i)
//             //                 printf("Failed check!");
//             //printf("%d,%d\t",blckidx,inblck);
//             printf("%e\t",loc[inblck]);
//         }
//         printf("\n");
//     }

    for ( unsigned int jj = 0; jj < n_blocks; jj++ ) {
        for ( unsigned int ii = 0; ii <= jj; ii++ ) {
            //A_iijj <-> B_jjii

            //Reserve rows ii and jj
            unsigned int i_lim = ( ii + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, vertical limit
            unsigned int j_lim = ( jj + 1 == n_blocks && size % rows_fetch != 0 ? size % rows_fetch : rows_fetch ); // Block A, horizontal limit

            //Get rows A_ii** and B_jj** into memory:
            adhereTo<double> aBlock ( *rows[ii * n_blocks + jj] );
            adhereTo<double> bBlock ( *rows[jj * n_blocks + ii] );

            double *aLoc = aBlock;
            double *bLoc = bBlock;

            #pragma omp parallel for
            for ( unsigned int j = 0; j < j_lim; j++ ) {
                for ( unsigned int i = 0; i < ( jj == ii ? j : i_lim ); i++ ) { //Inner block matrix transpose, vertical index in A
                    //Inner block matrxi transpose, horizontal index in A
                    double inter = aLoc[i * rows_fetch + j]; //Store inner element A_ij
                    aLoc[i * rows_fetch + j] = bLoc[j * rows_fetch + i]; //Override with element of B_ji
                    bLoc[j * rows_fetch + i] = inter; //set B_ji to former val of A_ij
                }
            }
        }
    }

    test.addTimeMeasurement();
// //      printf("\n");
//     for ( unsigned int i = 0; i < size; ++i ) {
//         for ( unsigned int j = 0; j < size; ++j ) {
//             unsigned int blckidx = blockIdx(i,j);
//             unsigned int inblck = inBlockIdx(i,j);
//             adhereTo<double> adh(*rows[blckidx]);
//             double * loc = adh;
//                         if(loc[inblck] != j * size + i)
//                             printf("Failed check!");
// //             printf("%d,%d\t",blckidx,inblck);
// //             printf("%e\t",loc[inblck]);
//         }
// //         printf("\n");
//     }




    // Delete
    #pragma omp parallel for
    for ( unsigned int i = 0; i < n_blocks * n_blocks; ++i ) {
        delete rows[i];
    }

    test.addTimeMeasurement();
}

string matrixCleverBlockTransposeOpenMPTest::generateMyGnuplotPlotPart ( const string &file )
{
    stringstream ss;
    ss << "plot '" << file << "' using 1:3 with lines title \"Allocation & Definition\", \\";
    ss << "'" << file << "' using 1:4 with lines title \"Transposition\", \\";
    ss << "'" << file << "' using 1:5 with lines title \"Deletion\", \\";
    ss << "'" << file << "' using 1:($3+$4+$5) with lines title \"Total\"";
    return ss.str();
}
