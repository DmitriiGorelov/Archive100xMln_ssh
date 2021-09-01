// CompressMln.cpp : Defines the entry point for the console application.
//
//LICENSE:
// This is demonstration program of Huffman compressing algorithm with involving of GPU. It builds archive file
// out of (a) source file and (b) huffman-tree graph.
// USING THIS CODE IS LIMITED and ILLEGAL until other granted in written permission from Gorelov Dmitry, dmitry.gorelov@gmail.com 

#include <functional>
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <conio.h>
#include <algorithm>
#include <bitset>

#include "cudacaller.h"

#define gMAX_LEN 1000000
#define gMAX_VALUE 100
#define gMIN_VALUE 1

using namespace std;
namespace fs = std::filesystem;

typedef uint64_t pattern;

bool onCPU(false);

static string sPathSource;
static string sPathArchive;
static string sPathUnpacked;

static vector<unsigned char> source;
static int bitCount = 0;

void prepareTree();

class freqElem {

public:
	freqElem()
		: elem(-1)
		, counter(0)
		, left(nullptr)
		, right(nullptr)
	{

	}

	int elem;
	int counter;
	shared_ptr<freqElem> left;
	shared_ptr<freqElem> right;
	vector<unsigned char> code;

	shared_ptr<freqElem> getLetter(bool value)
	{
		if (value)
			return right;
		else
			return left;
	}

	bool getCode(unsigned char value, vector<unsigned char>& code)
	{
		if (elem == value)
		{
			return true;
		}
		if (left)
		{
			if (left->getCode(value, code))
			{
				code.insert(code.begin(), false);
				return true;
			}
		}
		if (right)
		{
			if (right->getCode(value, code))
			{
				code.insert(code.begin(), true);
				return true;
			}
		}
		if (elem > 0)
			return false;

		return false;
	}

	void operator++(int) { counter++; return; }
};
static vector<freqElem> frequencies;
static vector<freqElem> tree;
static vector<vector<unsigned char>> rawcodes(256);

void saveMln(const std::string& sPath)
{
	std::ofstream outputFile(sPath, ios::binary);

	if (!outputFile.is_open())
	{
		cout << "could not open file " << sPath;
	}

	outputFile.write((char*)(&source[0]), source.size());
	outputFile.close();
};

void unpack(const std::string& sPathArchive, const std::string& sPathUnpacked)
{
	std::ifstream inputFile(sPathArchive, ios::binary);

	if (!inputFile.is_open())
	{
		cout << "could not open file " << sPathArchive;
		exit(1);
	}

	typedef unsigned int LONGWORD;
	LONGWORD size = 0;
	char byte4[sizeof(LONGWORD)];
	LONGWORD* byte4p = (LONGWORD*)&byte4;

	//read len of frequencies
	inputFile.read((char*)&byte4, sizeof(byte4));
	size = *byte4p;

	//
	for (size_t i = 0; i < size; i++)
	{
		inputFile.read((char*)&byte4, 1); //1 byte
		freqElem it;
		it.elem = unsigned char(*byte4p);

		inputFile.read((char*)&byte4, 4); //1 byte		
		it.counter = *byte4p;

		frequencies.push_back(it);
	}

	prepareTree();

	if (frequencies.size() == 1)
	{
		source = vector<unsigned char>(frequencies[0].counter, (unsigned char)frequencies[0].elem);
	}
	else
	{
		//read len of unpacked file
		inputFile.read((char*)&byte4, sizeof(byte4));
		int fileSize = *byte4p;
		source.resize(fileSize);		
		//read len of bit-array
		inputFile.read((char*)&byte4, sizeof(byte4));
		bitCount = *byte4p;
		vector<unsigned char> bits(bitCount / 8 + ((bitCount % 8) > 0 ? 1 : 0), 0);
		int cnt = 0;
		auto n = tree[0];
		int fileCounter(0);
		inputFile.read((char*)&bits[0], bits.size());
		int bitsCounter(0);
		while (bitCount > 0)
		{
			while (n.elem < 0)
			{
				n = *n.getLetter(bits[bitsCounter] & (1 << cnt)).get();
				bitCount--;
				cnt++;
				if (cnt >= 8)
				{
					//inputFile.read((char*)&bits, 1);
					bitsCounter++;
					cnt = 0;
				}
			}
			memcpy(&source[fileCounter], &n.elem, 1);
			//source.push_back((unsigned char)n.elem);
			fileCounter++;
			n = tree[0];
		}
	}
	//...
	inputFile.close();

	saveMln(sPathUnpacked);
}



void prepareFreq()
{
	for (int i = 0; i < 256; i++)
	{
		freqElem it;
		it.elem = i;
		it.counter = 0;
		frequencies.push_back(it);
	}
}

void prepareTree()
{
	std::sort(frequencies.begin(), frequencies.end(), [](freqElem left, freqElem right) {if (left.counter == right.counter) return left.elem < right.elem; else return left.counter > right.counter; });

	frequencies.erase(std::remove_if(frequencies.begin(), frequencies.end(), [](freqElem elem) {return elem.counter < 1; }), frequencies.end());

	if (frequencies.size() == 1)
	{
		freqElem top;
		top.counter = frequencies[0].counter;
		top.elem = -1;
		top.left = nullptr;// make_shared<freqElem>(frequencies[0]);
		top.right = nullptr;
		tree.push_back(top);
	}
	else
		tree = frequencies;

	while (tree.size() > 1)
	{
		std::sort(tree.begin(), tree.end(), [](freqElem left, freqElem right) {return left.counter > right.counter; });

		auto it = --tree.end();
		auto right = *it;
		it = --tree.erase(it);
		auto left = *it;
		tree.erase(it);

		freqElem top;
		top.counter = right.counter + left.counter;
		top.elem = -1;
		top.left = make_shared<freqElem>(left);
		top.right = make_shared<freqElem>(right);
		tree.push_back(top);
	}

	for (auto& it : frequencies)
	{
		tree[0].getCode(it.elem, rawcodes[it.elem]);
	}

	std::sort(frequencies.begin(), frequencies.end(), [](freqElem left, freqElem right) {return left.elem < right.elem; });
}

void pack1()
{
	prepareFreq();

	for (auto it : source)
	{
		frequencies[it]++;
	}

	prepareTree();

	for (int it = 0; it < source.size(); it++)
	{
		bitCount += rawcodes[source[it]].size();
	}
}

void saveArchive(const std::string& sPathArchive)
{
	// save archive to file
	std::ofstream outputFile(sPathArchive, ios::binary);

	if (!outputFile.is_open())
	{
		cout << "could not open file " << sPathArchive;
	}

	typedef unsigned int LONGWORD;
	LONGWORD size = frequencies.size();
	char byte4[sizeof(LONGWORD)];
	LONGWORD* byte4p = (LONGWORD*)&byte4;
	*byte4p = size;

	//write len of frequencies
	outputFile.write((char*)&byte4, sizeof(byte4));
	//write elements with frequencies
	for (auto it : frequencies)
	{
		*byte4p = it.elem;
		outputFile.write((char*)&byte4, 1); //1 byte
		*byte4p = it.counter;
		outputFile.write((char*)&byte4, 4); //4 bytes
	}

	//write len of source file
	*byte4p = source.size();	
	outputFile.write((char*)&byte4, sizeof(byte4));

	//write len of bit-array
	*byte4p = bitCount;	
	outputFile.write((char*)&byte4, sizeof(byte4));
		
	if (bitCount > 0)
	{
		vector<unsigned char> hostBytes(bitCount / 8 + ((bitCount % 8) > 0 ? 1 : 0), 0);

		// CUDA
		vector<unsigned char> hostBits(bitCount);
		bool cudaSucceed(!onCPU && cudaCaller::canCuda());
		if (cudaSucceed)
		{			
			printf("RUN ON GPU!!!!");

			int cnt(0);
			for (auto it : source)
			{				
				memcpy(&hostBits[cnt], &rawcodes[it][0], rawcodes[it].size());
				cnt += rawcodes[it].size();
				//hostBits.insert(hostBits.end(), rawcodes[it].begin(), rawcodes[it].end());
			}			
			cudaSucceed = cudaCaller::doCuda(hostBits, hostBytes, bitCount);
		}

		if (!cudaSucceed)
		{
			//vector<unsigned char> hostBytesCPU(bitCount / 8 + ((bitCount % 8) > 0 ? 1 : 0), 0);
			// CPU
			printf("RUN ON CPU!");
			int cnt = 0;
			int idx(0);
			auto it = source.begin();
			while (it != source.end())
			{
				auto bit = rawcodes[(*it)].begin();
				while (bit != rawcodes[(*it)].end())
				{
					hostBytes[idx] |= (*bit << cnt);
					cnt++;
					if (cnt == 8)
					{
						cnt = 0;
						idx++;

						bit++;
						continue;
					}

					bit++;
				}
				it++;
			}			

			//TEST:
			// COMPARE vectors of archive data in GPU and CPU
			/*for (int i = 0; i < hostBytes.size(); i++)
			{
				if (hostBytes[i] != hostBytesCPU[i])
				{
					printf("data not the same!");
					break;
				}
			}*/
		}		

		outputFile.write((char*)&hostBytes[0], hostBytes.size());
		
	}

	//..
	outputFile.close();

	
}

int main(int argc, const char* args[])
{
	size_t found;
	string sTmp(args[0]);
	found = sTmp.find_last_of("/\\");
	//cout << " folder: " << sTmp.substr(0, found) << endl;
	//cout << " file: " << sTmp.substr(found + 1) << endl;

	fs::path path(sTmp.substr(0, found));

	string sPathParent = path.string() + "\\";
	cout << sPathParent << endl;

	sPathSource = sPathParent + "source.txt";
	sPathArchive = sPathParent + "archive.huf";
	sPathUnpacked = sPathParent + "unpacked.txt";

	if (argc > 4)
	{
		onCPU = string(args[4]) == "cpu";
	}
	if (argc > 3 && string(args[1]) == "u")
	{
		sPathArchive = sPathParent + string(args[2]);
		sPathUnpacked = sPathParent + string(args[3]);

		unpack(sPathArchive, sPathUnpacked);
		return 0;
	}
	else
	if (argc > 3 && string(args[1]) == "p")
	{
		sPathSource= sPathParent + string(args[2]);
		sPathArchive = sPathParent + string(args[3] + (fs::path(args[3]).extension()==".huf" ? string("") : string(".huf")));

		string y("n");
		if (fs::exists(sPathSource))
		{
			ifstream inFile(sPathSource, std::ios::binary);
			if (!inFile.is_open())
			{
				cout << " File open failed " << sPathSource << endl;
				exit(1);
			}

			// Stop eating new lines in binary mode!!!
			inFile.unsetf(std::ios::skipws);

			// get its size:
			std::streampos fileSize;

			inFile.seekg(0, std::ios::end);
			fileSize = inFile.tellg();
			inFile.seekg(0, std::ios::beg);

			// reserve capacity			
			source.reserve(fileSize);
			source.insert(source.begin(),
				std::istream_iterator<unsigned char>(inFile),
				std::istream_iterator<unsigned char>());

			inFile.close();
		}
		else
		{
			auto generateMln = [&]()
			{
				srand(time(0));
				source.resize(gMAX_LEN);
				int rangeProvider = gMAX_VALUE - gMIN_VALUE + 1;
				std::generate(source.begin(), source.end(), [=]() {return rand() % (rangeProvider)+gMIN_VALUE; });
			};

			cout << "Do you want to generate source.txt and compress it? y/n ...";
			cin >> y;

			y = tolower(y[0]);
			if (y == "y")
			{
				generateMln();
				saveMln(sPathSource);
			}
			else
				exit(1);
		}

		pack1();

		saveArchive(sPathArchive);

		fs::remove(sPathSource);		

		return 0;
	}

	return 0;

}

