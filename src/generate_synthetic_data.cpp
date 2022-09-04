#include <iterator>
#include <sstream>
#include <fstream>
#include <random>
#include <iomanip>
#include <algorithm>
#include "relation.h"

std::string to_string_trim_zeros(double a){
    // Print value to a string
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << a;
    std::string str = ss.str();
    // Ensure that there is a decimal point somewhere (there should be)
    if(str.find('.') != std::string::npos)
    {
        // Remove trailing zeroes
        str = str.substr(0, str.find_last_not_of('0')+1);
        // If the decimal point is now the last character, remove that as well
        if(str.find('.') == str.size()-1)
        {
            str = str.substr(0, str.size()-1);
        }
    }
    return str;
}

void generateDatasets(){
    for (int size : {1000000,10000000,100000000}){ 
        for (double parameter : {0.5,0.75,1.0,1.25,1.5,1.75,2.0,2.25,2.5,2.75,3.0}){
            // Generate data and shuffle
            Relation *r1 = new Relation(size, size);
            r1->Generate_Data(1, parameter, 0);
            auto rng = default_random_engine {};
            shuffle(begin((*r1->tuples)), end((*r1->tuples)), rng);

            // write data to file
            std::ofstream output_file("datasets/zipf_" + to_string_trim_zeros(parameter) + "_"  + std::to_string(size) +  ".txt");
            std::ostream_iterator<std::uint32_t> output_iterator(output_file, " ");
            std::copy(r1->tuples->begin(), r1->tuples->end(), output_iterator);
            output_file.close();
        }
    }
}

int main(){
    generateDatasets();
    return 0;
}