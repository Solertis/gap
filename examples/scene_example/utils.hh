/*!
    \file examples/scene_example/utils.hh
    \brief Generic utilities for scene generation example

    \author João Borrego : jsbruglie
    \author Rui Figueiredo : ruipimentelfigueiredo
*/

// C libraries
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// C++ libraries

// I/O streams
#include <iostream>
// File system
#include <boost/filesystem.hpp>

// Custom debug utilities
#include "debug.hh"

// Definitions

// Default arg values

/// Default number of scenes
#define ARG_SCENES_DEFAULT      10
/// Default index of the first scene
#define ARG_START_DEFAULT       0
/// Default image directory
#define ARG_IMGS_DIR_DEFAULT    "imgs"
/// Default dataset directory
#define ARG_DATASET_DIR_DEFAULT "dataset"
/// Default value of debug flag
#define ARG_DEBUG_DEFAULT       false

// Function headers

/// \brief Returns string with program usage information
/// \return Program usage
const std::string getUsage(const char* argv_0);

/// \brief Parses command-line arguments
/// \param argc         Argument count
/// \param argv         Argument values
/// \param scenes       Number of scenes to generate
/// \param start        Index of the first scene
/// \param imgs_dir     Image output directory
/// \param dataset_dir  Dataset annotations output directory
/// \param debug        Debug flag
void parseArgs(
    int argc,
    char** argv,
    unsigned int & scenes,
    unsigned int & start,
    std::string & imgs_dir,
    std::string & dataset_dir,
    bool & debug);

/// \brief Creates the directory given its path
/// \param path The path string
/// \return true on success
bool createDirectory(std::string & path);

/// \brief Get a random integer in a given interval
///
/// Value is sampled from uniform distribution
///
/// \param min Interval lower bound
/// \param min Interval upper bound
/// \return Random integer
int getRandomInt(int min, int max);

/// \brief Get a random double in a given interval
///
/// Value is sampled from uniform distribution
///
/// \param min Interval lower bound
/// \param min Interval upper bound
/// \return Random double
double getRandomDouble(double min, double max);

/// \brief Randomly shuffles an integer vector
void shuffleIntVector(std::vector<int> & vector);
