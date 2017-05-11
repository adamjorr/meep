#include "seqem.h"
#include "plpdata.h"
#include <string>
#include <iostream>

int main(int argc, char *argv[]){
	Seqem seq("testdata/test.sam","testdata/test.fa");
	std::tuple<long double> result = seq.start(.00001);
	std::cout << "Theta is: " << std::get<0>(result) << std::endl;
	return 0;
}