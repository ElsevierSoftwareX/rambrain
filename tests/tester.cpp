#include "tester.h"
#include "managedMemory.h"
tester::tester ( const char *name ) : name ( name )
{
    startNewRNGCycle();
}

tester::~tester()
{
}

void tester::addParameter ( char *param )
{
    parameters.push_back ( param );
}

void tester::addTimeMeasurement()
{
#ifdef LOGSTATS
    rambrain::managedMemory::sigswapstats ( SIGUSR1 );
#endif
    std::chrono::high_resolution_clock::time_point t = std::chrono::high_resolution_clock::now();
    timeMeasures.back().push_back ( t );
}

void tester::addExternalTime ( chrono::duration< double > mdur )
{
    if ( timeMeasures.back().size() == 0 ) {
        std::chrono::high_resolution_clock::time_point t = std::chrono::high_resolution_clock::now();
        timeMeasures.back().push_back ( t );
    }

    std::chrono::high_resolution_clock::time_point t = timeMeasures.back().back();
    t += std::chrono::duration_cast< std::chrono::milliseconds > ( mdur );
    timeMeasures.back().push_back ( t );

}


void tester::addComment ( const char *comment )
{
    this->comment = std::string ( comment );
}

void tester::setSeed ( unsigned int seed )
{
    seeds.back() = seed;
    seeded.back() = true;
    srand48 ( seed );
    srand ( seed );

    std::cout << "I am running with a seed of " << seed << std::endl;
}

int tester::random ( int max ) const
{
    return static_cast<uint64_t> ( rand() ) * max / RAND_MAX;
}

uint64_t tester::random ( uint64_t max ) const
{
    return random ( static_cast<double> ( max ) );
}

double tester::random ( double max ) const
{
    return drand48() * max;
}

void tester::startNewTimeCycle()
{
    timeMeasures.push_back ( std::vector<std::chrono::high_resolution_clock::time_point>() );
}

void tester::startNewRNGCycle()
{
    seeds.push_back ( 0u );
    seeded.push_back ( false );
}

void tester::writeToFile()
{
    if ( std::string ( name ).empty() ) {
        std::cerr << "Can not write to file without file name" << std::endl;
        return;
    }

    std::stringstream fileName;
    fileName << name;
    for ( auto it = parameters.begin(); it != parameters.end(); ++it ) {
        fileName << "#" << *it;
    }
    std::ofstream out ( fileName.str(), std::ofstream::out );

    out << "# " << name;
    for ( auto it = parameters.begin(); it != parameters.end(); ++it ) {
        out << " " << *it;
    }
    out << std::endl;

    const int cyclesCount = timeMeasures.size();
    out << "# " << cyclesCount << " cycles run for average" << std::endl;

    bool firstSeed = true;
    for ( size_t i = 0; i < seeded.size(); ++i ) {
        if ( seeded[i] ) {
            if ( firstSeed ) {
                out << "# Random seeds: ";
                firstSeed = false;
            }
            out << seeds[i] << " ";
        } else {
            if ( ! firstSeed ) {
                out << "* ";
            }
        }
    }
    if ( !firstSeed ) {
        out << std::endl;
    }

    out << "# Columns: (Time [ms], Start [ms], End [ms], Percentage) per repetition run and afterwards for average (without Start, End)" << std::endl;

    if ( ! comment.empty() ) {
        out << "# " << comment << std::endl;
    }

    const int timesCount = timeMeasures.front().size() - 1;
    int64_t durations[timesCount][cyclesCount], starts[timesCount][cyclesCount], ends[timesCount][cyclesCount];
    double percentages[timesCount][cyclesCount];

    int cycle = 0, time;
    for ( auto repIt = timeMeasures.begin(); repIt != timeMeasures.end(); ++repIt, ++cycle ) {
        int64_t totms = std::chrono::duration_cast<std::chrono::milliseconds> ( repIt->back() - repIt->front() ).count();

        time = 0;
        for ( auto it = repIt->begin(), jt = repIt->begin() + 1; it != repIt->end() && jt != repIt->end(); ++it, ++jt, ++time ) {
            starts[time][cycle] = std::chrono::duration_cast<std::chrono::milliseconds> ( it->time_since_epoch() ).count();
            ends[time][cycle] = std::chrono::duration_cast<std::chrono::milliseconds> ( jt->time_since_epoch() ).count();
            durations[time][cycle] = std::chrono::duration_cast<std::chrono::milliseconds> ( ( *jt ) - ( *it ) ).count();
            percentages[time][cycle] = 100.0 * durations[time][cycle] / totms;
        }
    }

    int64_t avgTime;
    double avgPercentage;
    for ( time = 0; time < timesCount; ++time ) {

        avgTime = 0;
        avgPercentage = 0.0;

        for ( cycle = 0; cycle < cyclesCount; ++cycle ) {
            avgTime += durations[time][cycle];
            avgPercentage += percentages[time][cycle];

            out << durations[time][cycle] << "\t" << starts[time][cycle] << "\t" << ends[time][cycle] << "\t" << percentages[time][cycle] << "\t";
        }
        avgTime /= cyclesCount;
        avgPercentage /= cyclesCount;

        out << avgTime << "\t" << avgPercentage << std::endl;
    }

    out << std::flush;
}

std::vector<int64_t> tester::getDurationsForCurrentCycle() const
{
    std::vector<int64_t> durations;

    for ( auto it = timeMeasures.back().begin(), jt = timeMeasures.back().begin() + 1; it != timeMeasures.back().end() && jt != timeMeasures.back().end(); ++it, ++jt ) {
        durations.push_back ( std::chrono::duration_cast<std::chrono::milliseconds> ( ( *jt ) - ( *it ) ).count() );
    }

    return durations;
}
