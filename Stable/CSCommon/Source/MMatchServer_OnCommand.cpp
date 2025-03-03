#include "stdafx.h"
#include "MMatchServer.h"
#include "MSharedCommandTable.h"
#include "MErrorTable.h"
#include "MBlobArray.h"
#include "MObject.h"
#include "MMatchObject.h"
#include "MAgentObject.h"
#include "MMatchNotify.h"
#include "Msg.h"
#include "MMatchStage.h"
#include "MCommandCommunicator.h"
#include "MMatchTransDataType.h"
#include "MDebug.h"
#include "MMatchStatus.h"
#include "MMatchSchedule.h"
#include "MTypes.h"
#include "MMatchConfig.h"
#include "AntiSql.h"

#define _STATUS_CMD_START	unsigned long int nStatusStartTime = timeGetTime();
#define _STATUS_CMD_END		MGetServerStatusSingleton()->AddCmd(pCommand->GetID(), pCommand->GetSenderUID(), 0, timeGetTime()-nStatusStartTime);

bool MMatchServer::OnCommand(MCommand* pCommand)
{
	// ???? ??? Agent ?? ?????�??? ??????? ?�??? ???
	if(GetAgent(pCommand->GetSenderUID())==NULL && pCommand->GetSenderUID()!=m_This)
	{
		bool bFloodingSuspect = false;
		m_objectCommandHistory.PushCommand( pCommand->GetSenderUID(), pCommand->GetID(), GetGlobalClockCount(), &bFloodingSuspect);
		if(bFloodingSuspect)
		{
			if (MGetServerConfig()->IsUseBlockFlooding())
			{
				MMatchObject* pObj = GetObject( pCommand->GetSenderUID() );
				if( pObj && pObj->GetDisconnStatusInfo().GetStatus() == MMDS_CONNECTED)
				{
					LOG(LOG_FILE,"Command Flood detected");
					LogObjectCommandHistory( pCommand->GetSenderUID() );
					pObj->DisconnectHacker( MMHT_COMMAND_FLOODING );
				}
				else
				{
					LOG(LOG_FILE,"Command(%d) Flood detected, but i can't find matchobject", pCommand->GetID());
				}

				// ????? ??? �????????? �??
				return true;
			}
		}
	}

	MGetServerStatusSingleton()->AddCmd(pCommand->GetID(), pCommand->GetSenderUID());

	if (MServer::OnCommand(pCommand) == true)
		return true;

 _STATUS_CMD_START;

	switch(pCommand->GetID())
	{
		case MC_MATCH_LOGIN:
			{
				char szUserID[ MAX_USERID_STRING_LEN ];
				char szPassword[ MAX_USER_PASSWORD_STRING_LEN ];
				int nCommandVersion = 0;

				if (pCommand->GetParameter(szUserID, 0, MPT_STR, MAX_USERID_STRING_LEN )==false) break;
				if (pCommand->GetParameter(szPassword, 1, MPT_STR, MAX_USER_PASSWORD_STRING_LEN )==false) break;
				if (pCommand->GetParameter(&nCommandVersion, 2, MPT_INT)==false) break;
				MCommandParameter* pLoginParam = pCommand->GetParameter(3);
				if (pLoginParam->GetType() != MPT_BLOB) break;
				void *pLoginBlob = pLoginParam->GetPointer();
				if( NULL == pLoginBlob )
				{
					// Hacker�� Blob�� ũ�⸦ �����ϸ� MCommand�� ���鶧 Blob�����Ͱ� NULL�����͸� ������ �ִ�.
					break;
				}
				char *szEncryptMD5Value = (char *)MGetBlobArrayElement(pLoginBlob, 0);
				
				OnMatchLogin(pCommand->GetSenderUID(), szUserID, szPassword, nCommandVersion, szEncryptMD5Value);
			}
			break;
		case MC_MATCH_LOGIN_NETMARBLE_JP:
			{
				char szLoginID[ MAX_USERID_STRING_LEN ];
				char szLoginPW[ MAX_USER_PASSWORD_STRING_LEN ];
				int nCommandVersion = 0;
				unsigned long nChecksumPack = 0;
				if (pCommand->GetParameter(szLoginID,		0, MPT_STR, MAX_USERID_STRING_LEN )==false) break;
				if (pCommand->GetParameter(szLoginPW,		1, MPT_STR, MAX_USER_PASSWORD_STRING_LEN )==false) break;
				if (pCommand->GetParameter(&nCommandVersion,2, MPT_INT)==false) break;
				if (pCommand->GetParameter(&nChecksumPack,	3, MPT_UINT)==false) break;

				OnMatchLoginFromNetmarbleJP(pCommand->GetSenderUID(), 
										szLoginID, szLoginPW, nCommandVersion, nChecksumPack);
			}
			break;
		case MC_MATCH_LOGIN_FROM_DBAGENT:
			{
				MUID CommUID;
				char szLoginID[256];
				char szName[256];
				int nSex;
				bool bFreeLoginIP;
				unsigned long nChecksumPack;

				if (pCommand->GetParameter(&CommUID,		0, MPT_UID)==false) break;
				if (pCommand->GetParameter(szLoginID,		1, MPT_STR, sizeof(szLoginID) )==false) break;
				if (pCommand->GetParameter(szName,			2, MPT_STR, sizeof(szName) )==false) break;
				if (pCommand->GetParameter(&nSex,			3, MPT_INT)==false) break;
				if (pCommand->GetParameter(&bFreeLoginIP,	4, MPT_BOOL)==false) break;
				if (pCommand->GetParameter(&nChecksumPack,	5, MPT_UINT)==false) break;

				OnMatchLoginFromDBAgent(CommUID, szLoginID, szName, nSex, bFreeLoginIP, nChecksumPack);
			}
			break;
		case MC_MATCH_LOGIN_FROM_DBAGENT_FAILED:
			{
				MUID CommUID;
				int nResult;

				if (pCommand->GetParameter(&CommUID,	0, MPT_UID)==false) break;
				if (pCommand->GetParameter(&nResult,	1, MPT_INT)==false) break;

				OnMatchLoginFailedFromDBAgent(CommUID, nResult);
			}
			break;

		case MC_MATCH_BRIDGEPEER:
			{
				MUID uidChar;
				DWORD dwIP, nPort;

				pCommand->GetParameter(&uidChar,	0, MPT_UID);
				pCommand->GetParameter(&dwIP,		1, MPT_UINT);
				pCommand->GetParameter(&nPort,		2, MPT_UINT);
				OnBridgePeer(uidChar, dwIP, nPort);
			}
			break;
		case MC_DEBUG_TEST:
			{
				DebugTest();
			}
			break;
		case MC_MATCH_REQUEST_RECOMMANDED_CHANNEL:
			{
				OnRequestRecommendedChannel(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_CHANNEL_REQUEST_JOIN:
			{
				MUID uidPlayer, uidChannel;

				uidPlayer = pCommand->GetSenderUID();
				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidChannel, 1, MPT_UID);

				OnRequestChannelJoin(uidPlayer, uidChannel);
			}
			break;
		case MC_MATCH_CHANNEL_REQUEST_JOIN_FROM_NAME:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nChannelType;
				char szChannelName[CHANNELNAME_LEN];

				//pCommand->GetParameter(&uidPlayer,		0, MPT_UID);
				pCommand->GetParameter(&nChannelType,	1, MPT_INT);
				pCommand->GetParameter(szChannelName,	2, MPT_STR, CHANNELNAME_LEN );

				OnRequestChannelJoin(uidPlayer, MCHANNEL_TYPE(nChannelType), szChannelName);
			}
			break;
		case MC_MATCH_CHANNEL_LIST_START:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nChannelType;

				//pCommand->GetParameter(&uidPlayer,		0, MPT_UID);
				pCommand->GetParameter(&nChannelType,	1, MPT_INT);

				OnStartChannelList(uidPlayer, nChannelType);
			}
			break;
		case MC_MATCH_CHANNEL_LIST_STOP:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				//pCommand->GetParameter(&uidPlayer,		0, MPT_UID);

				OnStopChannelList(uidPlayer);
			}
			break;
		case MC_MATCH_CHANNEL_REQUEST_CHAT:
			{
				MUID uidSender, uidPlayer, uidChannel;
				static char szChat[ CHAT_STRING_LEN ];
				// static char szChat[1024];)
				
				///< ?????(2009.08.04)
				uidSender = pCommand->GetSenderUID(); 
				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				
				///< �?? ??????? ?????? ???, �?? ??? g?�?? ??? ??? ??? ????!
				if( uidSender != uidPlayer )
				{
					//LOG(LOG_FILE,"In MC_MATCH_CHANNEL_REQUEST_CHAT - Different User(S:%d, P:%d)", uidSender, uidPlayer);
					break;
				}
				///<???????....

				pCommand->GetParameter(&uidChannel, 1, MPT_UID);
				pCommand->GetParameter(szChat, 2, MPT_STR, CHAT_STRING_LEN );

				OnChannelChat(uidSender, uidChannel, szChat);
			}
			break;
		case MC_MATCH_STAGE_TEAM:
			{
				MUID uidPlayer, uidStage;
				MMatchTeam nTeam;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				pCommand->GetParameter(&nTeam, 2, MPT_UINT);

				OnStageTeam(uidPlayer, uidStage, nTeam);				
			}
			break;
		case MC_MATCH_STAGE_PLAYER_STATE:
			{
				MUID uidPlayer, uidStage;
				int nStageState;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				pCommand->GetParameter(&nStageState, 2, MPT_INT);


				OnStagePlayerState(uidPlayer, uidStage, MMatchObjectStageState(nStageState));				

			}
			break;
		case MC_MATCH_STAGE_CREATE:
			{
				MUID uidChar = pCommand->GetSenderUID();
				// char szStageName[128], szStagePassword[128];
				char szStageName[ STAGENAME_LENGTH ] = ""; 
				char szStagePassword[ STAGEPASSWD_LENGTH ] = "";
				bool bPrivate;
				vector<int> stageMods;

				// pCommand->GetParameter(&uidChar, 0, MPT_UID);
				pCommand->GetParameter(szStageName, 1, MPT_STR, STAGENAME_LENGTH );
				pCommand->GetParameter(&bPrivate, 2, MPT_BOOL);
				pCommand->GetParameter(szStagePassword, 3, MPT_STR, STAGEPASSWD_LENGTH );

				MCommandParameter* stageModBlob = pCommand->GetParameter(4);
				if (stageModBlob->GetType() != MPT_BLOB)
					break;

				void* data = stageModBlob->GetPointer();
				int count = MGetBlobArrayCount(data);
				for (int i = 0; i < count; ++i) {
					int mod = *(int*)MGetBlobArrayElement(data, i);
					stageMods.push_back(mod);
				}

				OnStageCreate(uidChar, szStageName, bPrivate, szStagePassword,stageMods);
			}
			break;
		
		case MC_MATCH_REQUEST_PRIVATE_STAGE_JOIN:
			{
				MUID uidPlayer, uidStage;
				// char szPassword[256];
				char szPassword[ STAGEPASSWD_LENGTH ] = "";

				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				pCommand->GetParameter(szPassword, 2, MPT_STR, STAGEPASSWD_LENGTH );

				OnPrivateStageJoin(uidPlayer, uidStage, szPassword);
			}
			break;
		
		case MC_MATCH_STAGE_FOLLOW:
			{
				// char szTargetName[MATCHOBJECT_NAME_LENGTH+1];
				// pCommand->GetParameter(szTargetName, 0, MPT_STR, MATCHOBJECT_NAME_LENGTH+1 );
				char szTargetName[ MAX_CHARNAME_LENGTH ];
				pCommand->GetParameter(szTargetName, 0, MPT_STR, MAX_CHARNAME_LENGTH );

				OnStageFollow(pCommand->GetSenderUID(), szTargetName);
			}
			break;
		case MC_MATCH_STAGE_LEAVE:
			{
				// MUID uidPlayer, uidStage;
				// uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				// pCommand->GetParameter(&uidStage, 1, MPT_UID);

				OnStageLeave(pCommand->GetSenderUID());//, uidStage);
			}
			break;
		case MC_MATCH_STAGE_REQUEST_PLAYERLIST:
			{
				MUID uidStage;
				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				OnStageRequestPlayerList(pCommand->GetSenderUID(), uidStage);
			}
			break;
		case MC_MATCH_STAGE_REQUEST_ENTERBATTLE:
			{
				MUID uidPlayer, uidStage;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				OnStageEnterBattle(uidPlayer, uidStage);
			}
			break;
		case MC_MATCH_STAGE_LEAVEBATTLE_TO_SERVER:
			{
				// MUID uidPlayer, uidStage;
				// uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				// pCommand->GetParameter(&uidStage, 1, MPT_UID);
				bool bGameFinishLeaveBattle;
				pCommand->GetParameter(&bGameFinishLeaveBattle, 1, MPT_BOOL);
				OnStageLeaveBattle(pCommand->GetSenderUID(), bGameFinishLeaveBattle);//, uidStage);
			}
			break;
		case MC_MATCH_STAGE_START:
			{
				MUID uidPlayer, uidStage;
				int nCountdown;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				pCommand->GetParameter(&nCountdown, 2, MPT_INT);

				OnStageStart(uidPlayer, uidStage, nCountdown);
			}
			break;
		case MC_MATCH_STAGE_START_SPY:
		{
			MUID uidPlayer, uidStage;
			pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
			pCommand->GetParameter(&uidStage, 1, MPT_UID);
			OnStageStartSpy(uidPlayer, uidStage);
		}break;
		case MC_MATCH_GAME_ROUNDSTATE:
			{
				MUID uidStage;
				int nState, nRound;

				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				pCommand->GetParameter(&nState, 1, MPT_INT);
				pCommand->GetParameter(&nRound, 2, MPT_INT);

				OnGameRoundState(uidStage, nState, nRound);
			}
			break;

		case MC_MATCH_GAME_REQUEST_SPAWN:
			{
				//MUID uidChar;
				MVector pos, dir;

				//pCommand->GetParameter(&uidChar, 0, MPT_UID);
				pCommand->GetParameter(&pos, 1, MPT_POS);
				pCommand->GetParameter(&dir, 2, MPT_DIR);

				if(_isnan(pos.x) || _isnan(pos.y) || _isnan(pos.z) || _isnan(dir.x) || _isnan(dir.y) || _isnan(dir.z) ||
				   !_finite(pos.x) || !_finite(pos.y) || !_finite(pos.z) || !_finite(dir.x) || !_finite(dir.y) || !_finite(dir.z))
					break;

				OnRequestSpawn(pCommand->GetSenderUID(), pos, dir);
			}
			break;
		case MC_MATCH_GAME_REQUEST_TIMESYNC:
			{
				unsigned int nLocalTimeStamp;
				pCommand->GetParameter(&nLocalTimeStamp, 0, MPT_UINT);

				OnGameRequestTimeSync(pCommand->GetSenderUID(), nLocalTimeStamp);
			}
			break;
		case MC_MATCH_GAME_REPORT_TIMESYNC:
			{
				unsigned int nLocalTimeStamp, nDataChecksum;
				pCommand->GetParameter(&nLocalTimeStamp, 0, MPT_UINT);
				pCommand->GetParameter(&nDataChecksum, 1, MPT_UINT);

				OnGameReportTimeSync(pCommand->GetSenderUID(), nLocalTimeStamp, nDataChecksum);
			}
			break;
		case MC_MATCH_STAGE_CHAT:
			{
				MUID uidPlayer, uidSender, uidStage;
				// static char szChat[512];
				static char szChat[CHAT_STRING_LEN];

				///< ?????(2009.08.04)
				uidSender = pCommand->GetSenderUID(); 
				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);

				///< �?? ??????? ?????? ???, �?? ??? g?�?? ??? ??? ??? ????!				
				if( uidSender != uidPlayer )
				{
					//LOG(LOG_FILE,"In MC_MATCH_STAGE_CHAT - Different User(S:%d, P:%d)", uidSender, uidPlayer);
					break;
				}
				///< ???????

				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				pCommand->GetParameter(szChat, 2, MPT_STR, CHAT_STRING_LEN );

				OnStageChat(uidPlayer, uidStage, szChat);
			}
			break;
		case MC_MATCH_STAGE_REQUEST_QUICKJOIN:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				MCommandParameter* pQuickJoinParam = pCommand->GetParameter(1);
				if(pQuickJoinParam->GetType()!=MPT_BLOB) break;

				void* pQuickJoinBlob = pQuickJoinParam->GetPointer();
				if( NULL == pQuickJoinBlob )
					break;

				OnRequestQuickJoin(uidPlayer, pQuickJoinBlob);

			}
			break;
		case MC_MATCH_STAGE_GO:
			{
				unsigned int nRoomNo;

				pCommand->GetParameter(&nRoomNo, 0, MPT_UINT);
				OnStageGo(pCommand->GetSenderUID(), nRoomNo);
			}
			break;
		case MC_MATCH_STAGE_LIST_START:
			{
				OnStartStageList(pCommand->GetSenderUID());

			}
			break;
		case MC_MATCH_STAGE_LIST_STOP:
			{
				OnStopStageList(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_REQUEST_STAGE_LIST:
			{
				MUID uidPlayer, uidChannel;
				int nStageCursor;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidChannel, 1, MPT_UID);
				pCommand->GetParameter(&nStageCursor, 2, MPT_INT);

				OnStageRequestStageList(uidPlayer, uidChannel, nStageCursor);
			}
			break;
		case MC_MATCH_CHANNEL_REQUEST_PLAYER_LIST:
			{
				MUID uidPlayer, uidChannel;
				int nPage;

				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidChannel, 1, MPT_UID);
				pCommand->GetParameter(&nPage, 2, MPT_INT);

				OnChannelRequestPlayerList(uidPlayer, uidChannel, nPage);
			}
			break;
		case MC_MATCH_STAGE_MAP:
			{
				MUID uidStage;
				// U?????T???? ?? ????? ?????? ??????? _MAX_DIR?? ??? ????. - by SungE 2007-04-02
				char szMapName[ MAPNAME_LENGTH ] = {0,};

				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				pCommand->GetParameter(szMapName, 1, MPT_STR, MAPNAME_LENGTH );

				OnStageMap(uidStage, szMapName, pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_STAGE_RELAY_MAP_ELEMENT_UPDATE:
			{
				MUID uidStage;
				int nType = 0;
				int nRepeatCount = 0;

				pCommand->GetParameter(&uidStage, 0, MPT_UID );
				pCommand->GetParameter(&nType, 1, MPT_INT );
				pCommand->GetParameter(&nRepeatCount, 2, MPT_INT );

				if (FindStage(uidStage) == NULL)
				{
					MMatchObject* pObj = GetObject(pCommand->GetSenderUID());
					if (pObj && pObj->GetDisconnStatusInfo().GetStatus() == MMDS_CONNECTED)
					{
						if (pObj->GetAccountName())
						{
							LOG(LOG_PROG, "Stage packet hack: %s", pObj->GetAccountName());
							break;
						}
					}
				}

				OnStageRelayMapElementUpdate(uidStage, nType, nRepeatCount);
			}
			break;
		case MC_MATCH_STAGE_RELAY_MAP_INFO_UPDATE:
			{
				MUID uidStage;
				int nType = 0;
				int nRepeatCount = 0;
				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				pCommand->GetParameter(&nType, 1, MPT_INT );
				pCommand->GetParameter(&nRepeatCount, 2, MPT_INT );
				MCommandParameter* pParam = pCommand->GetParameter(3);
				if (pParam->GetType() != MPT_BLOB)
					break;
				void* pRelayMapListBlob = pParam->GetPointer();
				if( NULL == pRelayMapListBlob )
					break;

				OnStageRelayMapListUpdate(uidStage, nType, nRepeatCount, pRelayMapListBlob);
			}
			break;
		case MC_MATCH_STAGESETTING:
			{
				MUID uidPlayer, uidStage;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);

				MCommandParameter* pStageParam = pCommand->GetParameter(2);
				if(pStageParam->GetType()!=MPT_BLOB) break;
				void* pStageBlob = pStageParam->GetPointer();
				if( NULL == pStageBlob )
					break;

				int nStageCount = MGetBlobArrayCount(pStageBlob);

				OnStageSetting(uidPlayer, uidStage, pStageBlob, nStageCount);
			}
			break;
		case MC_MATCH_REQUEST_STAGESETTING:
			{
				MUID uidStage;
				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				OnRequestStageSetting(pCommand->GetSenderUID(), uidStage);
			}
			break;
		case MC_MATCH_REQUEST_PEERLIST:
			{
				MUID uidStage;
				// pCommand->GetParameter(&uidChar, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				OnRequestPeerList(pCommand->GetSenderUID(), uidStage);
			}
			break;
		case MC_MATCH_REQUEST_GAME_INFO:
			{
				MUID uidStage;
				// pCommand->GetParameter(&uidChar, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				OnRequestGameInfo(pCommand->GetSenderUID(), uidStage);
			}
			break;
		case MC_MATCH_LOADING_COMPLETE:
			{
				
				int nPercent;
				// pCommand->GetParameter(&uidChar, 0, MPT_UID);
				pCommand->GetParameter(&nPercent, 1, MPT_INT);
				OnMatchLoadingComplete(pCommand->GetSenderUID(), nPercent);
			}
			break;
		case MC_MATCH_REQUEST_PEER_RELAY:
			{
				MUID  uidPeer;

				// if (pCommand->GetParameter(&uidChar, 0, MPT_UID) == false) break;
				if (pCommand->GetParameter(&uidPeer, 1, MPT_UID) == false) break;

				OnRequestRelayPeer(pCommand->GetSenderUID(), uidPeer);
			}
			break;
		case MC_AGENT_PEER_READY:
			{
				MUID uidChar;
				MUID uidPeer;

				if (pCommand->GetParameter(&uidChar, 0, MPT_UID) == false) break;
				if (pCommand->GetParameter(&uidPeer, 1, MPT_UID) == false) break;

				OnPeerReady(uidChar, uidPeer);
			}
			break;
		case MC_MATCH_REGISTERAGENT:
			{
				char szIP[16];
				int nTCPPort, nUDPPort;

				if (pCommand->GetParameter(&szIP, 0, MPT_STR, sizeof(szIP) ) == false) break;
				if (pCommand->GetParameter(&nTCPPort, 1, MPT_INT) == false) break;
				if (pCommand->GetParameter(&nUDPPort, 2, MPT_INT) == false) break;

				OnRegisterAgent(pCommand->GetSenderUID(), szIP, nTCPPort, nUDPPort);
			}
			break;
		case MC_MATCH_UNREGISTERAGENT:
			{
				OnUnRegisterAgent(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_AGENT_REQUEST_LIVECHECK:
			{
				unsigned long nTimeStamp;
				unsigned long nStageCount;
				unsigned long nUserCount;

				if (pCommand->GetParameter(&nTimeStamp, 0, MPT_UINT) == false) break;
				if (pCommand->GetParameter(&nStageCount, 1, MPT_UINT) == false) break;
				if (pCommand->GetParameter(&nUserCount, 2, MPT_UINT) == false) break;

				OnRequestLiveCheck(pCommand->GetSenderUID(), nTimeStamp, nStageCount, nUserCount);
			}
			break;
		case MC_AGENT_STAGE_READY:
			{
				MUID uidStage;
				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				OnAgentStageReady(pCommand->GetSenderUID(), uidStage);
			}
			break;

		case MC_MATCH_REQUEST_ACCOUNT_CHARINFO:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				char nCharNum;
				pCommand->GetParameter(&nCharNum, 0, MPT_CHAR);


				OnRequestAccountCharInfo(uidPlayer, nCharNum);
			}
			break;
		case MC_MATCH_REQUEST_SELECT_CHAR:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				unsigned long int nCharIndex;

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nCharIndex, 1, MPT_UINT);

				OnRequestSelectChar(uidPlayer, nCharIndex);
			}
			break;
		case MC_MATCH_REQUEST_DELETE_CHAR:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				unsigned long int nCharIndex;
				// char szCharName[MAX_CHARNAME];
				char szCharName[ MAX_CHARNAME_LENGTH ];

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nCharIndex, 1, MPT_UINT);
				pCommand->GetParameter(szCharName, 2, MPT_STR, MAX_CHARNAME_LENGTH );

				if(SqlCheckString(szCharName)) break;

				if(StringCheckFormat(szCharName)) break;

				OnRequestDeleteChar(uidPlayer, nCharIndex, szCharName);
			}
			break;
		case MC_MATCH_REQUEST_CREATE_CHAR:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				unsigned long int nCharIndex;
				unsigned long int nSex, nHair, nFace, nCostume;

				// char szCharName[MAX_CHARNAME];
				char szCharName[ MAX_CHARNAME_LENGTH ];

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nCharIndex, 1, MPT_UINT);
				if (pCommand->GetParameter(szCharName, 2, MPT_STR, MAX_CHARNAME_LENGTH )==false) break;
				pCommand->GetParameter(&nSex, 3, MPT_UINT);
				pCommand->GetParameter(&nHair, 4, MPT_UINT);
				pCommand->GetParameter(&nFace, 5, MPT_UINT);
				pCommand->GetParameter(&nCostume, 6, MPT_UINT);

				if(SqlCheckString(szCharName)) break;
				if(StringCheckName(szCharName))
				{
					MMatchObject *pObj = GetObjectA(pCommand->m_Sender);
					if(!pObj) break;
					mlog("Someone tried to create a character name with a color in it. %s - %s", pObj->GetIPString(), pObj->GetAccountName());
					break;
				}
				if(StringCheckFormat(szCharName))
				{
					MMatchObject *pObj = GetObjectA(pCommand->m_Sender);
					if(!pObj) break;
					LOG(7, "Someone tried to corrupt the stack: %s - %s", pObj->GetIPString(), pObj->GetAccountName());
					break;
				}

				OnRequestCreateChar(uidPlayer, nCharIndex, szCharName, nSex, nHair, nFace, nCostume);
			}
			break;
		case MC_MATCH_ROUND_FINISHINFO:
			{
				MUID uidStage, uidChar;

				pCommand->GetParameter(&uidStage, 0, MPT_UID);
				pCommand->GetParameter(&uidChar, 1, MPT_UID);
				OnUpdateFinishedRound(uidStage, uidChar, NULL, NULL);
			}
			break;
		

		case MC_MATCH_STAGE_REQUEST_FORCED_ENTRY:
			{
				MUID uidPlayer, uidStage;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&uidStage, 1, MPT_UID);
				
				OnRequestForcedEntry(uidStage, uidPlayer);
			}
			break;
		case MC_MATCH_FRIEND_ADD:
			{
				// char szArg[MAX_CHARNAME];
				char szArg[ MAX_CHARNAME_LENGTH ];
				pCommand->GetParameter(szArg, 0, MPT_STR, MAX_CHARNAME_LENGTH );

				OnFriendAdd(pCommand->GetSenderUID(), szArg);
			}
			break;
		case MC_MATCH_FRIEND_REMOVE:
			{
				// char szArg[MAX_CHARNAME];
				char szArg[ MAX_CHARNAME_LENGTH ];
				pCommand->GetParameter(szArg, 0, MPT_STR, MAX_CHARNAME_LENGTH );

				OnFriendRemove(pCommand->GetSenderUID(), szArg);
			}
			break;
		case MC_MATCH_FRIEND_LIST:
			{
				OnFriendList(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_FRIEND_MSG:
			{
				char szArg[CHAT_STRING_LEN];
				pCommand->GetParameter(szArg, 0, MPT_STR, CHAT_STRING_LEN );

				OnFriendMsg(pCommand->GetSenderUID(), szArg);
			}
			break;
		case MC_ADMIN_ANNOUNCE:
			{
				MUID uidAdmin;
				static char szChat[ ANNOUNCE_STRING_LEN ];
				unsigned long int nMsgType = 0;

				pCommand->GetParameter(&uidAdmin, 0, MPT_UID);
				pCommand->GetParameter(szChat, 1, MPT_STR, ANNOUNCE_STRING_LEN );
				pCommand->GetParameter(&nMsgType, 2, MPT_UINT);

				// OnAdminAnnounce(uidAdmin, szChat, nMsgType);
				OnAdminAnnounce(pCommand->GetSenderUID(), szChat, nMsgType);
			}
			break;
		case MC_ADMIN_TERMINAL:
			{
			    MUID uidAdmin;
				char szText[1024];
				
				// pCommand->GetParameter(&uidAdmin, 0, MPT_UID);
				pCommand->GetParameter(szText, 1, MPT_STR, sizeof(szText) );

				//OnAdminTerminal(uidAdmin, szText);
				OnAdminTerminal(pCommand->GetSenderUID(), szText);

			}
			break;
		case MC_ADMIN_REQUEST_SERVER_INFO:
			{
				// MUID uidAdmin;
				// pCommand->GetParameter(&uidAdmin, 0, MPT_UID);

				//OnAdminRequestServerInfo(uidAdmin);
				OnAdminRequestServerInfo(pCommand->GetSenderUID());
			}
			break;
		case MC_ADMIN_SERVER_HALT:
			{
				MUID uidAdmin = pCommand->GetSenderUID();
				LOG(LOG_PROG, "Command(MC_ADMIN_SERVER_HALT) Received - Shutdown Start(%d%d)", uidAdmin.High, uidAdmin.Low);
				OnAdminServerHalt(uidAdmin);
			}
			break;
		case MC_ADMIN_REQUEST_KICK_PLAYER:
			{
				char szPlayer[512];
				pCommand->GetParameter(szPlayer, 0, MPT_STR, sizeof(szPlayer) );

				OnAdminRequestKickPlayer(pCommand->GetSenderUID(), szPlayer);
			}
			break;

		case MC_ADMIN_REQUEST_MUTE_PLAYER:
			{
				int nPunishHour;
				char szPlayer[512];

				pCommand->GetParameter(szPlayer, 0, MPT_STR, sizeof(szPlayer) );
				pCommand->GetParameter(&nPunishHour, 1, MPT_INT);

				OnAdminRequestMutePlayer(pCommand->GetSenderUID(), szPlayer, nPunishHour);
			}
			break;

		case MC_ADMIN_REQUEST_BLOCK_PLAYER:
			{
				int nPunishHour;
				char szPlayer[512];

				pCommand->GetParameter(szPlayer, 0, MPT_STR, sizeof(szPlayer) );
				pCommand->GetParameter(&nPunishHour, 1, MPT_INT);

				OnAdminRequestBlockPlayer(pCommand->GetSenderUID(), szPlayer, nPunishHour);
			}
			break;

		case MC_ADMIN_REQUEST_UPDATE_ACCOUNT_UGRADE:
			{
				// MUID uidAdmin;
				char szPlayer[512];

				// pCommand->GetParameter(&uidAdmin, 0, MPT_UID);
				pCommand->GetParameter(szPlayer, 1, MPT_STR, sizeof(szPlayer) );
			
				//OnAdminRequestUpdateAccountUGrade(uidAdmin, szPlayer);
				OnAdminRequestUpdateAccountUGrade(pCommand->GetSenderUID(), szPlayer);
			}
			break;
		case MC_ADMIN_REQUEST_SWITCH_LADDER_GAME:
			{
				// MUID uidAdmin;
				bool bEnabled;

				// pCommand->GetParameter(&uidAdmin, 0, MPT_UID);
				pCommand->GetParameter(&bEnabled, 1, MPT_BOOL);
			
				//OnAdminRequestSwitchLadderGame(uidAdmin, bEnabled);
				OnAdminRequestSwitchLadderGame(pCommand->GetSenderUID(), bEnabled);
			}
			break;
		case MC_ADMIN_HIDE:
			{
				OnAdminHide(pCommand->GetSenderUID());
			}
			break;
		case MC_ADMIN_PING_TO_ALL:
			{
				OnAdminPingToAll(pCommand->GetSenderUID());
			}
			break;
		case MC_ADMIN_RESET_ALL_HACKING_BLOCK :
			{
				OnAdminResetAllHackingBlock( pCommand->GetSenderUID() );
			}
			break;
		case MC_EVENT_CHANGE_MASTER:
			{
				OnEventChangeMaster(pCommand->GetSenderUID());
			}
			break;
		case MC_EVENT_CHANGE_PASSWORD:
			{
				char szPassword[ STAGEPASSWD_LENGTH ]="";
				pCommand->GetParameter(szPassword, 0, MPT_STR, STAGEPASSWD_LENGTH );

				OnEventChangePassword(pCommand->GetSenderUID() ,szPassword);
			}
			break;
		case MC_EVENT_REQUEST_JJANG:
			{
				char szTargetName[128]="";
				pCommand->GetParameter(szTargetName, 0, MPT_STR, sizeof(szTargetName) );
				OnEventRequestJjang(pCommand->GetSenderUID(), szTargetName);
			}
			break;
		case MC_EVENT_REMOVE_JJANG:
			{
				char szTargetName[128]="";
				pCommand->GetParameter(szTargetName, 0, MPT_STR, sizeof(szTargetName) );
				OnEventRemoveJjang(pCommand->GetSenderUID(), szTargetName);
			}
			break;
		
		case MC_MATCH_REQUEST_ACCOUNT_ITEMLIST:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				OnRequestAccountItemList(uidPlayer);
			}
			break;

		///< AccountItem ??????? ????? ????
		// ?????(2010-03-09)
		/*
		case MC_MATCH_REQUEST_MOVE_ACCOUNTITEM_IN_BANK : 
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				int nAIIDFrom = 0;
				int nAIIDTo = 0;
				int nCount = 0;

				pCommand->GetParameter(&nAIIDFrom, 0, MPT_INT);
				pCommand->GetParameter(&nAIIDTo, 1, MPT_INT);
				pCommand->GetParameter(&nCount, 2, MPT_INT);

				OnRequestAccountItemMoveInBank(uidPlayer, nAIIDFrom, nAIIDTo, nCount);
			}
			break;
		*/
		///< ???????
		
		case MC_MATCH_REQUEST_SUICIDE:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);

				OnRequestSuicide(uidPlayer);
			}
			break;
		case MC_MATCH_REQUEST_OBTAIN_WORLDITEM:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nItemUID = 0;

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nItemUID, 1, MPT_INT);

				OnRequestObtainWorldItem(uidPlayer, nItemUID);
			}
			break;
		case MC_MATCH_REQUEST_FLAG_CAP:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nItemID = 0;

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nItemID, 0, MPT_INT);

				OnRequestFlagCap(uidPlayer, nItemID);
			}
			break;
		case MC_MATCH_REQUEST_SPAWN_WORLDITEM:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nItemID = 0;
				MVector pos;
				float nDropDelayTime = 0.f;

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nItemID, 1, MPT_INT);
				pCommand->GetParameter(&pos, 2, MPT_POS);
				pCommand->GetParameter(&nDropDelayTime, 3, MPT_FLOAT);

				if(_isnan(pos.x) || _isnan(pos.y) || _isnan(pos.z) ||
				   !_finite(pos.x) || !_finite(pos.y) || !_finite(pos.z))
					break;
				OnRequestSpawnWorldItem(uidPlayer, nItemID, pos.x, pos.y, pos.z, nDropDelayTime);
			}
			break;
		case MC_MATCH_NOTIFY_THROW_TRAPITEM:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nItemID = 0;
				pCommand->GetParameter(&nItemID, 0, MPT_SHORT);

				OnNotifyThrowTrapItem(uidPlayer, nItemID);
			}
			break;
		case MC_MATCH_NOTIFY_ACTIVATED_TRAPITEM:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int nItemID = 0;
				MShortVector s_pos;
				pCommand->GetParameter(&nItemID, 0, MPT_SHORT);
				pCommand->GetParameter(&s_pos, 1, MPT_SVECTOR);

				OnNotifyActivatedTrapItem(uidPlayer, nItemID, MVector3(s_pos.x, s_pos.y, s_pos.z));
			}
			break;

					case MC_MATCH_REPORT:
			{
				char szSenderName[ MAX_CHARNAME_LENGTH ]="";
				char szTargetName[ MAX_CHARNAME_LENGTH ]="";
				char szMessage[64]="";
				if (pCommand->GetParameter(szSenderName, 0, MPT_STR, MAX_CHARNAME_LENGTH ) == false) break;
				if (pCommand->GetParameter(szTargetName, 1, MPT_STR, MAX_CHARNAME_LENGTH ) == false) break;
				if (pCommand->GetParameter(szMessage, 2, MPT_STR, CHAT_STRING_LEN ) == false) break;
				OnReportPlayer(szSenderName, szTargetName, szMessage);
			}break;



		case MC_MATCH_USER_WHISPER:
			{
				// char szSenderName[128]="";
				// char szTargetName[128]="";
				// char szMessage[1024]="";
				char szSenderName[ MAX_CHARNAME_LENGTH ]="";
				char szTargetName[ MAX_CHARNAME_LENGTH ]="";
				char szMessage[CHAT_STRING_LEN]="";
				
				if (pCommand->GetParameter(szSenderName, 0, MPT_STR, MAX_CHARNAME_LENGTH ) == false) break;
				if (pCommand->GetParameter(szTargetName, 1, MPT_STR, MAX_CHARNAME_LENGTH ) == false) break;
				if (pCommand->GetParameter(szMessage, 2, MPT_STR, CHAT_STRING_LEN ) == false) break;

				OnUserWhisper(pCommand->GetSenderUID(), szSenderName, szTargetName, szMessage);
			}
			break;
		case MC_MATCH_USER_WHERE:
			{
				// char szTargetName[MAX_CHARNAME]="";
				char szTargetName[ MAX_CHARNAME_LENGTH ]="";
				pCommand->GetParameter(szTargetName, 0, MPT_STR, MAX_CHARNAME_LENGTH );

				OnUserWhere(pCommand->GetSenderUID(), szTargetName);
			}
			break;
		case MC_MATCH_USER_OPTION:
			{
				unsigned long nOptionFlags=0;
				pCommand->GetParameter(&nOptionFlags, 0, MPT_UINT);

				OnUserOption(pCommand->GetSenderUID(), nOptionFlags);
			}
			break;
		case MC_MATCH_CHATROOM_CREATE:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				char szChatRoomName[MAX_CHATROOMNAME_STRING_LEN];

				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(szChatRoomName, 1, MPT_STR, MAX_CHATROOMNAME_STRING_LEN );

				OnChatRoomCreate(uidPlayer, szChatRoomName);
			}
			break;
		case MC_MATCH_CHATROOM_JOIN:
			{
				// char szPlayerName[MAX_CHARNAME];
				char szPlayerName[ MAX_CHARNAME_LENGTH ];
				char szChatRoomName[MAX_CHATROOMNAME_STRING_LEN];
				
				pCommand->GetParameter(szPlayerName, 0, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(szChatRoomName, 1, MPT_STR, MAX_CHATROOMNAME_STRING_LEN );

				OnChatRoomJoin(pCommand->GetSenderUID(), szPlayerName, szChatRoomName);
			}
			break;
		case MC_MATCH_CHATROOM_LEAVE:
			{
				// char szPlayerName[MAX_CHARNAME];
				char szPlayerName[ MAX_CHARNAME_LENGTH ];
				char szChatRoomName[MAX_CHATROOMNAME_STRING_LEN];
				
				pCommand->GetParameter(szPlayerName, 0, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(szChatRoomName, 1, MPT_STR, MAX_CHATROOMNAME_STRING_LEN );

				OnChatRoomLeave(pCommand->GetSenderUID(), szPlayerName, szChatRoomName);
			}
			break;
		case MC_MATCH_CHATROOM_SELECT_WRITE:
			{
				char szChatRoomName[MAX_CHATROOMNAME_STRING_LEN];

				pCommand->GetParameter(szChatRoomName, 0, MPT_STR, MAX_CHATROOMNAME_STRING_LEN );

				OnChatRoomSelectWrite(pCommand->GetSenderUID(), szChatRoomName);
			}
			break;
		case MC_MATCH_CHATROOM_INVITE:
			{
				char szSenderName[ MAX_CHARNAME_LENGTH ]="";
				char szTargetName[MAX_CHARNAME_LENGTH ]="";
				char szRoomName[MAX_CHATROOMNAME_STRING_LEN]="";

				pCommand->GetParameter(szSenderName, 0, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(szTargetName, 1, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(szRoomName, 2, MPT_STR, MAX_CHATROOMNAME_STRING_LEN );

				OnChatRoomInvite(pCommand->GetSenderUID(), szTargetName);
			}
			break;
		case MC_MATCH_CHATROOM_CHAT:
			{
				char szRoomName[MAX_CHATROOMNAME_STRING_LEN];
				// char szPlayerName[MAX_CHARNAME];
				char szPlayerName[ MAX_CHARNAME_LENGTH ];
				// char szMessage[256];
				char szMessage[CHAT_STRING_LEN];

				pCommand->GetParameter(szRoomName, 0, MPT_STR, MAX_CHATROOMNAME_STRING_LEN );
				pCommand->GetParameter(szPlayerName, 1, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(szMessage, 2, MPT_STR, CHAT_STRING_LEN);

				OnChatRoomChat(pCommand->GetSenderUID(), szMessage);
			}
			break;
		case MC_MATCH_REQUEST_MY_SIMPLE_CHARINFO:
			{
				MUID uidPlayer;
				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				OnRequestMySimpleCharInfo(uidPlayer);
			}
			break;
		case MC_MATCH_REQUEST_COPY_TO_TESTSERVER:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				OnRequestCopyToTestServer(uidPlayer);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_CREATE_CLAN:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				char szClanName[CLAN_NAME_LENGTH];
				char szSponsorNames[CLAN_SPONSORS_COUNT][256];
				char* sncv[CLAN_SPONSORS_COUNT];
				int nRequestID;
				bool bLeave = false;

				//pCommand->GetParameter(&uidPlayer,			0, MPT_UID);
				pCommand->GetParameter(&nRequestID,			1, MPT_INT);
				pCommand->GetParameter(szClanName,			2, MPT_STR, CLAN_NAME_LENGTH );

				if(SqlCheckString(szClanName)) break;

				for (int i = 0; i < CLAN_SPONSORS_COUNT; i++)
				{
					pCommand->GetParameter(szSponsorNames[i],	3+i, MPT_STR, sizeof(szSponsorNames[i]));

					if(SqlCheckString(szSponsorNames[i]))
					{
						bLeave = true;
						break;
					}

					sncv[i] = szSponsorNames[i];
				}

				if(bLeave) break;

				OnClanRequestCreateClan(uidPlayer, nRequestID, szClanName, sncv);
			}
			break;
		case MC_MATCH_CLAN_ANSWER_SPONSOR_AGREEMENT:
			{
				MUID uidClanMaster;
				int nRequestID;
				bool bAnswer;
				// char szCharName[MAX_CHARNAME];
				char szCharName[ MAX_CHARNAME_LENGTH ];

				pCommand->GetParameter(&nRequestID,		0, MPT_INT);
				pCommand->GetParameter(&uidClanMaster,	1, MPT_UID);
				pCommand->GetParameter(szCharName,		2, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(&bAnswer,		3, MPT_BOOL);

				OnClanAnswerSponsorAgreement(nRequestID, uidClanMaster, szCharName, bAnswer);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_AGREED_CREATE_CLAN:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				char szClanName[CLAN_NAME_LENGTH];
				char szSponsorNames[CLAN_SPONSORS_COUNT][256];
				char* sncv[CLAN_SPONSORS_COUNT];
				bool bLeave = false;

				//pCommand->GetParameter(&uidPlayer,			0, MPT_UID);
				pCommand->GetParameter(szClanName,			1, MPT_STR, CLAN_NAME_LENGTH );

				if(SqlCheckString(szClanName)) break;

				// ????? ?????u ????? ??�?. - 4??
				for (int i = 0; i < CLAN_SPONSORS_COUNT; i++)
				{
					pCommand->GetParameter(szSponsorNames[i],	2+i, MPT_STR, sizeof(szSponsorNames[i]) );

					if(SqlCheckString(szSponsorNames[i]))
					{
						bLeave = true;
						break;
					}

					sncv[i] = szSponsorNames[i];
				}

				if(bLeave) break;

				OnClanRequestAgreedCreateClan(uidPlayer, szClanName, sncv);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_CLOSE_CLAN:
			{
				MUID uidClanMaster;
				char szClanName[CLAN_NAME_LENGTH];

				uidClanMaster = pCommand->GetSenderUID();

				// pCommand->GetParameter(&uidClanMaster,		0, MPT_UID);
				pCommand->GetParameter(szClanName, 1, MPT_STR, CLAN_NAME_LENGTH );

				if(SqlCheckString(szClanName)) break;

				OnClanRequestCloseClan(uidClanMaster, szClanName);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_JOIN_CLAN:
			{
				// MUID uidClanAdmin;
				char szClanName[CLAN_NAME_LENGTH];
				// char szJoiner[MAX_CHARNAME];
				char szJoiner[ MAX_CHARNAME_LENGTH ];

				// pCommand->GetParameter(&uidClanAdmin,	0, MPT_UID);
				pCommand->GetParameter(szClanName,		1, MPT_STR, CLAN_NAME_LENGTH );
				pCommand->GetParameter(szJoiner,		2, MPT_STR, MAX_CHARNAME_LENGTH );

				if(SqlCheckString(szClanName)) break;
				if(SqlCheckString(szJoiner)) break;

				OnClanRequestJoinClan(pCommand->GetSenderUID(), szClanName, szJoiner);
			}
			break;
		case MC_MATCH_CLAN_ANSWER_JOIN_AGREEMENT:
			{
				MUID uidClanAdmin;
				bool bAnswer;
				// char szJoiner[MAX_CHARNAME];
				char szJoiner[ MAX_CHARNAME_LENGTH ];

				// uidClanAdmin = pCommand->GetSenderUID();

				pCommand->GetParameter(&uidClanAdmin,	0, MPT_UID);
				pCommand->GetParameter(szJoiner,		1, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(&bAnswer,		2, MPT_BOOL);

				if(SqlCheckString(szJoiner)) break;

				OnClanAnswerJoinAgreement(uidClanAdmin, szJoiner, bAnswer);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_AGREED_JOIN_CLAN:
			{
				///< ?? ????? ????? ????...		

				// MUID uidClanAdmin;
				char szClanName[CLAN_NAME_LENGTH];
				// char szJoiner[MAX_CHARNAME];
				char szJoiner[ MAX_CHARNAME_LENGTH ];

				// pCommand->GetParameter(&uidClanAdmin,	0, MPT_UID);
				pCommand->GetParameter(szClanName,		1, MPT_STR, CLAN_NAME_LENGTH );
				// ?????? ?????? ?? ??????? ????? ??????...-_-;;
				pCommand->GetParameter(szJoiner,		2, MPT_STR, MAX_CHARNAME_LENGTH );

				if(SqlCheckString(szClanName)) break;
				if(SqlCheckString(szJoiner)) break;

				OnClanRequestAgreedJoinClan(pCommand->GetSenderUID(), szClanName, szJoiner);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_LEAVE_CLAN:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer,	0, MPT_UID);

				OnClanRequestLeaveClan(uidPlayer);
			}
			break;
		case MC_MATCH_CLAN_MASTER_REQUEST_CHANGE_GRADE:
			{
				MUID uidClanMaster;
				// char szMember[MAX_CHARNAME];
				char szMember[ MAX_CHARNAME_LENGTH ];
				int nClanGrade;

				uidClanMaster = pCommand->GetSenderUID();

				// pCommand->GetParameter(&uidClanMaster,	0, MPT_UID);
				pCommand->GetParameter(szMember,		1, MPT_STR, MAX_CHARNAME_LENGTH );
				pCommand->GetParameter(&nClanGrade,		2, MPT_INT);

				if(SqlCheckString(szMember)) break;

				OnClanRequestChangeClanGrade(uidClanMaster, szMember, nClanGrade);
			}
			break;
		case MC_MATCH_CLAN_ADMIN_REQUEST_EXPEL_MEMBER:
			{
				MUID uidClanAdmin;
				// char szMember[MAX_CHARNAME];
				char szMember[ MAX_CHARNAME_LENGTH ];

				uidClanAdmin = pCommand->GetSenderUID();
				// pCommand->GetParameter(&uidClanAdmin,	0, MPT_UID);
				pCommand->GetParameter(szMember,		1, MPT_STR, MAX_CHARNAME_LENGTH );

				OnClanRequestExpelMember(uidClanAdmin, szMember);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_MSG:
			{
				MUID uidSender;
				char szMsg[MSG_STRING_LEN];

				uidSender = pCommand->GetSenderUID();
				// pCommand->GetParameter(&uidSender,	0, MPT_UID);
				pCommand->GetParameter(szMsg,		1, MPT_STR, MSG_STRING_LEN );

				OnClanRequestMsg(uidSender, szMsg);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_MEMBER_LIST:
			{
				MUID uidChar;

				uidChar = pCommand->GetSenderUID();
				// pCommand->GetParameter(&uidChar,	0, MPT_UID);

				OnClanRequestMemberList(uidChar);
			}
			break;
		case MC_MATCH_CLAN_REQUEST_CLAN_INFO:
			{
				MUID uidChar;
				char szClanName[CLAN_NAME_LENGTH] = "";

				uidChar = pCommand->GetSenderUID();
				// pCommand->GetParameter(&uidChar,	0, MPT_UID);
				
				pCommand->GetParameter(szClanName,	1, MPT_STR, CLAN_NAME_LENGTH );

				OnClanRequestClanInfo(uidChar, szClanName);

			}
			break;
		case MC_MATCH_CHANNEL_REQUEST_ALL_PLAYER_LIST:
			{
				MUID uidPlayer, uidChannel;
				unsigned long int nPlaceFilter;
				unsigned long int nOptions;
				uidPlayer = pCommand->GetSenderUID();

				//pCommand->GetParameter(&uidPlayer,		0, MPT_UID);
				pCommand->GetParameter(&uidChannel,		1, MPT_UID);
				pCommand->GetParameter(&nPlaceFilter,	2, MPT_UINT);
				pCommand->GetParameter(&nOptions,		3, MPT_UINT);

				OnChannelRequestAllPlayerList(uidPlayer, uidChannel, nPlaceFilter, nOptions);
			}
			break;
		case MC_MATCH_REQUEST_CHARINFO_DETAIL:
			{
				// MUID uidChar;
				char szCharName[ MAX_CHARNAME_LENGTH ];

				// pCommand->GetParameter(&uidChar,	0, MPT_UID);
				pCommand->GetParameter(szCharName,	1, MPT_STR, MAX_CHARNAME_LENGTH );

				OnRequestCharInfoDetail(pCommand->GetSenderUID(), szCharName);
			}
			break;


		case MC_MATCH_LADDER_REQUEST_CHALLENGE:
			{
				int nMemberCount;
				unsigned long int nOptions;

				pCommand->GetParameter(&nMemberCount,		0, MPT_INT);
				pCommand->GetParameter(&nOptions,			1, MPT_UINT);

				MCommandParameter* pMemberNamesBlobParam = pCommand->GetParameter(2);
				if(pMemberNamesBlobParam->GetType()!=MPT_BLOB) break;

				void* pMemberNamesBlob = pMemberNamesBlobParam->GetPointer();
				if( NULL == pMemberNamesBlob )
					break;

				OnLadderRequestChallenge(pCommand->GetSenderUID(), pMemberNamesBlob, nOptions);
			}
			break;
		case MC_MATCH_LADDER_REQUEST_CANCEL_CHALLENGE:
			{
				OnLadderRequestCancelChallenge(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_REQUEST_PROPOSAL:
			{
				MUID uidChar;
				int nProposalMode, nRequestID, nReplierCount;

				uidChar = pCommand->GetSenderUID();
				// pCommand->GetParameter(&uidChar,			0, MPT_UID);

				pCommand->GetParameter(&nProposalMode,		1, MPT_INT);
				pCommand->GetParameter(&nRequestID,			2, MPT_INT);
				pCommand->GetParameter(&nReplierCount,		3, MPT_INT);

				MCommandParameter* pReplierNamesParam = pCommand->GetParameter(4);
				if(pReplierNamesParam->GetType()!=MPT_BLOB) break;

				void* pReplierNamesBlob = pReplierNamesParam->GetPointer();
				if( NULL == pReplierNamesBlob )
					break;

				OnRequestProposal(uidChar, nProposalMode, nRequestID, nReplierCount, pReplierNamesBlob);
			}
			break;
		case MC_MATCH_REPLY_AGREEMENT:
			{
				MUID uidProposer, uidReplier;
				char szReplierName[256];
				int nProposalMode, nRequestID;
				bool bAgreement;

				pCommand->GetParameter(&uidProposer,	0, MPT_UID);

				// pCommand->GetParameter(&uidReplier,		1, MPT_UID);
				uidReplier = pCommand->GetSenderUID();

				pCommand->GetParameter(szReplierName,	2, MPT_STR, sizeof(szReplierName) );
				pCommand->GetParameter(&nProposalMode,	3, MPT_INT);
				pCommand->GetParameter(&nRequestID,		4, MPT_INT);
				pCommand->GetParameter(&bAgreement,		5, MPT_BOOL);

				OnReplyAgreement(uidProposer, uidReplier, szReplierName, nProposalMode, nRequestID, bAgreement);
			}
			break;
		case MC_MATCH_CALLVOTE:
			{
				char szDiscuss[ VOTE_DISCUSS_STRING_LEN ]="";
				char szArg[ VOTE_ARG_STRING_LEN ]="";
				
				pCommand->GetParameter(szDiscuss, 0, MPT_STR, VOTE_DISCUSS_STRING_LEN );
				pCommand->GetParameter(szArg, 1, MPT_STR, VOTE_ARG_STRING_LEN );
				OnVoteCallVote(pCommand->GetSenderUID(), szDiscuss, szArg);
			}
			break;
		case MC_MATCH_VOTE_YES:
			{
				OnVoteYes(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_VOTE_NO:
			{
				OnVoteNo(pCommand->GetSenderUID());
			}
			break;
		case MC_MATCH_CLAN_REQUEST_EMBLEMURL:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				MCommandParameter* pParam = pCommand->GetParameter(0);
				if (pParam->GetType() != MPT_BLOB)
				{
					break;
				}
				void* pEmblemURLListBlob = pParam->GetPointer();
				if( NULL == pEmblemURLListBlob )
					break;

				OnClanRequestEmblemURL(uidPlayer, pEmblemURLListBlob);
			}
			break;
		
		case MC_QUEST_REQUEST_NPC_DEAD:
			{
				MUID uidKiller, uidNPC;
				MShortVector s_pos;
				pCommand->GetParameter(&uidKiller,	0, MPT_UID);
				pCommand->GetParameter(&uidNPC,		1, MPT_UID);
				pCommand->GetParameter(&s_pos,		2, MPT_SVECTOR);

				MUID uidSender = pCommand->GetSenderUID();
				MVector pos = MVector((float)s_pos.x, (float)s_pos.y, (float)s_pos.z);

				OnRequestNPCDead(uidSender, uidKiller, uidNPC, pos);

			}
			break;
		case MC_MATCH_QUEST_REQUEST_DEAD:
			{
				MUID uidVictim = pCommand->GetSenderUID();
				OnQuestRequestDead(uidVictim);
			}
			break;
		case MC_QUEST_TEST_REQUEST_NPC_SPAWN:
			{
				int nNPCType, nNPCCount;

				pCommand->GetParameter(&nNPCType,	0, MPT_INT);
				pCommand->GetParameter(&nNPCCount,	1, MPT_INT);
				MUID uidPlayer = pCommand->GetSenderUID();

				OnQuestTestRequestNPCSpawn(uidPlayer, nNPCType, nNPCCount);
			}
			break;
		case MC_QUEST_TEST_REQUEST_CLEAR_NPC:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				OnQuestTestRequestClearNPC(uidPlayer);
			}
			break;
		case MC_QUEST_TEST_REQUEST_SECTOR_CLEAR:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				OnQuestTestRequestSectorClear(uidPlayer);
			}
			break;
		case MC_QUEST_TEST_REQUEST_FINISH:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				OnQuestTestRequestQuestFinish(uidPlayer);
			}
			break;
		case MC_QUEST_REQUEST_MOVETO_PORTAL:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				OnQuestRequestMovetoPortal(uidPlayer);
			}
			break;
		case MC_QUEST_READYTO_NEWSECTOR:
			{
				MUID uidPlayer = pCommand->GetSenderUID();

				OnQuestReadyToNewSector(uidPlayer);
			}
			break;

		case MC_QUEST_PONG:
			{
				MUID uidPlayer = pCommand->GetSenderUID();
//				unsigned long int nTimeStamp;
//				
//				pCommand->GetParameter(&nTimeStamp, 0, MPT_UINT);

				OnQuestPong(uidPlayer);
			}
			break;

		case MC_MATCH_REQUEST_CHAR_QUEST_ITEM_LIST :
			{
				MUID uidChar;

				pCommand->GetParameter( &uidChar, 0, MPT_UID );

				OnRequestCharQuestItemList( uidChar );
			}
			break;

		case MC_MATCH_REQUEST_DROP_SACRIFICE_ITEM :
			{
				// MUID	uidQuestItemOwner;
				int		nSlotIndex;
				int		nItemID;

				// pCommand->GetParameter( &uidQuestItemOwner, 0, MPT_UID );
				pCommand->GetParameter( &nSlotIndex, 1, MPT_INT );
				pCommand->GetParameter( &nItemID, 2, MPT_INT );

				OnRequestDropSacrificeItemOnSlot( pCommand->GetSenderUID(), nSlotIndex, nItemID );
			}
			break;

		case MC_MATCH_REQUEST_CALLBACK_SACRIFICE_ITEM :
			{
				MUID uidPlayer = pCommand->GetSenderUID();
				int		nSlotIndex;
				int		nItemID;

				//pCommand->GetParameter( &uidPlayer, 0, MPT_UID );
				pCommand->GetParameter( &nSlotIndex, 1, MPT_INT );
				pCommand->GetParameter( &nItemID, 2, MPT_INT );

				OnRequestCallbackSacrificeItem( uidPlayer, nSlotIndex, nItemID );
			}
			break;

		case MC_MATCH_REQUEST_BUY_QUEST_ITEM :
			{
				int	nItemID;
				int	nItemCount;

				pCommand->GetParameter( &nItemID, 1, MPT_INT );
				pCommand->GetParameter( &nItemCount, 2, MPT_INT );

				OnRequestBuyQuestItem( pCommand->GetSenderUID(), nItemID, nItemCount );
			}
			break;

		case MC_MATCH_REQUEST_SELL_QUEST_ITEM :
			{
				int		nItemID;
				int		nCount;

				pCommand->GetParameter( &nItemID, 1, MPT_INT );
				pCommand->GetParameter( &nCount, 2, MPT_INT );

				OnRequestSellQuestItem( pCommand->GetSenderUID(), nItemID, nCount );
			}
			break;

		case MC_QUEST_REQUEST_QL :
			{
				// MUID uidSender;

				// pCommand->GetParameter( &uidSender, 0, MPT_UID );

				OnRequestQL( pCommand->GetSenderUID() );
			}
			break;

		case MC_MATCH_REQUEST_SLOT_INFO :
			{
				//MUID uidSender;

				//pCommand->GetParameter( &uidSender, 0, MPT_UID );

				OnRequestSacrificeSlotInfo( pCommand->GetSenderUID() );
			}
			break;

		case MC_QUEST_STAGE_MAPSET :
			{
				MUID uidStage;
				char nMapsetID;

				pCommand->GetParameter( &uidStage,	0, MPT_UID );
				pCommand->GetParameter( &nMapsetID, 1, MPT_CHAR );

				OnQuestStageMapset(uidStage, (int)nMapsetID );
			}
			break;
		case MC_MATCH_REQUEST_MONSTER_BIBLE_INFO :
			{
				MUID uidSender;

				pCommand->GetParameter( &uidSender, 0, MPT_UID );

				OnRequestMonsterBibleInfo( uidSender );
			}
			break;

		case MC_RESPONSE_RESOURCE_CRC32 :
			{
				DWORD dwResourceXORCache = 0;
				DWORD dwResourceCRC32Cache = 0;

				pCommand->GetParameter( &dwResourceCRC32Cache, 0, MPT_UINT );
				pCommand->GetParameter( &dwResourceXORCache, 1, MPT_UINT );

				_ASSERT( 0 != dwResourceXORCache );
				_ASSERT( 0 != dwResourceCRC32Cache );

				MMatchObject* pObj = GetObject( pCommand->GetSenderUID() );
				if( NULL == pObj )
				{
					return false;
				}

				MMatchStage* pStage = FindStage( pObj->GetStageUID() );
				if( NULL == pStage )
				{
					// ?????? ??? ?????? ???????? ???.
					_ASSERT( 0 );
					return false;
				}

				if( !pStage->IsValidResourceCRC32Cache(pCommand->GetSenderUID(), dwResourceCRC32Cache, dwResourceXORCache) )
				{
					//StageLeave( pObj->GetUID());

					LOG(LOG_PROG,"Invalid Item CRC Check (ID:%s)", pObj->GetAccountName());

					DisconnectObject(pCommand->GetSenderUID());

					return false;
				}
			}
			break;


		///< ?????(2009.08.04)
		///< CommandBuilder ????? ???? Flooding ???? ?????? ???? Command �?????.
		///< ?? ???? Handler?? ?a? ?? ??? ??????.. ??� ?? ????.. ????? �??????!
		///< ?????? MC_NET_CLEAR???? ???? ??????? Abuse?? ??u ?? ???.		
		case MC_NET_BANPLAYER_FLOODING :
			{
				MUID uidPlayer;
				
				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				if (MGetServerConfig()->IsUseBlockFlooding())
				{
					MMatchObject* pObj = GetObject( uidPlayer );
					if( pObj && pObj->GetDisconnStatusInfo().GetStatus() == MMDS_CONNECTED)
					{
						if( pObj->GetAccountName() ) {
							LOG(LOG_PROG,"Ban Player On Flooding - (MUID:%d%d, ID:%s)"
								, uidPlayer.High, uidPlayer.Low, pObj->GetAccountName());
						} else {
							LOG(LOG_PROG,"Ban Player On Flooding - (MUID:%d%d, ID:%s)"
								, uidPlayer.High, uidPlayer.Low);
						}
						
						pObj->DisconnectHacker( MMHT_COMMAND_FLOODING );
					} 
					else 
					{
						LOG(LOG_PROG,"Ban Player On Flooding - Can't Find Object");
					}
				}
			}
			break;
		///< ???????

		case MC_MATCH_DUELTOURNAMENT_REQUEST_JOINGAME :
			{
				MUID uidPlayer;
				MDUELTOURNAMENTTYPE nType;

				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nType, 1, MPT_INT);

				if( MGetServerConfig()->IsEnabledDuelTournament() )	{
                    ResponseDuelTournamentJoinChallenge(uidPlayer, nType);
				}

			}
			break;

		case MC_MATCH_DUELTOURNAMENT_REQUEST_CANCELGAME :
			{
				MUID uidPlayer;
				MDUELTOURNAMENTTYPE nType;

				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
				pCommand->GetParameter(&nType, 1, MPT_INT);

				if( MGetServerConfig()->IsEnabledDuelTournament() )	{
					ResponseDuelTournamentCancelChallenge(uidPlayer, nType);
				}
			}
			break;

		case MC_MATCH_DUELTOURNAMENT_REQUEST_SIDERANKING_INFO :
			{
				MUID uidPlayer;

				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);

				if( MGetServerConfig()->IsEnabledDuelTournament() ){
					ResponseDuelTournamentCharSideRanking(uidPlayer);
				}
			}
			break;
 
		case MC_MATCH_DUELTOURNAMENT_GAME_PLAYER_STATUS :
			{
				MUID uidPlayer;
				pCommand->GetParameter(&uidPlayer, 0, MPT_UID);

				if( MGetServerConfig()->IsEnabledDuelTournament() ){
					ResponseDuelTournamentCharStatusInfo(uidPlayer, pCommand);
				}
			}
			break;

		case MC_MATCH_REQUEST_USE_SPENDABLE_NORMAL_ITEM :
			{
				MUID uidItem, uidPlayer;

				uidPlayer = pCommand->GetSenderUID();
				pCommand->GetParameter(&uidItem, 0, MPT_UID);

				OnRequestUseSpendableNormalItem(uidPlayer, uidItem);			
			}
			break;

		case MC_MATCH_REQUEST_USE_SPENDABLE_BUFF_ITEM :
			{
				MUID uidItem;
				pCommand->GetParameter(&uidItem, 0, MPT_UID);

				//?????????????? OnRequestUseSpendableBuffItem(pCommand->GetSenderUID(), uidItem);
				_ASSERT(0);//???�???????? ???? ?????? ??? ?????? ??????
			}
			break;

		case MC_NEWQUEST_REQUEST_NPC_DEAD:
		case MC_NEWQUEST_REQUEST_NPC_SPAWN:
			{
				MUID uidSender = pCommand->GetSenderUID();
				MMatchObject* pSender = GetObject(uidSender);
				if (!IsEnabledObject(pSender)) break;

				MMatchStage* pStage = FindStage(pSender->GetStageUID());
				if (!pStage)
				{
					_ASSERT(0); break;
				}

				pStage->OnCommand(pCommand);
			}
			break;
		case MC_MATCH_REQUEST_PLANT_BOMB:
		{

			MUID planter;
			pCommand->GetParameter(&planter, 0, MPT_UID);
			MMatchObject* player = GetObject(planter);
			if (!IsEnabledObject(player)) break;

			OnRequestPlantBomb(player->GetUID());

		}break;
		//Custom: blitz
		case MC_BLITZ_REQUEST_CLASS:
		case MC_BLITZ_REQUEST_UPGRADE:
		{
			MUID sender = pCommand->GetSenderUID();
			MMatchObject* player = GetObject(sender);
			if (!IsEnabledObject(player))
				break;

			MMatchStage* pStage = FindStage(player->GetStageUID());
			if (!pStage)
			{
				_ASSERT(0); break;
			}

			pStage->OnCommand(pCommand);

		}break;
		case MC_MATCH_CHECK_STAGEMODS:
		{
			MCommandParameter* stageModParam = pCommand->GetParameter(0);
			if (stageModParam->GetType() != MPT_BLOB)
				break;

			vector<int> clientMods;
			void* stageModBlob = stageModParam->GetPointer();
			int count = MGetBlobArrayCount(stageModBlob);
			for (int i = 0; i < count; ++i)
			{
				clientMods.push_back(*(int*)MGetBlobArrayElement(stageModBlob, i));
			}

			MUID player;
			pCommand->GetParameter(&player, 1, MPT_UID);

			OnCheckStageModHacks(clientMods, player);
		}break;
		case MC_MATCH_UPDATE_SKILLMAP_BESTTIME:
		{
			MUID player;
			int mapName;
			unsigned int bestTime;
			pCommand->GetParameter(&player, 0, MPT_UID);
			pCommand->GetParameter(&mapName, 1, MPT_INT);
			pCommand->GetParameter(&bestTime, 2, MPT_UINT);

			RequestUpdateSkillMapBestTime(player, mapName, bestTime);
		}break;
		case MC_MATCH_REQUEST_SKILLMAP_BESTTIME:
		{
			MUID player;
			char mapName[24];
			pCommand->GetParameter(&player, 0, MPT_UID);
			pCommand->GetParameter(&mapName, 1, MPT_STR, 24);

			ResponseSkillMapBestTime(player, mapName);
		}break;
		
		case MC_ADMIN_REQUEST_SLAP:
		{
			MUID admin, player;
			pCommand->GetParameter(&admin, 0, MPT_UID);
			pCommand->GetParameter(&player, 1, MPT_UID);

			OnResponseAdminSlap(admin, player);
		}break;
		case MC_MATCH_REQUEST_CREATE_ACCOUNT:
		{
			char szUserID[64];
			char szPassword[64];
			char szEmail[64];

			if (pCommand->GetParameter(szUserID, 0, MPT_STR, MAX_USERID_STRING_LEN) == false) break;
			if (pCommand->GetParameter(szPassword, 1, MPT_STR, MAX_USER_PASSWORD_STRING_LEN) == false) break;
			if (pCommand->GetParameter(szEmail, 2, MPT_STR, MAX_USER_PASSWORD_STRING_LEN) == false) break;

			CreateAccount(pCommand->GetSenderUID(), szUserID, szPassword, szEmail);
		}
		break;


		default:
//			_ASSERT(0);	// ���� �ڵ鷯�� ����.
			return false;
	}

_STATUS_CMD_END;


	return true;
}