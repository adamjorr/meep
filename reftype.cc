#include "reftype.h"
#include <stdexcept>
#include <stdlib.h>

Reftype::Reftype(std::string reference_name) : ref(){
	faidx_t* faidx = fai_load(reference_name.c_str());
	if (faidx == nullptr){
		throw std::runtime_error("error loading reference");
	}
	faidx_p = faidx;
}

Reftype::Reftype(faidx_t* faidx_p) : faidx_p(faidx_p), ref() {
}

Reftype::Reftype() : faidx_p(nullptr), ref(){
}

Reftype::~Reftype(){
	if (faidx_p != nullptr){
		fai_destroy(faidx_p);
	}
}

std::string Reftype::get_ref(std::string region){
	if (region != this->region){
		this->region = region;
		ref_p = fai_fetch(faidx_p,region.c_str(),&ref_len);
		if (ref_p == nullptr){
			throw std::runtime_error("error getting ref");
		}
		ref.assign(ref_p,ref_len);
	}
	return ref;
}

int Reftype::get_ref_len(){
	return ref_len;
}



