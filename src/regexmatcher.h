#ifndef REGEXMATCHER_H
#define REGEXMATCHER_H

#include <regex>
#include <string>
#include <map>

using namespace std;

namespace membrain
{

/**
 * @brief Class to handle regex matching used for parsing configuration files
 */
class regexMatcher
{

public:

    /**
     * @brief An enum to caracterise what shall be matched in a specific case; flaggy
     */
    enum matchType {
        integer = 1 << 0,
        floating = 1 << 1,
        units = 1 << 2,
        text = 1 << 3,
        boolean = 1 << 4
    };

    /**
     * @brief Create a new regex handler, basically a dummy
     */
    regexMatcher();

    /**
     * @brief Checks if a string matches the header of a configuration block
     * @param str The source string
     * @param blockname The config block name to check for
     * @return Result of matching
     */
    bool matchConfigBlock ( const string &str, const string &blockname = "default" ) const;

    /**
     * @brief Checks if a string matches something like key = value
     * @param str The source string
     * @param valueType Which matchType the value should be, does not match otherwise
     * @return Result key and value
     * @todo give possibility to give constraints on value, like must be a double or so
     */
    pair<string, string> matchKeyEqualsValue ( const string &str, int valueType = text ) const;

private:
    /**
     * @brief Create a matching regex for a certain type
     * @param type The type
     * @return The partial regex to match the type
     */
    string createRegexMatching ( int type ) const;

};

}

#endif // REGEXMATCHER_H
