#include <iostream>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>

inline std::string reverseComplement(const std::string &input)
{
	std::string s = input;
	std::reverse(s.begin(), s.end());
	for (std::size_t i = 0; i < s.length(); ++i)
	{
		switch (s[i]){
			case 'A':
				s[i] = 'T';
				break;
			case 'C':
				s[i] = 'G';
				break;
			case 'G':
				s[i] = 'C';
				break;
			case 'T':
				s[i] = 'A';
				break;
		}
	}
	return s;
} 

inline void makeUpperCase(std::string &s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
}

inline void flipEdge (std::tuple<std::string, std::string, std::string, std::string> &e)
{
	std::string fromNode, fromStrand;
	std::string toNode, toStrand;
	fromNode = std::get<0>(e); fromStrand = std::get<1>(e);
	toNode = std::get<2>(e); toStrand = std::get<3>(e);

	std::get<0>(e) = toNode;
	std::get<2>(e) = fromNode;
	if (toStrand == "+")
		std::get<1>(e) = "-";
	else
		std::get<1>(e) = "+";

	if (fromStrand == "+")
		std::get<3>(e) = "-";
	else
		std::get<3>(e) = "+";
}

namespace aux{
	template<std::size_t...> struct seq{};

	template<std::size_t N, std::size_t... Is>
		struct gen_seq : gen_seq<N-1, N-1, Is...>{};

	template<std::size_t... Is>
		struct gen_seq<0, Is...> : seq<Is...>{};

	template<class Ch, class Tr, class Tuple, std::size_t... Is>
		void print_tuple(std::basic_ostream<Ch,Tr>& os, Tuple const& t, seq<Is...>){
			using swallow = int[];
			(void)swallow{0, (void(os << (Is == 0? "" : ", ") << std::get<Is>(t)), 0)...};
		}
} // aux::

template<class Ch, class Tr, class... Args>
auto operator<<(std::basic_ostream<Ch, Tr>& os, std::tuple<Args...> const& t)
	-> std::basic_ostream<Ch, Tr>&
{
	os << "(";
	aux::print_tuple(os, t, aux::gen_seq<sizeof...(Args)>());
	return os << ")";
}

int main(int argc, char** argv)
{
	// Check the number of parameters
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " input-gfa-file output-gfa-file" << std::endl;
		return 1;
	}

	std::unordered_map <std::string, std::string> nodeseq;
	std::unordered_map <std::string, int> nodeIntegerID;
	std::vector <std::tuple<std::string, std::string, std::string, std::string>> edges; //from, fromstrand, to, tostrand

	std::ifstream ifs (argv[1]);
	std::string line;
	while (getline(ifs,line))
	{
		if (line.size() == 0) continue;
		if (line[0] != 'S' && line[0] != 'L') continue;
		if (line[0] == 'S')
		{
			std::stringstream sstr (line);
			std::string tmp;
			std::string nodename;
			std::string seq;
			sstr >> tmp; assert(tmp == "S");
			sstr >> nodename;
			sstr >> seq;
			makeUpperCase (seq);
			nodeseq[nodename] = seq;
			int integerId = nodeIntegerID.size()+1; //1-based ids
			nodeIntegerID[nodename] = integerId;
		}
		if (line[0] == 'L')
		{
			std::stringstream sstr (line);
			std::string fromNode, fromStrand;
			std::string toNode, toStrand;
			std::string tmp;
			sstr >> tmp; assert(tmp == "L");
			sstr >> fromNode; sstr >> fromStrand;
			sstr >> toNode; sstr >> toStrand;
			assert (fromStrand == "+" || fromStrand == "-");
			assert (toStrand == "+" || toStrand == "-");
			assert (nodeIntegerID.find(fromNode) != nodeIntegerID.end());
			assert (nodeIntegerID.find(toNode) != nodeIntegerID.end());
			edges.emplace_back (fromNode, fromStrand, toNode, toStrand);
		}
	}

	std::cerr << "count of nodes = " << nodeseq.size() << "\n";
	std::cerr << "count of edges = " << edges.size() << "\n";

	std::unordered_map <std::string, std::string> registerNodeStrand;
	for (auto &e : edges)
	{
		std::string fromNode, fromStrand;
		std::string toNode, toStrand;
		fromNode = std::get<0>(e); fromStrand = std::get<1>(e);
		toNode = std::get<2>(e); toStrand = std::get<3>(e);

		//Both FROM and TO nodes are not seen before
		if (registerNodeStrand.find (fromNode) == registerNodeStrand.end() && registerNodeStrand.find (toNode) == registerNodeStrand.end())
		{
			registerNodeStrand[fromNode] = fromStrand;
			registerNodeStrand[toNode] = toStrand;
		}
		//FROM node seen before but TO node not seen before
		else if (registerNodeStrand.find (fromNode) != registerNodeStrand.end() && registerNodeStrand.find (toNode) == registerNodeStrand.end())
		{
			//if FROM node strand mismatches, then flip
			if (fromStrand != registerNodeStrand[fromNode])
			{
				flipEdge(e);
				registerNodeStrand[toNode] = toStrand == "+" ? "-" : "+";
			}
			else
				registerNodeStrand[toNode] = toStrand;
		}
		//FROM node not seen before but TO node seen before
		else if (registerNodeStrand.find (fromNode) == registerNodeStrand.end() && registerNodeStrand.find (toNode) != registerNodeStrand.end())
		{
			//if TO node strand mismatches, then flip
			if (toStrand != registerNodeStrand[toNode])
			{
				flipEdge(e);
				registerNodeStrand[fromNode] = fromStrand == "+" ? "-" : "+";
			}
			else
				registerNodeStrand[fromNode] = fromStrand;
		}
		//Both FROM and TO nodes have been seen before
		else if (registerNodeStrand.find (fromNode) != registerNodeStrand.end() && registerNodeStrand.find (toNode) != registerNodeStrand.end())
		{
			//if both do not agree, just flip
			if (fromStrand != registerNodeStrand[fromNode] && toStrand != registerNodeStrand[toNode])
				flipEdge(e);
			//if both agree, then
			else if (fromStrand == registerNodeStrand[fromNode] && toStrand == registerNodeStrand[toNode])
			{ /* do nothing */}
			//else raise failure
			else {
				std::cerr << "FAILURE at edge " << e << "\n";
				std::exit(1);
			}
		}
	}

	std::ofstream ofs (argv[2]);
	for (auto &s: nodeseq)
	{
		if (registerNodeStrand.find(s.first) == registerNodeStrand.end())
			ofs << "S\t" << nodeIntegerID[s.first] << "\t" << s.second << "\n";
		else if (registerNodeStrand[s.first] == "+")
			ofs << "S\t" << nodeIntegerID[s.first] << "\t" << s.second << "\n";
		else 
			ofs << "S\t" << nodeIntegerID[s.first] << "\t" << reverseComplement(s.second) << "\n";
	}

	for (auto &e: edges)
	{
		std::string fromNode, fromStrand;
		std::string toNode, toStrand;
		fromNode = std::get<0>(e); fromStrand = std::get<1>(e);
		toNode = std::get<2>(e); toStrand = std::get<3>(e);
		assert (registerNodeStrand[fromNode] == fromStrand);
		assert (registerNodeStrand[toNode] == toStrand);
		ofs << "L\t" << nodeIntegerID[fromNode] << "\t" << "+\t" << nodeIntegerID[toNode] << "\t+\t*\n";
	}
	return 0;
}
