#include "pileup.h"
#include <htslib/sam.h>
#include "samio.h"
#include <iostream>

Pileup::Pileup(std::string samfile, std::string reffile): reader(samfile), ref(reffile), tid(), pos(), cov(), pileup(nullptr), iter(), alleles(), qual(), names(), readgroups(), counts({{'A',0},{'T',0},{'G',0},{'C',0}})  {
	iter = bam_plp_init(&Pileup::plp_get_read, &reader);
}

Pileup::Pileup(std::string samfile, std::string reffile, std::string region): reader(samfile, region), ref(reffile), tid(), pos(), cov(), pileup(nullptr), iter(), alleles(), qual(), names(), readgroups(), counts({{'A',0},{'T',0},{'G',0},{'C',0}}) {
	iter = bam_plp_init(&Pileup::plp_get_read, &reader);
}

Pileup::~Pileup(){
	std::cout << "calling pileup destructor" << std::endl;
	bam_plp_destroy(iter);
	std::cout << "bam_plp_destroy success" << std::endl;
}

//typedef int (*bam_plp_auto_f)(void *data, bam1_t *b);
int Pileup::plp_get_read(void *data, bam1_t *b){
	SamReader *reader = (SamReader*)data;
	return reader->next(b);
}

//possible optimization: store sequence strings in a hash w/ alignment, throw out of hash once no longer in pileup
int Pileup::next(){
	if((pileup = bam_plp_auto(iter, &tid, &pos, &cov)) != nullptr){ //successfully pile up new position
		alleles.clear(); qual.clear(); names.clear(); readgroups.clear(); counts.clear();
		std::cout << "cleared arrays OK" << std::endl;
		alleles.reserve(cov); qual.reserve(cov); names.reserve(cov); readgroups.reserve(cov);
		std::cout << "reserved memory OK" << std::endl;
		for (int i = 0; i < cov; ++i){
			bam1_t* alignment = pileup[i].b;
			uint8_t* seq = bam_get_seq(alignment);
			int qpos = pileup[i].qpos;
			int baseint = bam_seqi(seq,qpos);
			char allele = seq_nt16_str[baseint];
			std::string name(bam_get_qname(alignment));
			std::cout<< "got variables OK" << std::endl;
			alleles[i] = allele;
			++counts[allele];
			std::cout << "counted OK" << std::endl;
			qual[i] = bam_get_qual(alignment)[qpos];
			std::cout << "set qualities OK" << std::endl;
			names[i] = name;
			std::cout << "set names OK" << std::endl;


			// if (rg == NULL){
			// 	readgroups[i] = "";
			// }
			// else{
			// 	char* readgroup = bam_aux2Z(rg);
			// 	if (readgroup != NULL){
			// 		readgroups[i] = std::string(readgroup);
			// 	}
			// }

			readgroups[i] = std::string(bam_aux2Z(bam_aux_get(alignment, "RG")))2;

			std::cout<< "set readgroups OK" << std::endl;
			// bam_destroy1(alignment);
		}
		return 1;
	} else {
		return 0;
	}
}

int Pileup::get_tid(){
	return tid;
}

int Pileup::get_pos(){
	return pos;
}

std::string Pileup::chr_name(){
	return reader.get_ref_name(tid);
}

int Pileup::get_ref_tid(std::string name){
	return reader.get_ref_tid(name);
}

std::map<std::string,int> Pileup::get_name_map(){
	return reader.get_name_map();
}

