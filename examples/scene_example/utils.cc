/*
 *  Copyright (C) 2018 João Borrego
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *      
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*!
    \file examples/scene_example/utils.cc
    \brief Generic utilities for scene generation example implementation

    \author João Borrego : jsbruglie
    \author Rui Figueiredo : ruipimentelfigueiredo
*/

#include "utils.hh"

const std::string getUsage(const char* argv_0)
{
    return \
        "usage:   " + std::string(argv_0) + " [options]\n" +
        "options: -s <number of scenes to generate>\n"  +
        "         -n <index of the first scene>\n" +
        "         -i <image output directory>\n" +
        "         -d <dataset output directory>\n" +
        "         -D Debug mode\n";
}

//////////////////////////////////////////////////
void parseArgs(
    int argc,
    char** argv,
    unsigned int & scenes,
    unsigned int & start,
    std::string & imgs_dir,
    std::string & dataset_dir,
    bool & debug)
{

    int opt;
    bool s, n, i, m, d, D;

    while ((opt = getopt(argc,argv,"s: n: i: d: D")) != EOF)
    {
        switch (opt)
        {
            case 's':
                s = true; scenes = atoi(optarg); break;
            case 'n':
                n = true; start = atoi(optarg); break;
            case 'i':
                i = true; imgs_dir = optarg;    break;
            case 'd':
                d = true; dataset_dir = optarg; break;
            case 'D':
                D = true; debug = true;         break;
            case '?':
                std::cerr << getUsage(argv[0]);
            default:
                std::cout << std::endl;
                exit(EXIT_FAILURE);
        }
    }

    // If arg was not set then assign default values
    if (!s) scenes  = ARG_SCENES_DEFAULT;
    if (!n) start   = ARG_START_DEFAULT;
    if (!i) imgs_dir    = ARG_IMGS_DIR_DEFAULT;
    if (!d) dataset_dir = ARG_DATASET_DIR_DEFAULT;
    if (!D) debug       = ARG_DEBUG_DEFAULT;

    debugPrint("Parameters: " << std::endl <<
        "scenes: '"         << scenes <<
        "'; images dir: '"  << imgs_dir <<
        "'; dataset dir: '" << dataset_dir <<
        "'; debug: "        << debug << std::endl);
}

//////////////////////////////////////////////////
bool createDirectory(std::string & path)
{
    boost::filesystem::path dir(path);
    if (boost::filesystem::create_directory(dir)) {
        debugPrintTrace("Created directory " << path);
        return true;
    }
    return false;
}

//////////////////////////////////////////////////

// Random integer generator
std::random_device rng;
std::mt19937 mt_rng(rng());
std::uniform_int_distribution<int> uniform_dist;

//////////////////////////////////////////////////
int getRandomInt(int min, int max)
{
    int aux = uniform_dist(mt_rng);
    return aux % (max - min + 1) + min;;
}

//////////////////////////////////////////////////
double getRandomDouble(double min, double max)
{
    double aux = ((double) uniform_dist(mt_rng)) / (double) RAND_MAX;
    return aux * (max - min) + min;
}

//////////////////////////////////////////////////
void shuffleIntVector(std::vector<int> & vector)
{
    std::shuffle(vector.begin(), vector.end(), mt_rng);
}
