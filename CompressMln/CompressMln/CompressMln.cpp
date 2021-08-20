// CompressMln.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <functional>
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <conio.h>

#define gMAX_LEN 1000000
#define gMAX_VALUE 100
#define gMIN_VALUE 1
#define gWIN_LEN 5
#define gWIN_LEN_SMALLEST 3

using namespace std;
namespace fs = std::filesystem;

int main(int argc, const char* args[])
{		
	size_t found;
	string sTmp(args[0]);
	found = sTmp.find_last_of("/\\");
	//cout << " folder: " << sTmp.substr(0, found) << endl;
	//cout << " file: " << sTmp.substr(found + 1) << endl;

	fs::path path(sTmp.substr(0, found));

	string sPathParent = path.parent_path().string() + "\\";
	cout << sPathParent << endl;

	string sPathSource = sPathParent + "source.txt";

	vector<int> source;

	auto saveMln = [&]()
	{
		std::ofstream outputFile(sPathSource);

		if (!outputFile.is_open())
		{
			cout << "could not open file "<< sPathSource;
		}

		for (auto it : source)
		{
			outputFile << it << "\n";
		}
		outputFile.close();
	};

	auto genereateMln = [&]() 
	{ 
		srand(time(0));
		source.resize(gMAX_LEN);
		int rangeProvider = gMAX_VALUE - gMIN_VALUE + 1;
		for (int i = 0; i < gMAX_LEN; i++)
		{			
			source[i]=rand() % (rangeProvider) + gMIN_VALUE;
			//source[i] = 2;
		}
	};

	auto compare = [](const vector<char> vec, int leftCheck, int leftWin, int size) 
	{
		for (int i = 0; i < size; i++)
		{
			if (vec[i + leftCheck] != vec[i + leftWin])
				return false;
		}
		return true;
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
				saveMln();
			}
		}
	}
	else 
	{
		genereateMln();
		saveMln();
	}

	// compress by elements size (max=100 need 7 bits)
	vector<char> chrSource(source.begin(),source.end());
#if gWIN_LEN
	int leftOfSeq(fmax(chrSource.size()-gWIN_LEN, 3));
#else
	int leftOfSeq(chrSource.size() / 2 + 1);
#endif
	int sizeOfSeq(chrSource.size() - leftOfSeq);

	typedef int counter;


	//string lastKey = "";
	// 
	// ›“¿œ 1 œŒ»—  ¬—≈’ œŒƒ–ﬂƒ œŒ¬“Œ–ﬂﬁŸ»’—ﬂ ›À≈Ã≈Õ“Œ¬ ‰ÎËÌÓÈ 3 Ë ·ÓÎ¸¯Â
	struct stage1Helper
	{
		stage1Helper()
			: pos(0)
			, len(0)
		{

		}

		int pos;
		int len;
	};
	
	unordered_map<char, map<int, int>> chrStage1; // key1 is element, key2 is pos, value is len of sequence at this pos
	vector<char> chrStage1Rest;
	int idxSequence = 0;
	for (int i = 0; i < chrSource.size() - 2; i++)
	{
		if (chrSource[i] == chrSource[i + 1] && chrSource[i] == chrSource[i + 2])
		{
			idxSequence = i;
			chrStage1[chrSource[i]][idxSequence] += 3;

			i += 2;
			for (int c = i; c < chrSource.size() - 1; c++)
			{
				i++;
				if (chrSource[c] != chrSource[c + 1])
				{
					break;
				}
				else
				{
					chrStage1[chrSource[i]][idxSequence]++;
				}
			}
		}
		else
		{
			chrStage1Rest.push_back(chrSource[i]);
		}

		// i points to 1st element after sequence (if any)
	}
	chrStage1Rest.push_back(chrSource[chrSource.size() - 2]);
	chrStage1Rest.push_back(chrSource[chrSource.size() - 1]);

	// ›“¿œ 2 œŒ»—  œŒ¬“Œ–ÕŒ ¬—“–≈◊¿ﬁŸ»’—ﬂ  Àﬁ◊≈…
	// 
	unordered_map<int, unordered_map<string, counter>> dict; // key1 - winLen, key2 - pattern, value - counter for pattern.
	unordered_map<string, vector<int>> dictTmp; // key1 - pattern, pair counter, pair vector - positions of found patterns
	map<int, unordered_map<string, vector<int>>> dictTmp2; // ordered map! important for iteration below. key1 is winLen, key2 is pattern, pair counter, pair vector of positions where pattern was found
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
		eDirection flagDirection = eDirection::dKeep;
		int numEntries;
		while (winLen >= gWIN_LEN_SMALLEST)
		{
			memoWinLen.push_back(winLen);
			flagDirection = eDirection::dKeep;
			numEntries = 0;
			for (int i = 0; (i <= chrStage1Rest.size() - winLen)/* && (dict.size()<255)*/; i++)
			{
				dictTmp[string(chrStage1Rest.begin() + i, chrStage1Rest.begin() + i + winLen)].push_back(i);

				if (dictTmp[string(chrStage1Rest.begin() + i, chrStage1Rest.begin() + i + winLen)].size() == 2) // more than 1 but not count twice
					numEntries++;
				if (numEntries > ((1 << (min(4, winLen / 2) * 8)) - 1))
				{
					flagDirection = eDirection::dUp;
					break;
				}
			}
			
			if (flagDirection == eDirection::dKeep)
			{				
				for (auto& it : dictTmp)
				{
					if (it.second.size() > 1)
					{
						dictTmp2[winLen][it.first] = it.second;
					}
				}
				dictTmp.clear();

				//to optimize here: some patterns may repeat many times, and this will be space saving
				if (dictTmp2[winLen].size() > ((1 << (min(4, winLen / 2) * 8)) - 1)) // we cannot enumerate all patterns and write an index at the place of pattern,
					//because the index requires more bytes than the length of a patern (no space saving)
				{
					flagDirection = eDirection::dUp;
				}
				else
				if (dictTmp2[winLen].size() < 127)
				{
					flagDirection = eDirection::dDown;
				}
				else
				{
					flagDirection = eDirection::dDown;
				}
			}			

			switch (flagDirection)
			{				
				case eDirection::dUp:
				{
					winLen++;					
					break;
				}
				case eDirection::dDown:
				{
					winLen--; break;
				}
			}
			if (find(memoWinLen.begin(), memoWinLen.end(), winLen) != memoWinLen.end()) // already searched for patterns of this size and found some
			{
				flagDirection = eDirection::dStop;
				break;
			}

			if (flagDirection == eDirection::dStop || flagDirection == eDirection::dKeep)
				break;
			// sort from biggest counter to smallest
			// add to dict with positions
			// scan array and remove the most often patterns
			vector<char> chrStage2Rest;

			/*if (sizeTmp >= dict.size())
				break;*/
		}

		// remove crossing positions of patterns BETWEEN DIFFERENT WINLEN, searching from bigger to smaller, removing smaller
		auto ritWinLen = dictTmp2.rbegin();
		auto ritWinLenNext = ritWinLen;
		while (ritWinLen != dictTmp2.rend()) // winLen
		{	
			vector<string> posSorted;

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
							posSmallPattern = (*ritWinLen).second[(*itPattern)].erase(posSmallPattern);
							found = true;
							break;
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

	// Stage 3: save archive to file
	string sPathArchive = sPathParent + "archive.gdv";
	std::ofstream outputFile(sPathArchive, ios::binary);

	if (!outputFile.is_open())
	{
		cout << "could not open file " << sPathArchive;
	}

	typedef unsigned int LONGWORD;
	LONGWORD size = chrStage1.size();
	char byte4[sizeof(LONGWORD)];
	LONGWORD* byte4p = (LONGWORD *)&byte4;
	*byte4p = size;

	outputFile.write((char*)&byte4, sizeof(byte4));
	for (auto elem : chrStage1)
	{
		outputFile << elem.first; // write element as a key

		for (auto pos : elem.second)
		{			
			*byte4p = pos.first; // write pos
			outputFile.write((char*)&byte4, sizeof(byte4));

			if (pos.first < 128) // write pos
			{
				char c = pos.first | 128;
				outputFile << c;
			}
			else
			{
				*byte4p = pos.first;
				outputFile.write((char*)&byte4, 4);
			}

			if (pos.second<128) // write len
			{ 
				char c = pos.second | 128;
				outputFile << c;
			}
			else
			{
				*byte4p = pos.second; 
				outputFile.write((char*)&byte4, 4);
			}
			
		}
	}

	outputFile.close();

    return 0;

}

