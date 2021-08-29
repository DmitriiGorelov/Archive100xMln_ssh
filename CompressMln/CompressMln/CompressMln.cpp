// CompressMln.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"

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

#define gMAX_LEN 1000000
#define gMAX_VALUE 100
#define gMIN_VALUE 1
// WinLen must not exceed 7 bytes! 8th byte is used to save the pattern length
#define gWIN_LEN 5
#define gWIN_LEN_SMALLEST 3

using namespace std;
namespace fs = std::filesystem;

typedef uint64_t pattern;

static string sPathSource;
static string sPathArchive;
static string sPathUnpacked;

static vector<int> source;
static unordered_map<unsigned char, map<int, int>> chrStage1; // key1 is element, key2 is pos, value is len of sequence at this pos
static vector<unsigned char> chrStage1Rest;
static map<shared_ptr<pattern>, vector<int>> dictPatterns;

void saveMln(const std::string& sPath)
{
	std::ofstream outputFile(sPath);

	if (!outputFile.is_open())
	{
		cout << "could not open file " << sPath;
	}

	for (auto it : source)
	{
		outputFile << it << "\n";
	}
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

	//read len of the not packed vector
	inputFile.read((char*)&byte4, sizeof(byte4));
	size=*byte4p;

	//write elements of not packed vector
	for (size_t i=0; i< size; i++)
	{
		inputFile.read((char*)&byte4, 1); //1 byte
		chrStage1Rest.push_back(*byte4p);
	}

	// read len of dictionary
	inputFile.read((char*)&byte4, sizeof(byte4));
	size = *byte4p;
	pattern key;
	for (size_t dic = 0; dic < size; dic++)
	{
		// size of pattern		
		inputFile.read((char*)&byte4, sizeof(byte4));
		LONGWORD pattSize(*byte4);
		char* bytenp = new char[pattSize+1];
		bytenp[pattSize] = 0;
		// pattern
		inputFile.read(bytenp, pattSize);
		key = 0;
		memcpy(&key, bytenp, pattSize);
		shared_ptr<pattern> pt=make_shared<pattern>(key + (pattern(pattSize) << 56));
		delete[] bytenp;
		// len of positions vector
		inputFile.read((char*)&byte4, sizeof(byte4));
		LONGWORD pattVector = *byte4p;

		for (size_t pos = 0; pos < pattVector; pos++)
		{
			inputFile.read((char*)&byte4, sizeof(byte4));
			LONGWORD posValue;
			memcpy(&posValue,byte4p,4);
			dictPatterns[pt].push_back(posValue);
		}
	}
	inputFile.close();

	map<int, shared_ptr<pattern>> dictTmp3; // key is pos
	for (auto& itPattern : dictPatterns)
	{
		//shared_ptr<pattern> pn = make_shared<pattern>(itPattern.first);
		for (auto& itPos : itPattern.second)
		{
			dictTmp3[itPos] = itPattern.first;
		}
	}

	for (auto pt : dictTmp3)
	{
		//for (auto pos : pt.second)
		{
			vector<unsigned char> v(*pt.second.get() >> 56); //{ pt.second->begin(), pt.second->end() };			
			memcpy(&v[0], pt.second.get(), v.size());
			chrStage1Rest.insert(chrStage1Rest.begin() + pt.first, v.begin(),v.end());
		}
	}

	auto it = chrStage1Rest.begin();
	while (it!= chrStage1Rest.end())
	{
		if ((*it) & 128)
		{
			int value = (*it) & 127;			
			it++;
			int count = (*it);			
			for (int i = 0; i < count; i++)
			{
				source.push_back(value);				
			}
			it++;
		}
		else
		{
			source.push_back((*it));
			it++;
		}
	}

	saveMln(sPathUnpacked);
}

void pack1()
{
	// 
	// Stage 1: search repeating sequences with lenght 3 and more
	// 
	// compress by elements size (max=100 need 7 bits)
	vector<unsigned char> chrSource(source.begin(), source.end());
#if gWIN_LEN
	int leftOfSeq(fmax(chrSource.size() - gWIN_LEN, 3));
#else
	int leftOfSeq(chrSource.size() / 2 + 1);
#endif
	int sizeOfSeq(chrSource.size() - leftOfSeq);

	//string lastKey = "";	
	
	size_t idxStage1Rest=0;
	int idxSequence = 0;
	int i = 0;
	while (i < chrSource.size() - 2)
	{
		if (chrSource[i] == chrSource[i + 1] && chrSource[i] == chrSource[i + 2])
		{
			idxSequence = i;
			chrStage1[chrSource[i]][idxSequence] += 3;
			chrStage1Rest.push_back(chrSource[i] | 128);
			chrStage1Rest.push_back(3);
			idxStage1Rest = chrStage1Rest.size() - 1;

			i += 2;
			for (int c = i; (c < chrSource.size() - 2); c++)
			{
				i++;
				if (chrSource[c] != chrSource[c + 1])
				{					
					break;
				}
				else
				{
					chrStage1[chrSource[i]][idxSequence]++;
					chrStage1Rest[idxStage1Rest]++;
				}
				if (chrStage1Rest[idxStage1Rest] >= 254)
				{
					i++;
					break;
				}
			}
		}
		else
		{
			chrStage1Rest.push_back(chrSource[i]);
			i++;
		}

		// i points to 1st element after sequence (if any)
	}
	chrStage1Rest.push_back(chrSource[chrSource.size() - 2]);
	chrStage1Rest.push_back(chrSource[chrSource.size() - 1]);
}

void pack2()
{
	//
	// Stage 2: Search for repeating patterns
	// 	
	unordered_map<int, unordered_map<pattern, int>> dict; // key1 - winLen, key2 - pattern, value - counter for pattern.
	unordered_map<pattern, vector<int>> dictTmp; // key1 - pattern, pair counter, pair vector - positions of found patterns
	map<int, unordered_map<pattern, vector<int>>> dictTmp2; // ordered map! important for iteration below. key1 is winLen, key2 is pattern, vector of positions where pattern was found
	list<int> memoWinLen;
	int sizeTmp = 0;
	int winLen = gWIN_LEN;

	enum class eDirection {
		dKeep,
		dUp,
		dDown,
		dStop
	};
	if (chrStage1Rest.size() > winLen)
	{
		auto func = [&dictTmp2](int winLen) {
			unordered_map<pattern, vector<int>> dictTmp;
			pattern key;
			int numEntries = 0;
			for (int i = 0; (i <= chrStage1Rest.size() - winLen); i++)
			{				
				key = 0;
				memcpy(&key, &chrStage1Rest[i], winLen);
				dictTmp[key].push_back(i);

				if (dictTmp[key].size() == 2) // more than 1 but not count twice
					numEntries++;
				if (numEntries > ((1 << (min(4, winLen / 2) * 8)) - 1))
				{
					return;
				}
			}

			for (auto& it : dictTmp)
			{
				if (it.second.size() > 1)
				{
					dictTmp2[winLen][it.first + (pattern(winLen) << 56) ] = it.second;
				}
			}
			dictTmp.clear();
		};

		int i = -1;
		vector<thread> t(gWIN_LEN - gWIN_LEN_SMALLEST+1);
		for (int winLen = gWIN_LEN_SMALLEST; winLen <= gWIN_LEN; winLen++)
		{
			i++;
			t[i]= thread(bind(func, winLen));			
		}		
		for (auto& it : t)
		{
			it.join();
		}

		// remove crossing positions of patterns BETWEEN DIFFERENT WINLEN, searching from bigger to smaller, removing smaller
		auto ritWinLen = dictTmp2.rbegin();
		auto ritWinLenNext = ritWinLen;
		while (ritWinLen != dictTmp2.rend()) // winLen
		{
			vector<pattern> posSorted;

			auto fillSorted = [&]() {
				//sort patterns by counter value
				posSorted.clear();
				auto itSorted = posSorted.begin();
				for (auto& itPattern : (*ritWinLen).second)
				{
					while (itSorted != posSorted.end())
					{
						if (itPattern.second.size() < (*ritWinLen).second[(*itSorted)].size())
						{
							break;
						}
						itSorted++;
					}
					posSorted.insert(itSorted, itPattern.first);
					itSorted = posSorted.begin();
				}
			};

			fillSorted();
			// remove crossing positions for each pattern starting from less often 
			if (posSorted.size() > 0)
				for (auto& itPattern = posSorted.begin(); itPattern != posSorted.end(); itPattern++)
				{
					bool found = false;
					auto posSmallPattern = (*ritWinLen).second[(*itPattern)].begin();
					auto posBigPattern = posSmallPattern;
					if (posBigPattern == (*ritWinLen).second[(*itPattern)].end())
						continue;

					while (posSmallPattern != (*ritWinLen).second[(*itPattern)].end())
					{
						found = false;
						posBigPattern = posSmallPattern;
						posBigPattern++;
						while (posBigPattern != (*ritWinLen).second[(*itPattern)].end())
						{
							if (((*posSmallPattern) > (*posBigPattern) && (*posSmallPattern) < (*posBigPattern) + (*ritWinLen).first) ||
								((*posSmallPattern) + (*ritWinLen).first > (*posBigPattern) && (*posSmallPattern) + (*ritWinLen).first < (*posBigPattern) + (*ritWinLen).first))
							{
								/*posSmallPattern = */(*ritWinLen).second[(*itPattern)].erase(posBigPattern);
								found = true;
								break;
							}
							else
							{
								if ((*posSmallPattern) + (*ritWinLen).first < (*posBigPattern))
								{
									break;
								}
							}
							posBigPattern++;
						}
						if ((*ritWinLen).second[(*itPattern)].size() < 2)
						{
							(*ritWinLen).second.erase((*itPattern));
							break;
						}
						if (!found)
						{
							if (posSmallPattern != (*ritWinLen).second[(*itPattern)].end())
								posSmallPattern++;
						}
					}
				}

			fillSorted();
			// remove crossing positions between patterns OF ONE WINLEN!!!
			if (posSorted.size() > 0)
				for (auto& itPattern = posSorted.begin(); itPattern != posSorted.end() - 1; itPattern++)
				{
					for (auto& itPattern2 = itPattern + 1; itPattern2 != posSorted.end(); itPattern2++)
					{
						bool found = false;
						auto posSmallPattern = (*ritWinLen).second[(*itPattern)].begin();
						auto posBigPattern = (*ritWinLen).second[(*itPattern2)].begin();
						if (posBigPattern == (*ritWinLen).second[(*itPattern2)].end())
							continue;

						while (posSmallPattern != (*ritWinLen).second[(*itPattern)].end())
						{
							found = false;
							posBigPattern = (*ritWinLen).second[(*itPattern2)].begin();
							//posBigPattern++;
							while (posBigPattern != (*ritWinLen).second[(*itPattern2)].end())
							{
								if (((*posSmallPattern) > (*posBigPattern) && (*posSmallPattern) < (*posBigPattern) + (*ritWinLen).first) ||
									((*posSmallPattern) + (*ritWinLen).first > (*posBigPattern) && (*posSmallPattern) + (*ritWinLen).first < (*posBigPattern) + (*ritWinLen).first))
								{
									posSmallPattern = (*ritWinLen).second[(*itPattern)].erase(posSmallPattern);
									found = true;
									break;
								}
								posBigPattern++;
							}
							if ((*ritWinLen).second[(*itPattern)].size() < 1)
							{
								(*ritWinLen).second.erase((*itPattern));
								break;
							}
							if (!found)
							{
								if (posSmallPattern != (*ritWinLen).second[(*itPattern)].end())
									posSmallPattern++;
							}
						}
					}
				}

			ritWinLenNext = ritWinLen;
			ritWinLenNext++;
			while (ritWinLenNext != dictTmp2.rend())
			{
				std::size_t found;
				auto itBigPatterns = (*ritWinLen).second.begin();
				while (itBigPatterns != (*ritWinLen).second.end())
				{
					bool found = false;
					vector<int> posSave;
					auto itSmallPatterns = (*ritWinLenNext).second.begin();
					while (itSmallPatterns != (*ritWinLenNext).second.end())
					{
						posSave.clear();
						auto posSmallPatterns = (*itSmallPatterns).second.begin();
						while (posSmallPatterns != (*itSmallPatterns).second.end())
						{
							auto posBigPatterns = (*itBigPatterns).second.begin();
							while (posBigPatterns != (*itBigPatterns).second.end())
							{
								int smLen = (*ritWinLenNext).first;
								if (((*posSmallPatterns) > (*posBigPatterns) && (*posSmallPatterns) < (*posBigPatterns) + (*ritWinLen).first) ||
									((*posSmallPatterns) + smLen > (*posBigPatterns) && (*posSmallPatterns) + (*ritWinLenNext).first < (*posBigPatterns) + (*ritWinLen).first))
								{
									found = true;
									break;
								}
								else
								{
									if ((*posSmallPatterns)  > (*posBigPatterns)+ (*ritWinLen).first)
										break;
								}
								posBigPatterns++;
							}
							if (!found)
							{
								posSave.push_back((*posSmallPatterns));
							}
							posSmallPatterns++;
						}
						if (posSave.size() > 1)
						{
							(*itSmallPatterns).second = posSave;
							itSmallPatterns++;
						}
						else
							itSmallPatterns = (*ritWinLenNext).second.erase(itSmallPatterns);
					}
					itBigPatterns++;
				}
				if ((*ritWinLenNext).second.size() < 1)
				{
					auto it = dictTmp2.erase(--ritWinLenNext.base());
					ritWinLenNext = ritWinLen;
					ritWinLenNext++;
				}
				else
					ritWinLenNext++;
			}
			ritWinLen++;
		}
	}

	map<int, shared_ptr<pattern>> dictTmp3; // key is pos
	for (auto& itWinLen : dictTmp2)
	{
		for (auto& itPattern : itWinLen.second)
		{
			shared_ptr<pattern> pn=make_shared<pattern>(itPattern.first);
			for (auto& itPos : itPattern.second)
			{
				dictTmp3[itPos] = pn;
			}
		}
	}
	
	for (auto it = dictTmp3.rbegin(); it != dictTmp3.rend(); it++)
	{
		dictPatterns[(*it).second].emplace(dictPatterns[(*it).second].begin(),(*it).first);
		chrStage1Rest.erase(chrStage1Rest.begin()+ (*it).first, chrStage1Rest.begin() + (*it).first+(*(*it).second.get() >> 56));
	}
}

void saveArchive(const std::string& sPathArchive)
{
	// Stage 3: save archive to file
	std::ofstream outputFile(sPathArchive, ios::binary);

	if (!outputFile.is_open())
	{
		cout << "could not open file " << sPathArchive;
	}

	typedef unsigned int LONGWORD;
	LONGWORD size = chrStage1Rest.size();
	char byte4[sizeof(LONGWORD)];
	LONGWORD* byte4p = (LONGWORD*)&byte4;
	*byte4p = size;

	//write len of the not packed vector
	outputFile.write((char*)&byte4, sizeof(byte4));
	//write elements of not packed vector
	for (auto elem : chrStage1Rest)
	{
		*byte4p = elem;
		outputFile.write((char*)&byte4, 1); //1 byte
	}

	//write size of dictitonary	
	size = dictPatterns.size();
	*byte4p = size;
	outputFile.write((char*)&byte4, sizeof(byte4));

	//write pattern dictionary
	for (auto it : dictPatterns)
	{
		// size of pattern
		size = *it.first.get() >> 56;
		*byte4p = size;
		outputFile.write((char*)&byte4, sizeof(byte4));
		// write pattern
		outputFile.write((char*)(it.first.get()), size);
		//write len of positions vector
		size = it.second.size();
		*byte4p = size;
		outputFile.write((char*)&byte4, sizeof(byte4));
		//write positions one by one, 4 bytes every
		for (auto it2 : it.second)
		{
			size = it2;
			*byte4p = size;
			outputFile.write((char*)&byte4, sizeof(byte4));			
		}
	}
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
	sPathArchive = sPathParent + "archive.gdv";
	sPathUnpacked = sPathParent + "unpacked.txt";

	if (argc>1 && string(args[1])=="u")
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
			ifstream inFile(sPathSource);
			if (!inFile.is_open())
			{
				cout << " File open failed " << sPathSource << endl;								
				exit(1);
			}

			int a;
			string s;
			while (!inFile.eof())
			{
				string s;
				inFile >> s;
				if (!s.empty())
				{
					a = atoi(s.c_str());
					source.push_back(a);
				}
			} 			
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
	pack2();

	saveArchive(sPathArchive);

    return 0;

}

