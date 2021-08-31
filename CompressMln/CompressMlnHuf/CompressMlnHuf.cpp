// CompressMln.cpp : Defines the entry point for the console application.
//
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

#define gMAX_LEN 1000000
#define gMAX_VALUE 100
#define gMIN_VALUE 1

using namespace std;
namespace fs = std::filesystem;

typedef uint64_t pattern;

static string sPathSource;
static string sPathArchive;
static string sPathUnpacked;

static vector<unsigned char> source;
static int bitCount = 0;
static vector<vector<bool>> chrCodes;

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
	vector<bool> code;

	shared_ptr<freqElem> getLetter(bool value)
	{
		if (value)
			return right;
		else
			return left;
	}

	bool getCode(unsigned char value, vector<bool>& code)
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
		//read len of bit-array
		inputFile.read((char*)&byte4, sizeof(byte4));
		bitCount = *byte4p;
		unsigned char bits = 0;
		int cnt = 0;
		auto n = tree[0];
		inputFile.read((char*)&bits, 1);
		while (bitCount > 0)
		{
			while (n.elem < 0)
			{
				n = *n.getLetter(bits & (1 << cnt)).get();
				bitCount--;
				cnt++;
				if (cnt >= 8)
				{
					inputFile.read((char*)&bits, 1);
					cnt = 0;
				}
			}
			source.push_back((unsigned char)n.elem);
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
		tree[0].getCode(it.elem, it.code);
	}

	std::sort(frequencies.begin(), frequencies.end(), [](freqElem left, freqElem right) {return left.elem < right.elem; });
}

void pack1()
{
	prepareFreq();

	//vector<unsigned char> chrSource(source.begin(), source.end());	
	chrCodes.resize(source.size());

	for (auto it : source)
	{
		frequencies[it]++;
	}

	vector<vector<bool>> rawcodes(frequencies.size());

	prepareTree();

	for (auto& it : frequencies)
	{
		rawcodes[it.elem] = it.code;
	}

	// can be parallel!	
	for (int it=0; it<source.size(); it++)
	{		
		chrCodes[it]= rawcodes[source[it]];
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

	*byte4p = bitCount;
	//write len of bits
	outputFile.write((char*)&byte4, sizeof(byte4));

	int cnt = 0;
	//unsigned char bits=0;
	vector<unsigned char> bytes(bitCount / 8 + ((bitCount % 8) > 0 ? 1 : 0), 0);
	int idx(0);
	auto it = chrCodes.begin();
	while (it != chrCodes.end())
	{
		auto bit = (*it).begin();
		while (bit != (*it).end())
		{
			bytes[idx] |= (*bit << cnt);
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
	outputFile.write((char*)&bytes[0], bytes.size());

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

	if (argc > 1 && string(args[1]) == "u")
	{
		unpack(sPathArchive, sPathUnpacked);
		return 0;
	}

	auto genereateMln = [&]()
	{
		srand(time(0));
		source.resize(gMAX_LEN);
		int rangeProvider = gMAX_VALUE - gMIN_VALUE + 1;
		std::generate(source.begin(), source.end(), [=]() {return rand() % (rangeProvider)+gMIN_VALUE; });
	};

	string y("n");
	if (fs::exists(sPathSource))
	{
		cout << "Do you want to open an existing source.txt? y/n ...";
		cin >> y;

		y = tolower(y[0]);
		if (y == "y")
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
			genereateMln();
			cout << "Do you want to overwrite source.txt with new generated array? y/n ...";
			cin >> y;

			y = tolower(y[0]);
			if (y == "y")
			{
				saveMln(sPathSource);
			}
		}
	}
	else
	{
		genereateMln();
		saveMln(sPathSource);
	}	

	pack1();

	saveArchive(sPathArchive);

	return 0;

}

