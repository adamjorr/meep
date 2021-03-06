#include "seqem.h"
#include <cmath>
#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <limits>

const std::map<char,double> Seqem::uniform_pi = {{'A',.25},{'T',.25},{'C',.25},{'G',.25}};

Seqem::Seqem(Pileupdata p, int ploidy) : plp(p), theta(std::make_tuple(0.01)),
	em(std::bind(&Seqem::q_function, this, std::placeholders::_1), std::bind(&Seqem::m_function,this,std::placeholders::_1), theta),
	ploidy(ploidy){
	possible_gts = Genotype::enumerate_gts(ploidy);
}

Seqem::Seqem(Pileupdata p): Seqem(p, 2){
}

Seqem::Seqem(std::string samfile, std::string refname, int ploidy) : plp(samfile, refname), theta(std::make_tuple(0.1)),
	em(std::bind(&Seqem::q_function, this, std::placeholders::_1), std::bind(&Seqem::m_function,this,std::placeholders::_1), theta),
	ploidy(ploidy){
	possible_gts = Genotype::enumerate_gts(ploidy);
}

Seqem::Seqem(std::string samfile, std::string refname) : Seqem(samfile, refname, 2) {
}

Seqem::theta_t Seqem::start(double stop){
	return em.start(stop);
}

double Seqem::q_function(theta_t theta){
	double likelihood = 0.0;
	pileupdata_t plpdata = plp.get_data();

	for (pileupdata_t::iterator tid = plpdata.begin(); tid != plpdata.end(); ++tid){
		for(std::vector<pileuptuple_t>::iterator pos = tid->begin(); pos != tid->end(); ++pos){
			const std::vector<char> &x = std::get<0>(*pos);
			for (std::vector<Genotype>::iterator g = possible_gts.begin(); g != possible_gts.end(); ++g){
				likelihood += pg_x_given_theta(*g,x,theta,uniform_pi);
			}
		}
	}
	return likelihood;
}

Seqem::theta_t Seqem::m_function(theta_t theta){
	pileupdata_t plpdata = plp.get_data();
	std::vector<double> s(3,0.0); //TODO:make this generic, depends on ploidy

	for (pileupdata_t::iterator tid = plpdata.begin(); tid != plpdata.end(); ++tid){
		for(std::vector<pileuptuple_t>::iterator pos = tid->begin(); pos != tid->end(); ++pos){
			increment_s(s, std::get<0>(*pos), possible_gts, theta, uniform_pi);
		}
	}
	return std::make_tuple(calc_epsilon(s));
}

void Seqem::increment_s(std::vector<double> &s, std::vector<char> x, const std::vector<Genotype> gts, theta_t theta, std::map<char,double> pi){
	for (auto g : gts){
		std::vector<double> site_s = calc_s(x,g);
		double pg_x = pg_x_given_theta(g,x,theta,pi);
		for(size_t i = 0; i < s.size(); ++i){
			s[i] += pg_x * site_s[i];	
		}
	}
}

std::vector<double> Seqem::calc_s(std::vector<char> x, Genotype g){ //TODO: make this generic
	std::vector<double> s(3,0.0);
	for (std::vector<char>::iterator i = x.begin(); i != x.end(); ++i){
		int numgt = g.numbase(*i);
		if (numgt == 2){
			s[0]++;
		}
		else if (numgt == 1){
			s[1]++;
		}
		else if (numgt == 0){
			s[2]++;
		}
	}
	return s;
}

//RESULT NOT IN LOG SPACE
double Seqem::pg_x_given_theta(const Genotype g,const std::vector<char> x,const theta_t theta, std::map<char,double> pi){
	double px = px_given_gtheta(x,g,theta);
	return exp(px + pg(g,pi));
}

//may be faster if we represent x as a map w/ char and counts, like gt?? we support this in plpdata
double Seqem::px_given_gtheta(const std::vector<char> x,const Genotype g,const theta_t theta){
	double px = 0.0;
	std::map<char, int> counts;

	for (std::vector<char>::const_iterator i = x.begin(); i != x.end(); ++i){
		counts[*i] += 1;
	}
	for (std::map<char,int>::iterator i = counts.begin(); i!= counts.end(); ++i){
		double pn = pn_given_gtheta(i->first,g,theta);
		if (pn == -std::numeric_limits<double>::infinity()){
			return -std::numeric_limits<double>::infinity();
		}
		else{
			px += i->second * pn;
			// px -= std::lgamma(i->second + 1);
		}
	}
	// px += std::lgamma(x.size()+1);
	return px;
}

//LOG SPACE
double Seqem::pn_given_gtheta(char n, Genotype g, theta_t theta){
	double epsilon = std::get<0>(theta);
	double p;
	// p = ((double)g.numbase(n))/g.getploidy()*(1.0-3.0*epsilon) + ((double)g.numnotbase(n))/g.getploidy()*epsilon;
	int numgt = g.numbase(n);
	if (numgt == 2){
		p = (1.0 - 3.0 * epsilon);
	}
	else if (numgt == 1){
		p = (0.5 - epsilon);
	}
	else{
		p = epsilon;
	}
	if (p == 0){
		return -std::numeric_limits<double>::infinity();
	}
	else if (p < 0){
		std::clog << "N=" << n << "\tGT=" << g << "\tPloidy=" << g.getploidy() << "\t#N=" << g.numbase(n) << "\t#!N=" << g.numnotbase(n) << "\tP1=" << ((double)g.numbase(n))/g.getploidy()*(1-3*epsilon) << "\tP2=" << ((double)g.numnotbase(n))/g.getploidy()*epsilon << "\tP=" << p << "\tTheta=" << theta << std::endl;
		throw std::runtime_error("p < 0 detected");
	}
	else{
		return log(p);
	}
}

double Seqem::pg(Genotype g, std::map<char,double> pi){
	double p = 0.0;
	for (auto it=g.gt.begin(); it != g.gt.end(); ++it){
		p += it->second * log(pi[it->first]);
	}
	return p;
}

double Seqem::calc_epsilon(std::vector<double> s){
	double a = 3.0 * (s[0] + s[1] + s[2]);
	double b = - (3.0/2 * s[0] + s[1] + 5.0/2 * s[2]);
	double c = s[2] / 2;
	double epsilon_minus = (-b - sqrt(std::pow(b,2) - 4 * a * c))/(2 * a);
	// double epsilon_plus = (-b + sqrt(std::pow(b,2) - 4 * a * c))/(2 * a);
	double epsilon;
	if (epsilon_minus < 0){
		epsilon = 0;
	}
	else{
		epsilon = epsilon_minus;
	}
	return epsilon;
}

double Seqem::smallest_nonzero(std::vector<double> v){
	std::vector<double> sorted_v(v);
	std::sort(sorted_v.begin(),sorted_v.end());
	double smallest = 0;
	for (std::vector<double>::iterator i = sorted_v.begin(); i != sorted_v.end(); ++i){
		if(*i > smallest){
			return *i;
		}
	}
	return 0;
}

