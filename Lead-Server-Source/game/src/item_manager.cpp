#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "desc_client.h"
#include "db.h"
#include "log.h"
#include "item.h"
#include "item_manager.h"
#include "item_manager_private_types.h"
#include "text_file_loader.h"
#include "mob_manager.h"
#include "questmanager.h"

#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <charconv>
#include <string_view>
#include <ranges>

std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];

struct VnumRange { DWORD start; DWORD end; };

static std::vector<VnumRange> ParseVnumConfig(std::string_view input)
{
	std::vector<VnumRange> results;
	if (input.empty()) 
		return results;

	size_t first = 0;
	while (first < input.size())
	{
		const auto second = input.find_first_of(',', first);
		std::string_view segment;

		if (second == std::string_view::npos)
		{
			segment = input.substr(first);
			first = input.size();
		}
		else
		{
			segment = input.substr(first, second - first);
			first = second + 1;
		}

		if (segment.empty()) 
			continue;

		if (size_t tilde = segment.find('~'); tilde != std::string_view::npos)
		{
			DWORD s = 0, e = 0;
			std::from_chars(segment.data(), segment.data() + tilde, s);
			std::from_chars(segment.data() + tilde + 1, segment.data() + segment.size(), e);

			if (s > 0 || e > 0)
			{
				if (s > e) std::swap(s, e);
				results.push_back({ s, e });
			}
		}
		else
		{
			DWORD s = 0;
			std::from_chars(segment.data(), segment.data() + segment.size(), s);
			if (s > 0) results.push_back({ s, s });
		}
	}
	return results;
}

static int SafeStringToInt(std::string_view str)
{
	int value = 0;
	auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
	if (ec != std::errc()) return 0;
	return value;
}

int ReadCommon_GetMobType(const char* szTypeName)
{
	if (!strcmp(szTypeName, "PAWN")) return MOB_RANK_PAWN;
	if (!strcmp(szTypeName, "S_PAWN")) return MOB_RANK_S_PAWN;
	if (!strcmp(szTypeName, "KNIGHT")) return MOB_RANK_KNIGHT;
	if (!strcmp(szTypeName, "S_KNIGHT")) return MOB_RANK_S_KNIGHT;
	if (!strcmp(szTypeName, "BOSS")) return MOB_RANK_BOSS;
	if (!strcmp(szTypeName, "KING")) return MOB_RANK_KING + 1;
	return -1;
}

int ReadCommon_GetJob(const char* szTypeName)
{
	if (!strcmp(szTypeName, "WARRIOR")) return JOB_WARRIOR + 1;
	if (!strcmp(szTypeName, "ASSASSIN")) return JOB_ASSASSIN + 1;
	if (!strcmp(szTypeName, "SURA")) return JOB_SURA + 1;
	if (!strcmp(szTypeName, "SHAMAN")) return JOB_SHAMAN + 1;
	return 0;
}

std::pair<bool, std::string> ITEM_MANAGER::ReadCommonDropItemFile(const char* c_pszFileName)
{
	std::ifstream file(c_pszFileName);
	if (!file.is_open()) return { false, "Cannot open file" };

	std::vector<CItemDropInfo> tempCommonDropItem[MOB_RANK_MAX_NUM];
	std::string line;
	int lineNum = 0;

	while (std::getline(file, line))
	{
		++lineNum;
		auto is_space = [](unsigned char c) { return std::isspace(c); };
		if (std::all_of(line.begin(), line.end(), is_space)) continue;
		if (line.back() == '\r') line.pop_back();

		size_t first_char = line.find_first_not_of(" \t");
		if (first_char == std::string::npos) continue;
		if (line[first_char] == '#' || (line.size() > first_char + 1 && line[first_char] == '/' && line[first_char + 1] == '/'))
			continue;

		std::vector<std::string> tokens;
		std::stringstream ss(line);
		std::string token;
		while (std::getline(ss, token, '\t'))
			if (!token.empty()) tokens.push_back(token);

		if (tokens.size() < 6) continue;

		int type = ReadCommon_GetMobType(tokens[0].c_str());
		int iLvStart = 0, iLvEnd = 0, iCount = 0, iJob = 0;
		float fPercent = 0.0f;

		try {
			iLvStart = std::stoi(tokens[1]);
			iLvEnd = std::stoi(tokens[2]);
			fPercent = std::stof(tokens[3]);
			iCount = std::stoi(tokens[5]);
			if (tokens.size() > 6) iJob = ReadCommon_GetJob(tokens[6].c_str());
		}
		catch (...) {
			return { false, "Number parsing error at line " + std::to_string(lineNum) };
		}

		DWORD dwPct = (DWORD)(fPercent * 10000.0f + 0.5f);
		auto ranges = ParseVnumConfig(tokens[4]);

		if (ranges.empty()) continue;

		for (const auto& [start, end] : ranges)
		{
			if (!GetTable(start))
				return { false, "Invalid Vnum " + std::to_string(start) + " at line " + std::to_string(lineNum) };

			if (iLvStart == 0) continue;

			CItemDropInfo info(iLvStart, iLvEnd, dwPct, start, end, iCount, iJob);

			if (type == -1) {
				for (int i = MOB_RANK_PAWN; i <= MOB_RANK_S_KNIGHT; ++i)
					tempCommonDropItem[i].push_back(info);
			}
			else if (type >= 0 && type < MOB_RANK_MAX_NUM) {
				tempCommonDropItem[type].push_back(info);
			}
		}
	}

	for (int i = 0; i < MOB_RANK_MAX_NUM; ++i)
	{
		std::sort(tempCommonDropItem[i].begin(), tempCommonDropItem[i].end());
		g_vec_pkCommonDropItem[i].swap(tempCommonDropItem[i]);
	}

	return { true, "" };
}

std::pair<bool, std::string> ITEM_MANAGER::ReadEtcDropItemFile(const char* c_pszFileName)
{
	std::ifstream file(c_pszFileName);
	if (!file.is_open()) return { false, "Cannot open file" };

	std::map<DWORD, DWORD> tempEtcMap;
	std::string line;
	int lineNum = 0;

	while (std::getline(file, line))
	{
		++lineNum;
		size_t first = line.find_first_not_of(" \t\r\n");
		if (first == std::string::npos || line[first] == '#') continue;

		size_t lastTab = line.find_last_of('\t');
		if (lastTab == std::string::npos) continue;

		std::string itemName = line.substr(0, lastTab);
		size_t nameEnd = itemName.find_last_not_of(" \t");
		if (nameEnd != std::string::npos) itemName = itemName.substr(0, nameEnd + 1);

		std::string probStr = line.substr(lastTab + 1);
		float fProb = 0.0f;
		try { fProb = std::stof(probStr); }
		catch (...) { continue; }

		if (fProb <= 0.0f) continue;

		auto ranges = ParseVnumConfig(itemName);
		if (ranges.empty()) {
			DWORD dwVnum = 0;
			if (GetVnumByOriginalName(itemName.c_str(), dwVnum))
				ranges.push_back({ dwVnum, dwVnum });
		}

		for (const auto& [start, end] : ranges)
		{
			for (DWORD v = start; v <= end; ++v)
			{
				if (!GetTable(v)) continue;
				tempEtcMap[v] = (DWORD)(fProb * 10000.0f);
			}
		}
	}

	m_map_dwEtcItemDropProb = tempEtcMap;
	return { true, "" };
}

std::pair<bool, std::string> ITEM_MANAGER::ReadDropItemGroup(const char* c_pszFileName)
{
	CTextFileLoader loader;
	if (!loader.Load(c_pszFileName)) return { false, "Cannot open file (Load failed)" };

	std::map<DWORD, CDropItemGroup*> tempDropItemGr;

	auto cleanup = [&]() {
		for (auto& pair : tempDropItemGr) M2_DELETE(pair.second);
		};

	std::string stName;

	for (DWORD i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);
		loader.GetCurrentNodeName(&stName);

		int iVnum = 0, iMobVnum = 0;
		if (!loader.GetTokenInteger("vnum", &iVnum)) iVnum = 0;

		if (!loader.GetTokenInteger("mob", &iMobVnum))
		{
			cleanup();
			return { false, "Syntax error (missing mob) in node " + stName };
		}

		CDropItemGroup* pkGroup = nullptr;
		if (auto it = tempDropItemGr.find(iMobVnum); it != tempDropItemGr.end())
			pkGroup = it->second;
		else
		{
			pkGroup = M2_NEW CDropItemGroup(iVnum, iMobVnum, stName);
			tempDropItemGr[iMobVnum] = pkGroup;
		}

		for (int k = 1; k < MAX_GROUP_ITEM_NUM; ++k)
		{
			std::string key = std::to_string(k);
			TTokenVector* pTok;

			if (loader.GetTokenVector(key, &pTok))
			{
				if (pTok->size() < 2) {
					cleanup();
					return { false, "Missing columns at Group " + stName + " Line " + key };
				}

				auto ranges = ParseVnumConfig(pTok->at(0));
				if (ranges.empty() || !GetTable(ranges[0].start))
				{
					cleanup();
					return { false, "Invalid item " + pTok->at(0) + " in Group " + stName };
				}

				float fPercent = 0.0f;
				try { fPercent = std::stof(pTok->at(1)); }
				catch (...) {
					cleanup();
					return { false, "Invalid percent in Group " + stName };
				}

				int iCount = 1;
				if (pTok->size() > 2) iCount = SafeStringToInt(pTok->at(2));
				if (iCount < 1) iCount = 1;

				for (const auto& [start, end] : ranges)
				{
					pkGroup->AddItem(start, end, (DWORD)(fPercent * 10000.0f), iCount);
				}
			}
			else break;
		}
		loader.SetParentNode();
	}

	for (auto& pair : m_map_pkDropItemGroup) M2_DELETE(pair.second);
	m_map_pkDropItemGroup.clear();
	m_map_pkDropItemGroup = tempDropItemGr;

	return { true, "" };
}

std::pair<bool, std::string> ITEM_MANAGER::ReadMonsterDropItemGroup(const char* c_pszFileName)
{
	CTextFileLoader loader;
	if (!loader.Load(c_pszFileName)) return { false, "Cannot open file" };

	std::map<DWORD, std::vector<CMobItemGroup*> > tempMobItemGr;
	std::map<DWORD, CDropItemGroup*> tempDropItemGr;
	std::map<DWORD, CLevelItemGroup*> tempLevelItemGr;

	auto cleanup = [&]() {
		for (auto& p : tempMobItemGr) for (auto* g : p.second) M2_DELETE(g);
		for (auto& p : tempDropItemGr) M2_DELETE(p.second);
		for (auto& p : tempLevelItemGr) M2_DELETE(p.second);
		};

	for (DWORD i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		std::string stName;
		loader.GetCurrentNodeName(&stName);
		loader.SetChildNode(i);

		std::string strType, strMobVnumList;
		int iKillDrop = 1, iLevelStart = 0, iLevelEnd = 0;

		if (!loader.GetTokenString("type", &strType) || !loader.GetTokenString("mob", &strMobVnumList))
		{
			cleanup();
			return { false, "Missing type/mob in node " + stName };
		}

		auto mobRanges = ParseVnumConfig(strMobVnumList);

		if (strType == "kill") loader.GetTokenInteger("kill_drop", &iKillDrop);
		else if (strType == "limit") {
			loader.GetTokenInteger("level_limit_start", &iLevelStart);
			loader.GetTokenInteger("level_limit_end", &iLevelEnd);
		}

		for (const auto& [mobStart, mobEnd] : mobRanges)
		{
			for (DWORD mobVnum = mobStart; mobVnum <= mobEnd; ++mobVnum)
			{
				if (strType == "kill")
				{
					auto& groupVec = tempMobItemGr[mobVnum];
					CMobItemGroup* group = M2_NEW CMobItemGroup(mobVnum, iKillDrop, stName);

					for (int k = 1; k < MAX_GROUP_ITEM_NUM; ++k)
					{
						std::string key = std::to_string(k);
						TTokenVector* pTok;
						if (!loader.GetTokenVector(key, &pTok)) break;

						auto itemRanges = ParseVnumConfig(pTok->at(0));
						if (itemRanges.empty() || !GetTable(itemRanges[0].start)) {
							M2_DELETE(group); cleanup();
							return { false, "Invalid item " + pTok->at(0) + " in node " + stName };
						}

						int count = SafeStringToInt(pTok->at(1));
						int pct = SafeStringToInt(pTok->at(2));
						int rare = (pTok->size() > 3) ? SafeStringToInt(pTok->at(3)) : 0;

						for (const auto& [iStart, iEnd] : itemRanges)
							group->AddItem(iStart, iEnd, count, pct, rare);
					}
					groupVec.push_back(group);
				}
				else if (strType == "drop")
				{
					CDropItemGroup* group = nullptr;
					if (tempDropItemGr.contains(mobVnum)) group = tempDropItemGr[mobVnum];
					else {
						group = M2_NEW CDropItemGroup(0, mobVnum, stName);
						tempDropItemGr[mobVnum] = group;
					}

					for (int k = 1; k < MAX_GROUP_ITEM_NUM; ++k)
					{
						std::string key = std::to_string(k);
						TTokenVector* pTok;
						if (!loader.GetTokenVector(key, &pTok)) break;

						auto itemRanges = ParseVnumConfig(pTok->at(0));
						if (itemRanges.empty() || !GetTable(itemRanges[0].start)) {
							cleanup();
							return { false, "Invalid item " + pTok->at(0) + " in node " + stName };
						}

						int count = SafeStringToInt(pTok->at(1));
						float fPct = 0.0f;
						try { fPct = std::stof(pTok->at(2)); }
						catch (...) {
							cleanup(); return { false, "Invalid percent in node " + stName };
						}

						for (const auto& [iStart, iEnd] : itemRanges)
							group->AddItem(iStart, iEnd, (DWORD)(fPct * 10000.0f), count);
					}
				}
				else if (strType == "limit")
				{
					CLevelItemGroup* group = M2_NEW CLevelItemGroup(iLevelStart, iLevelEnd);
					for (int k = 1; k < MAX_GROUP_ITEM_NUM; ++k)
					{
						std::string key = std::to_string(k);
						TTokenVector* pTok;
						if (!loader.GetTokenVector(key, &pTok)) break;
						auto itemRanges = ParseVnumConfig(pTok->at(0));
						if (itemRanges.empty() || !GetTable(itemRanges[0].start)) {
							M2_DELETE(group); cleanup();
							return { false, "Invalid item " + pTok->at(0) + " in node " + stName };
						}
						int count = SafeStringToInt(pTok->at(1));
						float fPct = 0.0f;
						try { fPct = std::stof(pTok->at(2)); }
						catch (...) {
							M2_DELETE(group); cleanup(); return { false, "Invalid percent in node " + stName };
						}

						for (const auto& [iStart, iEnd] : itemRanges)
							group->AddItem(iStart, iEnd, (DWORD)(fPct * 10000.0f), count);
					}
					if (tempLevelItemGr.contains(mobVnum)) M2_DELETE(tempLevelItemGr[mobVnum]);
					tempLevelItemGr[mobVnum] = group;
				}
			}
		}
		loader.SetParentNode();
	}

	for (auto& p : m_map_pkMobItemGroup) for (auto* g : p.second) M2_DELETE(g);
	m_map_pkMobItemGroup.clear();

	for (auto& p : m_map_pkLevelItemGroup) M2_DELETE(p.second);
	m_map_pkLevelItemGroup.clear();

	for (auto& p : m_map_pkDropItemGroup) M2_DELETE(p.second);
	m_map_pkDropItemGroup.clear();

	m_map_pkDropItemGroup = tempDropItemGr;

	m_map_pkMobItemGroup = tempMobItemGr;
	m_map_pkLevelItemGroup = tempLevelItemGr;

	return { true, "" };
}

std::pair<bool, std::string> ITEM_MANAGER::ReadSpecialDropItemFile(const char* c_pszFileName)
{
	CTextFileLoader loader;
	if (!loader.Load(c_pszFileName)) return { false, "File not found" };

	std::map<DWORD, CSpecialAttrGroup*> tempSpecAttr;
	std::map<DWORD, CSpecialItemGroup*> tempSpecItem;
	std::map<DWORD, CSpecialItemGroup*> tempSpecItemQuest;
	std::map<DWORD, int> tempItemToGroupMap;

	auto cleanup = [&]() {
		for (auto& p : tempSpecAttr) M2_DELETE(p.second);
		for (auto& p : tempSpecItem) M2_DELETE(p.second);
		for (auto& p : tempSpecItemQuest) M2_DELETE(p.second);
		};

	for (DWORD i = 0; i < loader.GetChildNodeCount(); ++i) {
		loader.SetChildNode(i);
		std::string stName; loader.GetCurrentNodeName(&stName);

		int iVnum = 0;
		if (!loader.GetTokenInteger("vnum", &iVnum)) { cleanup(); return { false, "No vnum in " + stName }; }

		std::string stType; loader.GetTokenString("type", &stType);

		if (stType == "attr") {
			CSpecialAttrGroup* pkGroup = M2_NEW CSpecialAttrGroup(iVnum);
			for (int k = 1; k < MAX_GROUP_ITEM_NUM; ++k) {
				TTokenVector* pTok;
				if (loader.GetTokenVector(std::to_string(k), &pTok)) {
					DWORD applyType = FN_get_apply_type(pTok->at(0).c_str());
					if (applyType == 0) applyType = SafeStringToInt(pTok->at(0));
					pkGroup->m_vecAttrs.emplace_back(applyType, SafeStringToInt(pTok->at(1)));
				}
				else break;
			}
			tempSpecAttr[iVnum] = pkGroup;
		}
		else {
			int type = CSpecialItemGroup::NORMAL;
			if (stType == "pct") type = CSpecialItemGroup::PCT;
			else if (stType == "quest") { type = CSpecialItemGroup::QUEST; quest::CQuestManager::instance().RegisterNPCVnum(iVnum); }
			else if (stType == "special") type = CSpecialItemGroup::SPECIAL;

			CSpecialItemGroup* pkGroup = M2_NEW CSpecialItemGroup(iVnum, type);
			for (int k = 1; k < MAX_GROUP_ITEM_NUM; ++k) {
				TTokenVector* pTok;
				if (loader.GetTokenVector(std::to_string(k), &pTok)) {
					auto ranges = ParseVnumConfig(pTok->at(0));
					int count = SafeStringToInt(pTok->at(1));
					int prob = SafeStringToInt(pTok->at(2));
					int rare = (pTok->size() > 3) ? SafeStringToInt(pTok->at(3)) : 0;

					for (const auto& r : ranges) {
						pkGroup->AddItem(r.start, r.end, count, prob, rare);
						for (DWORD v = r.start; v <= r.end; ++v)
							if (iVnum < 30000) tempItemToGroupMap[v] = iVnum;
					}
				}
				else break;
			}
			if (type == CSpecialItemGroup::QUEST) tempSpecItemQuest[iVnum] = pkGroup;
			else tempSpecItem[iVnum] = pkGroup;
		}
		loader.SetParentNode();
	}

	for (auto& p : m_map_pkSpecialItemGroup) M2_DELETE(p.second);
	for (auto& p : m_map_pkQuestItemGroup) M2_DELETE(p.second);
	for (auto& p : m_map_pkSpecialAttrGroup) M2_DELETE(p.second);

	m_map_pkSpecialItemGroup = tempSpecItem;
	m_map_pkQuestItemGroup = tempSpecItemQuest;
	m_map_pkSpecialAttrGroup = tempSpecAttr;
	m_ItemToSpecialGroup = tempItemToGroupMap;
	return { true, "" };
}

