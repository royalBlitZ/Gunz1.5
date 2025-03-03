#include "stdafx.h"
#include "ZApplication.h"
#include "ZFile.h"
#include "ZGameClient.h"
#include "ZReplay.h"
#include "ZGame.h"
#include "ZNetCharacter.h"
#include "ZMyCharacter.h"
#include "ZPost.h"
#include "MMatchUtil.h"
#include "ZRuleDuel.h"
#include "ZRuleDuelTournament.h"
#include "ZRuleDeathMatch.h"
#include "ZWorldItem.h"
#include "ZRuleGunGame.h"
#include "../GunGame.h"
#include "ZRuleInfection.h"
#include "ZRuleSpyMode.h"
#include "../SpyMode.h"
#include "ZRuleQuestChallenge.h"
#include "ZActorWithFSM.h"
#include "ZActorActionManager.h"
bool g_bTestFromReplay = false;

bool CreateReplayGame(char *filename)
{
	static char szLastFile[256] = "";
	char szBuf[256];
	if (filename != NULL) strcpy(szBuf, filename);
	else strcpy(szBuf, szLastFile);

	if (filename != NULL) strcpy(szLastFile, filename);

	ZReplayLoader loader;
	if (!loader.Load(szBuf)) return false;
	ZGetGame()->OnLoadReplay(&loader);

	return true;

}


void ConvertStageSettingNodeForReplay(const REPLAY_STAGE_SETTING_NODE* pSource, MSTAGE_SETTING_NODE* pTarget)
{
//	_ASSERT(sizeof(REPLAY_STAGE_SETTING_NODE) == sizeof(MSTAGE_SETTING_NODE));
//	memcpy(pTarget, pSource, sizeof(REPLAY_STAGE_SETTING_NODE));

	pTarget->uidStage = pSource->uidStage;
	strcpy(pTarget->szMapName, pSource->szMapName);
	pTarget->nMapIndex = pSource->nMapIndex;
	pTarget->nGameType = pSource->nGameType;
	pTarget->nRoundMax = pSource->nRoundMax;
	pTarget->nLimitTime = pSource->nLimitTime;
	pTarget->nLimitLevel = pSource->nLimitLevel;
	pTarget->nMaxPlayers = pSource->nMaxPlayers;
	pTarget->bTeamKillEnabled = pSource->bTeamKillEnabled;
	pTarget->bTeamWinThePoint = pSource->bTeamWinThePoint;
	pTarget->bForcedEntryEnabled = pSource->bForcedEntryEnabled;
}

void ConvertStageSettingNodeForRecord(const MSTAGE_SETTING_NODE* pSource, REPLAY_STAGE_SETTING_NODE* pTarget)
{
	pTarget->uidStage = pSource->uidStage;
	strcpy(pTarget->szMapName, pSource->szMapName);
	pTarget->nMapIndex = pSource->nMapIndex;
	pTarget->nGameType = pSource->nGameType;
	pTarget->nRoundMax = pSource->nRoundMax;
	pTarget->nLimitTime = pSource->nLimitTime;
	pTarget->nLimitLevel = pSource->nLimitLevel;
	pTarget->nMaxPlayers = pSource->nMaxPlayers;
	pTarget->bTeamKillEnabled = pSource->bTeamKillEnabled;
	pTarget->bTeamWinThePoint = pSource->bTeamWinThePoint;
	pTarget->bForcedEntryEnabled = pSource->bForcedEntryEnabled;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int ZReplayLoader::m_nVersion=0;

ZReplayLoader::ZReplayLoader() : m_fGameTime(0.0f)
{
	memset(&m_StageSetting, 0, sizeof(REPLAY_STAGE_SETTING_NODE));
}

bool ZReplayLoader::Load(const char* filename)
{
	ZFile* file = zfopen(filename);

	if(!file) return false;
	
	if (!LoadHeader(file)) return false;
	if (!LoadStageSetting(file)) return false;
	ChangeGameState();

	if(m_nVersion>=4)
		if (!LoadStageSettingEtc(file)) return false;


	//Custom: NewQuest Replays!
	//if (m_StageSetting.nGameType == MMATCH_GAMETYPE_QUEST_CHALLENGE)
	//{
	//	if (!LoadNPCInfo(file))
	//	{
	//		return false;
	//	}
	//}

	if (!LoadCharInfo(file)) return false;


	if (!LoadCommandStream(file)) return false;

	zfclose(file);
	return true;
}

void ZReplayLoader::ChangeGameState()
{
	MSTAGE_SETTING_NODE stageSetting;
	memset(&stageSetting, 0, sizeof(MSTAGE_SETTING_NODE));
	ConvertStageSettingNodeForReplay(&m_StageSetting, &stageSetting);
	ZGetGameClient()->GetMatchStageSetting()->UpdateStageSetting(&stageSetting);
	ZApplication::GetStageInterface()->SetMapName(ZGetGameClient()->GetMatchStageSetting()->GetMapName());
	ZGetGameInterface()->SetState(GUNZ_GAME);
	ZGetCharacterManager()->Clear();
	ZGetObjectManager()->Clear();

#ifdef _REPLAY_TEST_LOG
	mlog("[Replay Start]\n");
	mlog("[Map:%s, LimitTime:%d, GemeType:%d]\n", stageSetting.szMapName, stageSetting.nLimitTime, stageSetting.nGameType);
#endif

}

bool ZReplayLoader::LoadHeader(ZFile* file)
{
	unsigned int version = 0;
	unsigned int header;
	int nRead;

	nRead = zfread(&header, sizeof(header), 1, file);
	if(nRead==0 || header!=GUNZ_REC_FILE_ID) return false;

	nRead = zfread(&version, sizeof(version), 1, file);
	if ( ( nRead == 0) || ( version > GUNZ_REC_FILE_VERSION))
		return false;

	m_nVersion = version;

	return true;
}

bool ZReplayLoader::LoadStageSetting(ZFile* file)
{
	// stage setting
	int nRead = zfread(&m_StageSetting, sizeof(REPLAY_STAGE_SETTING_NODE), 1, file);
	if(nRead==0) return false;

	return true;
}

bool ZReplayLoader::LoadStageSettingEtc(ZFile* file)
{
	// 게임룰 별 추가 세팅값 로드
	if(m_StageSetting.nGameType==MMATCH_GAMETYPE_DUEL)
	{
		ZRuleDuel* pDuel = (ZRuleDuel*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		int nRead = zfread(&pDuel->QInfo,sizeof(MTD_DuelQueueInfo),1,file);
		if(nRead==0) return false;
	}
	if(IsGameRuleCTF(m_StageSetting.nGameType))
	{
		ZRuleTeamCTF* pTeamCTF = (ZRuleTeamCTF*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		int nRead = zfread(&pTeamCTF->m_uidRedFlagHolder,sizeof(MUID),1,file);
		nRead = zfread(&pTeamCTF->m_uidBlueFlagHolder,sizeof(MUID),1,file);
		nRead = zfread(&pTeamCTF->m_RedFlagPos,sizeof(rvector),1,file);
		nRead = zfread(&pTeamCTF->m_BlueFlagPos,sizeof(rvector),1,file);
		int nRedFlagState = (int)pTeamCTF->GetRedFlagState();
		int nBlueFlagState = (int)pTeamCTF->GetBlueFlagState();
		nRead = zfread(&nRedFlagState,sizeof(int),1,file);
		nRead = zfread(&nBlueFlagState,sizeof(int),1,file);
		pTeamCTF->SetRedFlagState(nRedFlagState);
		pTeamCTF->SetBlueFlagState(nBlueFlagState);

		// now do logic. flag spawns are always up. we keep the state for archival purposes.

		ZGetWorldItemManager()->AddWorldItem(ZGetWorldItemManager()->GenStandAlondID(), CTF_RED_ITEM_ID,MTD_Static, rvector(pTeamCTF->GetRedFlagPos().x,pTeamCTF->GetRedFlagPos().y,pTeamCTF->GetRedFlagPos().z));
		ZGetWorldItemManager()->AddWorldItem(ZGetWorldItemManager()->GenStandAlondID(), CTF_BLUE_ITEM_ID,MTD_Static, rvector(pTeamCTF->GetBlueFlagPos().x,pTeamCTF->GetBlueFlagPos().y,pTeamCTF->GetBlueFlagPos().z));

		if(nRead==0) return false;
	}
	if(m_StageSetting.nGameType==MMATCH_GAMETYPE_DUELTOURNAMENT)
	{
		int tournamentType;
		int nRead = zfread(&tournamentType,sizeof(int),1,file);
		if(nRead==0) return false;

		int nCount = 0;
		nRead = zfread(&nCount,sizeof(int),1,file);
		if(nRead==0) return false;

		vector<DTPlayerInfo> vecPlayerInfo;
		DTPlayerInfo temp;
		for (int i=0; i<nCount; ++i)
		{
			nRead = zfread(&temp,sizeof(DTPlayerInfo),1,file);
			if(nRead==0) return false;
			vecPlayerInfo.push_back(temp);
		}

		ZGetGameInterface()->SetDuelTournamentCharacterList((MDUELTOURNAMENTTYPE)tournamentType, vecPlayerInfo);
		// 원래 게임에서는 이미 캐릭터목록이 세팅된 상태로 룰이 생성되는데, 리플레이는 순서가 반대라서 캐릭터 목록을 가져가도록 직접 지시해야한다..
		ZRuleDuelTournament* pRule = (ZRuleDuelTournament*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		pRule->InitCharacterList();

		// 녹화시작 당시의 경기는 m_DTGameInfo 커맨드를 이미 받아버린 상태였기때문에 따로 로드해서 넣어줘야 에너지바가 제대로 출력된다
		nRead = zfread(&pRule->m_DTGameInfo,sizeof(MTD_DuelTournamentGameInfo),1,file);
		if(nRead==0) return false;
	}
	if (m_StageSetting.nGameType == MMATCH_GAMETYPE_GUNGAME)
	{
		ZRuleGunGame* pRule = (ZRuleGunGame*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		
		int itemCount;
		int Read = zfread(&itemCount, sizeof(int), 1, file);
		if (Read == 0)
			return false;
		
		GGWeaponSet itemSet;
		for (int i = 0; i < itemCount; ++i)
		{
			Read = zfread(&itemSet, sizeof(GGWeaponSet), 1, file);
			pRule->matchSet.push_back(itemSet);
		}
	}
	if (m_StageSetting.nGameType == MMATCH_GAMETYPE_INFECTION)
	{
		ZRuleInfection* pRule = (ZRuleInfection*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		int Read = zfread(&pRule->uidFirstZombie, sizeof(MUID), 1, file);
		if (Read == 0)
			return false;
	}
	if (m_StageSetting.nGameType == MMATCH_GAMETYPE_SPYMODE)
	{
		ZRuleSpyMode* pRule = dynamic_cast<ZRuleSpyMode*>(ZGetGame()->GetMatch()->GetRule());

		int spyCount = 0;

		int	Read = zfread(&spyCount, sizeof(int), 1, file);
		if (Read == 0)
			return false;

		vector<MUID> Spies = {};
		MUID Spy;
		for (int i = 0; i < spyCount; ++i)
		{
			Read = zfread(&Spy, sizeof(MUID), 1, file);
			if (Read == 0)
				mlog("error reading spies\n");

			Spies.push_back(Spy);
		}
		pRule->SetSpies(Spies);

		int spyItemCount = 0;
		Read = zfread(&spyItemCount, sizeof(int), 1, file);
		if (Read == 0)
			return false;

		vector<MTD_SpyItems> spyItems = {};
		MTD_SpyItems item;
		for (int i = 0; i < spyItemCount; ++i)
		{
			Read = zfread(&item, sizeof(MTD_SpyItems), 1, file);
			spyItems.push_back(item);
		}

		pRule->SetSpyItems(spyItems);

	}
	return true;
}

void ConvertCharInfo_v0_to_v2(MTD_CharInfo_v2* v2, MTD_CharInfo_v0* v0)
{
	// 변수 하나 추가된 것이므로 그냥 덮어씌우고 추가된 변수 초기화
	memcpy(v2, v0, sizeof(MTD_CharInfo_v0));
	v2->nClanCLID = 0;
};

void ConvertCharInfo_v2_to_v5(MTD_CharInfo_v5* v5, MTD_CharInfo_v2* v2)
{
	memcpy(v5, v2, sizeof(MTD_CharInfo_v2));
	v5->nDTLastWeekGrade = 0;
}

void ConvertCharInfo_v5_to_curr(MTD_CharInfo* curr, MTD_CharInfo_v5* v5)
{
	// 일단 덮어씌우고
	memcpy(curr, v5, sizeof(MTD_CharInfo_v5));
	// nEquipedItemDesc[MMCIP_END] 에서 MMCIP_END가 변경됐으므로 직접 복사
	memset(curr->nEquipedItemDesc, 0, sizeof(curr->nEquipedItemDesc));
	for (int i=0; i<MMCIP_CUSTOM2+1; ++i)
		curr->nEquipedItemDesc[i] = v5->nEquipedItemDesc[i];
	// 그 뒤의 멤버변수 다시 복사
	curr->nUGradeID		= v5->nUGradeID;
	curr->nClanCLID		= v5->nClanCLID;
	curr->nDTLastWeekGrade	= v5->nDTLastWeekGrade;
	
	// 새로 추가된 변수들은 그냥 기본값으로 세팅
	memset(curr->uidEquipedItem, 0, sizeof(curr->uidEquipedItem));
	
	for (int i=0; i<MMCIP_END; ++i)
		curr->nEquipedItemCount[i] = 1;
}

void ConvertCharInfo_v6_to_curr(MTD_CharInfo* curr, MTD_CharInfo_v6* v6)
{
	// 일단 덮어씌우고
	memcpy(curr, v6, sizeof(MTD_CharInfo_v6));
	// nEquipedItemDesc[MMCIP_END] 에서 MMCIP_END가 변경됐으므로 직접 복사
	memset(curr->nEquipedItemDesc, 0, sizeof(curr->nEquipedItemDesc));
	for (int i = 0; i<MMCIP_CUSTOM3; ++i)
		curr->nEquipedItemDesc[i] = v6->nEquipedItemDesc[i];
	// 그 뒤의 멤버변수 다시 복사
	curr->nUGradeID = v6->nUGradeID;
	curr->nClanCLID = v6->nClanCLID;
	curr->nDTLastWeekGrade = v6->nDTLastWeekGrade;

	// 새로 추가된 변수들은 그냥 기본값으로 세팅
	memset(curr->uidEquipedItem, 0, sizeof(curr->uidEquipedItem));

	for (int i = 0; i<MMCIP_END; ++i)
		curr->nEquipedItemCount[i] = 1;
}

void ConvertCharInfo(MTD_CharInfo* currCharInfo, void* oldCharInfo, int nVerOld)
{
	MTD_CharInfo_v2 v2;
	MTD_CharInfo_v5 v5;

	switch (nVerOld)
	{
	case 0: case 1:
		ConvertCharInfo_v0_to_v2(&v2, (MTD_CharInfo_v0*)oldCharInfo);
		oldCharInfo = &v2;

	case 2: case 3: case 4:
		ConvertCharInfo_v2_to_v5(&v5, (MTD_CharInfo_v2*)oldCharInfo);
		oldCharInfo = &v5;

	case 5:
		ConvertCharInfo_v5_to_curr(currCharInfo, (MTD_CharInfo_v5*)oldCharInfo);
	case 6:
		ConvertCharInfo_v6_to_curr(currCharInfo, (MTD_CharInfo_v6*)oldCharInfo);

	}
	// case에 break가 없는 것은 의도된 것임

	if (nVerOld == GUNZ_REC_FILE_VERSION)
		memcpy(currCharInfo, oldCharInfo, sizeof(MTD_CharInfo));
}


bool ZReplayLoader::LoadCharInfo(ZFile* file)
{
	int nRead;

	// character info
	int nCharacterCount;
	zfread(&nCharacterCount, sizeof(nCharacterCount), 1, file);

	for(int i = 0; i < nCharacterCount; i++)
	{
		bool bHero;
		nRead = zfread(&bHero, sizeof(bHero), 1, file);
		if(nRead != 1) return false;

		MTD_CharInfo info;

		switch (m_nVersion)
		{
		case 0: case 1:
		{
			MTD_CharInfo_v0 old;
			nRead = zfread(&old, sizeof(old), 1, file);
			if (nRead != 1) return false;
			ConvertCharInfo(&info, &old, m_nVersion);
		}
		break;
		case 2: case 3: case 4:
		{
			MTD_CharInfo_v2 old;
			nRead = zfread(&old, sizeof(old), 1, file);
			if (nRead != 1) return false;
			ConvertCharInfo(&info, &old, m_nVersion);
		}
		break;
		case 5:
		{
			MTD_CharInfo_v5 old;
			nRead = zfread(&old, sizeof(old), 1, file);
			if (nRead != 1) return false;
			ConvertCharInfo(&info, &old, m_nVersion);
		}
		break;
		case 6:
		{
			MTD_CharInfo_v6 old;
			nRead = zfread(&old, sizeof(old), 1, file);
			if (nRead != 1)
				return false;
			ConvertCharInfo(&info, &old, m_nVersion);
		}break;
		case GUNZ_REC_FILE_VERSION:
		{
			nRead = zfread(&info, sizeof(info), 1, file);
			if (nRead != 1) return false;
		}
		}

		ZCharacter* pChar = NULL;
		if (bHero)
		{
			ZGetGame()->m_pMyCharacter = new ZMyCharacter(ZGetGame());
			ZGetGame()->CreateMyCharacter(&info/*, NULL*/);			///< TodoH(상) - 리플레이 관련..
			pChar = ZGetGame()->m_pMyCharacter;
			pChar->Load(file, m_nVersion);
		}
		else
		{
			pChar = new ZNetCharacter(ZGetGame());
			pChar->Load(file, m_nVersion);
			pChar->Create(&info/*, NULL*/);							///< TodoH(상) - 리플레이 관련..
		}
		ZGetCharacterManager()->Add(pChar);

		pChar->SetVisible(true);
#ifdef _REPLAY_TEST_LOG
		mlog("[Add Character %s(%d)]\n", pChar->GetUserName());
#endif

	}

	//assign CTF berserker state after we're in the clear

	if (IsGameRuleCTF(m_StageSetting.nGameType))
	{
		ZRuleTeamCTF* pTeamCTF = (ZRuleTeamCTF*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		for (ZCharacterManager::iterator itor = ZGetGame()->m_CharacterManager.begin();
			itor != ZGetGame()->m_CharacterManager.end(); ++itor)
		{
			ZCharacter* pCharacter = (ZCharacter*)(*itor).second;
			if (pCharacter)
			{
				if (pCharacter->GetUID() == pTeamCTF->GetBlueCarrier() || pCharacter->GetUID() == pTeamCTF->GetRedCarrier())
				{
					ZGetEffectManager()->AddBerserkerIcon(pCharacter);
					pCharacter->SetTagger(true);
				}
			}
		}
	}
	if (m_StageSetting.nGameType == MMATCH_GAMETYPE_GUNGAME)
	{
		ZRuleGunGame* pRule = (ZRuleGunGame*)ZGetGame()->GetMatch()->GetRule();
		for (auto itor = ZGetCharacterManager()->begin(); itor != ZGetCharacterManager()->end(); ++itor)
		{
			ZCharacter* pCharacter = (ZCharacter*)(*itor).second;
			GGWeaponSet set = pRule->GetMatchSet().at(pCharacter->GetKils());
			pCharacter->m_Items.EquipItem(MMCIP_MELEE, set.WeaponSet[0]);
			pCharacter->m_Items.EquipItem(MMCIP_PRIMARY, set.WeaponSet[1]);
			pCharacter->m_Items.EquipItem(MMCIP_SECONDARY, set.WeaponSet[2]);

			for (int i = MMCIP_CUSTOM1; i <= MMCIP_CUSTOM3; ++i)
			{
				if (i == MMCIP_CUSTOM1 || i == MMCIP_CUSTOM2 || i == MMCIP_CUSTOM3) {
					pCharacter->m_Items.EquipItem(static_cast<MMatchCharItemParts>(i), 0);
				}
			}
			pCharacter->ChangeWeapon(pCharacter->GetItems()->GetSelectedWeaponParts() == MMCIP_MELEE ? MMCIP_PRIMARY : MMCIP_MELEE);
			unsigned long nItemID[MMCIP_END];
			for (int i = 0; i < MMCIP_END; ++i)
			{
				nItemID[i] = pCharacter->m_Items.GetItem((MMatchCharItemParts)i)->GetDescID();
			}

			ZChangeCharParts(pCharacter->m_pVMesh, pCharacter->GetProperty()->nSex, pCharacter->GetProperty()->nHair, pCharacter->GetProperty()->nFace,
				nItemID);
		}
	}
	if (m_StageSetting.nGameType == MMATCH_GAMETYPE_INFECTION)
	{
		ZRuleInfection* pRule = (ZRuleInfection*)ZGetGame()->GetMatch()->GetRule();
		for (auto itor = ZGetCharacterManager()->begin(); itor != ZGetCharacterManager()->end(); ++itor)
		{
			ZCharacter* pChar = (ZCharacter*)(*itor).second;
			if (pChar->IsInfected())
			{
				if (pChar->GetUID() == pRule->GetFirstInfected())
				{
					pRule->InfectPlayer(pChar->GetUID(), 500, 250, true);
				}
				else
				{
					pRule->InfectPlayer(pChar->GetUID(), 200, 150,false);
				}
			}
		}
	}
	if (m_StageSetting.nGameType == MMATCH_GAMETYPE_SPYMODE)
	{
		ZRuleSpyMode* pRule = dynamic_cast<ZRuleSpyMode*>(ZGetGame()->GetMatch()->GetRule());
		SpyPlayerTable spyTable = MGetSpyMode()->GetSpyTableElement((int)ZGetCharacterManager()->size());
		for (auto itor = ZGetCharacterManager()->begin(); itor != ZGetCharacterManager()->end(); ++itor)
		{
			ZCharacter* pChar = dynamic_cast<ZCharacter*>((*itor).second);
			if (pRule->IsSpy(pChar->GetUID()))
			{
				pChar->SetTeamID(MMT_RED);
				pChar->SetMaxHP(pChar->GetMaxHP() + spyTable.hpap);
				pChar->SetMaxAP(pChar->GetMaxAP() + spyTable.hpap);
				pChar->SetHP(pChar->GetHP() + spyTable.hpap);
				pChar->SetAP(pChar->GetAP() + spyTable.hpap);
			}
		}
		//TODO: equip items
		pRule->SetTeams();

	}
	return true;
}


bool ZReplayLoader::LoadNPCInfo(ZFile* file)
{
	ZRuleQuestChallenge* pRule = dynamic_cast<ZRuleQuestChallenge*>(ZGetGame()->GetMatch()->GetRule());

	// character info
	int npcCount;
	zfread(&npcCount, sizeof(npcCount), 1, file);

	for (int i = 0; i < npcCount; i++)
	{
		//TODO: finish writing!

		for (int i = 0; i < npcCount; ++i)
		{
			ZActorWithFSM* pActor = nullptr;
			MTD_ZActorWithFSMInfo_v0 info;

			int Read = zfread(&info, sizeof(MTD_ZActorWithFSMInfo_v0), 1, file);
			if (Read != 1)
				break;

			pActor = new ZActorWithFSM(ZGetGame(),pRule->GetActorActionMgr());
			pActor->Load(file, info);
			pActor->InitWithActorDef(pRule->GetActorDefMgr()->GetDef(info.actorName),pRule->GetFSMMgr());							///< TodoH(상) - 리플레이 관련..
			pActor->SetHero(false);
			//pActor->SetAnimationInAction(reinterpret_cast<IActorAction*>(pRule->GetActorActionMgr()->GetActionByIndex(info.currAction)));
			pActor->SetVisible(true);
		}
	}

	return true;
}

bool ZReplayLoader::LoadCommandStream(ZFile* file)
{
	float fGameTime;
	zfread(&fGameTime, sizeof(fGameTime), 1, file);
	m_fGameTime = fGameTime;

	int nCommandCount=0;

	int nSize = 0;
	float fTime;
	while( zfread(&fTime, sizeof(fTime), 1, file) )
	{
		nCommandCount++;

		char CommandBuffer[1024];

		MUID uidSender;
		zfread(&uidSender, sizeof(uidSender), 1, file);
		zfread(&nSize, sizeof(int), 1, file);

		if(nSize<0 || nSize>sizeof(CommandBuffer)) {
			mlog("Error, size larger than commandbuffer\n");
			return false;
		}
		zfread(CommandBuffer, nSize, 1, file);


		ZObserverCommandItem *pZCommand=new ZObserverCommandItem;
		pZCommand->pCommand= CreateCommandFromStream(CommandBuffer);
		pZCommand->pCommand->m_Sender=uidSender;
		pZCommand->fTime=fTime;
		ZGetGame()->GetReplayCommandList()->push_back(pZCommand);

	}

	return true;
}


MCommand* ZReplayLoader::CreateCommandFromStream(char* pStream)
{
	if (m_nVersion <= 2)
	{
		return CreateCommandFromStreamVersion2(pStream);
	}

	MCommand* pCommand = new MCommand;
	pCommand->SetData(pStream, ZGetGameClient()->GetCommandManager());
	return pCommand;
}


MCommand* ZReplayLoader::CreateCommandFromStreamVersion2(char* pStream)
{
	MCommandManager* pCM = ZGetGameClient()->GetCommandManager();

	MCommand* pCommand = new MCommand;
	
	BYTE nParamCount = 0;
	unsigned short int nDataCount = 0;

	// Get Total Size
	unsigned short nTotalSize = 0;
	memcpy(&nTotalSize, pStream, sizeof(nTotalSize));
	nDataCount += sizeof(nTotalSize);

	// Command
	unsigned short int nCommandID = 0;
	memcpy(&nCommandID, pStream+nDataCount, sizeof(nCommandID));
	nDataCount += sizeof(nCommandID);

	MCommandDesc* pDesc = pCM->GetCommandDescByID(nCommandID);
	if (pDesc == NULL)
	{
		mlog("Error(MCommand::SetData): Wrong Command ID(%d)\n", nCommandID);
		_ASSERT(0);

		return pCommand;
	}
	pCommand->SetID(pDesc);

	if (ParseVersion2Command(pStream+nDataCount, pCommand))
	{
		return pCommand;
	}

	// Parameters
	memcpy(&nParamCount, pStream+nDataCount, sizeof(nParamCount));
	nDataCount += sizeof(nParamCount);
	for(int i=0; i<nParamCount; i++)
	{
		BYTE nType;
		memcpy(&nType, pStream+nDataCount, sizeof(BYTE));
		nDataCount += sizeof(BYTE);

		MCommandParameter* pParam = MakeVersion2CommandParameter((MCommandParameterType)nType, pStream, &nDataCount);
		if (pParam == NULL) return false;
		
		pCommand->m_Params.push_back(pParam);
	}

	return pCommand;
}

bool ZReplayLoader::ParseVersion2Command(char* pStream, MCommand* pCmd)
{
	switch (pCmd->GetID())
	{
	case MC_PEER_HPINFO:
	case MC_PEER_HPAPINFO:
	case MC_MATCH_OBJECT_CACHE:
	case MC_MATCH_STAGE_ENTERBATTLE:
	case MC_MATCH_STAGE_LIST:
	case MC_MATCH_CHANNEL_RESPONSE_PLAYER_LIST:
	case MC_MATCH_GAME_RESPONSE_SPAWN:
	case MC_PEER_DASH:
	case MC_MATCH_BRIDGEPEER:
	case MC_MATCH_SPAWN_WORLDITEM:
		{

		}
		break;
	default:
		return false;
	};

	BYTE nParamCount = 0;
	unsigned short int nDataCount = 0;
	vector<MCommandParameter*> TempParams;

	// Count
	memcpy(&nParamCount, pStream+nDataCount, sizeof(nParamCount));
	nDataCount += sizeof(nParamCount);

	for(int i=0; i<nParamCount; i++)
	{
		BYTE nType;
		memcpy(&nType, pStream+nDataCount, sizeof(BYTE));
		nDataCount += sizeof(BYTE);

		MCommandParameter* pParam = MakeVersion2CommandParameter((MCommandParameterType)nType, pStream, &nDataCount);
		if (pParam == NULL) return false;
		
		TempParams.push_back(pParam);
	}


	switch (pCmd->GetID())
	{
	case MC_PEER_HPAPINFO:
		{
			void* pBlob = TempParams[1]->GetPointer();
			struct REPLAY2_HP_AP_INFO 
			{
				MUID muid;
				float fHP;
				float fAP;
			};

			REPLAY2_HP_AP_INFO* pBlobData = (REPLAY2_HP_AP_INFO*)MGetBlobArrayElement(pBlob, 0);
			pCmd->AddParameter(new MCmdParamFloat(pBlobData->fHP));
			pCmd->AddParameter(new MCmdParamFloat(pBlobData->fAP));
		}
		break;
	case MC_PEER_HPINFO:
		{
			void* pBlob = TempParams[1]->GetPointer();
			struct REPLAY2_HP_INFO 
			{
				MUID muid;
				float fHP;
			};

			REPLAY2_HP_INFO* pBlobData = (REPLAY2_HP_INFO*)MGetBlobArrayElement(pBlob, 0);
			pCmd->AddParameter(new MCmdParamFloat(pBlobData->fHP));
		}
		break;
	case MC_MATCH_OBJECT_CACHE:
		{
			unsigned int nType;
			TempParams[0]->GetValue(&nType);
			MCmdParamBlob* pBlobParam = ((MCmdParamBlob*)TempParams[1])->Clone();

			pCmd->AddParameter(new MCmdParamUChar((unsigned char)nType));
			pCmd->AddParameter(pBlobParam);
		}
		break;
	case MC_MATCH_STAGE_ENTERBATTLE:
		{
			MUID uidPlayer, uidStage;
			int nParam;
			
			TempParams[0]->GetValue(&uidPlayer);
			TempParams[1]->GetValue(&uidStage);
			TempParams[2]->GetValue(&nParam);

			struct REPLAY2_ExtendInfo
			{
				char			nTeam;
				unsigned char	nPlayerFlags;
				unsigned char	nReserved1;
				unsigned char	nReserved2;
			};

			struct REPLAY2_PeerListNode
			{
				MUID				uidChar;
				char				szIP[64];
				unsigned int		nPort;
				MTD_CharInfo		CharInfo;
				REPLAY2_ExtendInfo	ExtendInfo;
			};


			void* pBlob = TempParams[3]->GetPointer();
			//int nCount = MGetBlobArrayCount(pBlob);
			REPLAY2_PeerListNode* pNode = (REPLAY2_PeerListNode*)MGetBlobArrayElement(pBlob, 0);


			void* pNewBlob = MMakeBlobArray(sizeof(MTD_PeerListNode), 1);
			MTD_PeerListNode* pNewNode = (MTD_PeerListNode*)MGetBlobArrayElement(pNewBlob, 0);
			pNewNode->uidChar = pNode->uidChar;
			pNewNode->dwIP = inet_addr(pNode->szIP);
			pNewNode->nPort = pNode->nPort;
			memcpy(&pNewNode->CharInfo, &pNode->CharInfo, sizeof(MTD_CharInfo));
			pNewNode->ExtendInfo.nTeam = pNode->ExtendInfo.nTeam;
			pNewNode->ExtendInfo.nPlayerFlags = pNode->ExtendInfo.nPlayerFlags;
			pNewNode->ExtendInfo.nReserved1 = pNode->ExtendInfo.nReserved1;
			pNewNode->ExtendInfo.nReserved2 = pNode->ExtendInfo.nReserved1;
			

			pCmd->AddParameter(new MCmdParamUChar((unsigned char)nParam));
			pCmd->AddParameter(new MCommandParameterBlob(pNewBlob, MGetBlobArraySize(pNewBlob)));

			MEraseBlobArray(pNewBlob);
		}
		break;
	case MC_MATCH_STAGE_LIST:
		{
			_ASSERT(0);
		}
		break;
	case MC_MATCH_CHANNEL_RESPONSE_PLAYER_LIST:
		{
			_ASSERT(0);
		}
		break;
	case MC_MATCH_GAME_RESPONSE_SPAWN:
		{
			MUID uidChar;
			rvector pos, dir;

			TempParams[0]->GetValue(&uidChar);
			TempParams[1]->GetValue(&pos);
			TempParams[2]->GetValue(&dir);

			pCmd->AddParameter(new MCmdParamUID(uidChar));
			pCmd->AddParameter(new MCmdParamShortVector(pos.x, pos.y, pos.z));
			pCmd->AddParameter(new MCmdParamShortVector(DirElementToShort(dir.x), DirElementToShort(dir.y), DirElementToShort(dir.z)));
		}
		break;
	case MC_PEER_DASH:
		{
			rvector pos, dir;
			int nSelType;

			TempParams[0]->GetValue(&pos);
			TempParams[1]->GetValue(&dir);
			TempParams[2]->GetValue(&nSelType);

			ZPACKEDDASHINFO pdi;
			pdi.posx = Roundf(pos.x);
			pdi.posy = Roundf(pos.y);
			pdi.posz = Roundf(pos.z);

			pdi.dirx = DirElementToShort(dir.x);
			pdi.diry = DirElementToShort(dir.y);
			pdi.dirz = DirElementToShort(dir.z);

			pdi.seltype = (BYTE)nSelType;

			pCmd->AddParameter(new MCommandParameterBlob(&pdi,sizeof(ZPACKEDDASHINFO)));
		}
		break;
	case MC_MATCH_SPAWN_WORLDITEM:
		{
			struct REPLAY2_WorldItem
			{
				unsigned short	nUID;
				unsigned short	nItemID;
				unsigned short  nItemSubType;
				float			x;
				float			y;
				float			z;
			};


			void* pBlob = TempParams[0]->GetPointer();
			int nCount = MGetBlobArrayCount(pBlob);

			void* pNewBlob = MMakeBlobArray(sizeof(MTD_WorldItem), nCount);

			for (int i = 0; i < nCount; i++)
			{
				REPLAY2_WorldItem* pNode = (REPLAY2_WorldItem*)MGetBlobArrayElement(pBlob, i);
				MTD_WorldItem* pNewNode = (MTD_WorldItem*)MGetBlobArrayElement(pNewBlob, i);

				pNewNode->nUID = pNode->nUID;
				pNewNode->nItemID = pNode->nItemID;
				pNewNode->nItemSubType = pNode->nItemSubType;
				pNewNode->x = (short)Roundf(pNode->x);
				pNewNode->y = (short)Roundf(pNode->y);
				pNewNode->z = (short)Roundf(pNode->z);
			}
			pCmd->AddParameter(new MCommandParameterBlob(pNewBlob, MGetBlobArraySize(pNewBlob)));
			MEraseBlobArray(pNewBlob);

		}
		break;
	//case MC_NEWQUEST_NPC_SPAWN:
	//{
	//	ZRuleQuestChallenge* pRule = dynamic_cast<ZRuleQuestChallenge*>(ZGetGame()->GetMatch()->GetRule());
	//	if (pRule)
	//	{
	//		ZAct
	//	}
	//}break;
	case MC_MATCH_BRIDGEPEER:
		{
			_ASSERT(0);
		}
		break;
	};


	for(int i=0; i<(int)TempParams.size(); i++){
		delete TempParams[i];
	}
	TempParams.clear();


	return true;
}


MCommandParameter* ZReplayLoader::MakeVersion2CommandParameter(MCommandParameterType nType, char* pStream, unsigned short int* pnDataCount)
{
	MCommandParameter* pParam = NULL;

	switch(nType) 
	{
	case MPT_INT:
		pParam = new MCommandParameterInt;
		break;
	case MPT_UINT:
		pParam = new MCommandParameterUInt;
		break;
	case MPT_FLOAT:
		pParam = new MCommandParameterFloat;
		break;
	case MPT_STR:
		{
			pParam = new MCommandParameterString;
			MCommandParameterString* pStringParam = (MCommandParameterString*)pParam;

			char* pStreamData = pStream+ *pnDataCount;

			int nValueSize = 0;
			memcpy(&nValueSize, pStreamData, sizeof(nValueSize));
			pStringParam->m_Value = new char[nValueSize];
			memcpy(pStringParam->m_Value, pStreamData+sizeof(nValueSize), nValueSize);
			int nParamSize = nValueSize+sizeof(nValueSize);

			*pnDataCount += nParamSize;
			return pParam;
		}
		break;
	case MPT_VECTOR:
		pParam = new MCommandParameterVector;
		break;
	case MPT_POS:
		pParam = new MCommandParameterPos;
		break;
	case MPT_DIR:
		pParam = new MCommandParameterDir;
		break;
	case MPT_BOOL:
		pParam = new MCommandParameterBool;
		break;
	case MPT_COLOR:
		pParam = new MCommandParameterColor;
		break;
	case MPT_UID:
		pParam = new MCommandParameterUID;
		break;
	case MPT_BLOB:
		pParam = new MCommandParameterBlob;
		break;
	case MPT_CHAR:
		pParam = new MCommandParameterChar;
		break;
	case MPT_UCHAR:
		pParam = new MCommandParameterUChar;
		break;
	case MPT_SHORT:
		pParam = new MCommandParameterShort;
		break;
	case MPT_USHORT:
		pParam = new MCommandParameterUShort;
		break;
	case MPT_INT64:
		pParam = new MCommandParameterInt64;
		break;
	case MPT_UINT64:
		pParam = new MCommandParameterUInt64;
		break;
	default:
		mlog("Error(MCommand::SetData): Wrong Param Type\n");
		_ASSERT(false);		// Unknow Parameter!!!
		return NULL;
	}

	*pnDataCount += pParam->SetData(pStream+ *pnDataCount);

	return pParam;
}