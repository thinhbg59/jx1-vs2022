// TongDB.cpp: implementation of the CTongDB class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Global.h"
#include "db.h"
#include "DBTable.h"
#include "TongDB.h"
#include "S3Relay.h"
#include "time.h"

CTongDB		g_cTongDB;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
int get_TongName(DB *db, const DBT *pkey, const DBT *pdata, DBT *ikey) 
{
	//给定一个完整的buffer，得到account作为索引
	memset( ikey, 0, sizeof( DBT ) );
	//TRoleData *pRoleData = (TRoleData *)pdata->data;
	//ikey->data = pRoleData->BaseInfo.caccname;
	//ikey->size = strlen( pRoleData->BaseInfo.caccname ) + 1;
	TMemberStruct* pMemberStruct = (TMemberStruct*)pdata->data;
	ikey->data = pMemberStruct->szTong;
	ikey->size = strlen( pMemberStruct->szTong ) + 1;
	return 0;
}

CTongDB::CTongDB()
{//初始化变量
	m_TongListCount = 0;
	memset(m_TongList,0,sizeof(TTongList) * MAX_TONG_COUNT);
}

bool CTongDB::Open()
{
	m_TongTable = new ZDBTable("TongDB","TongTable");		//打开帮会表
	if(!m_TongTable->open())return false;

	m_MemberTable = new ZDBTable("TongMemberDB","MemberTable");	//帮众表
	m_MemberTable->addIndex(get_TongName);
	if(!m_MemberTable->open())
	{
		m_TongTable->close();
		return false;
	}
	return true;
}

bool CTongDB::OpenNew()
{
	DeleteFile("TongDB");
	DeleteFile("TongMemberDB");
	m_TongTable = new ZDBTable("TongDB","TongTable");		//打开帮会表
	if(!m_TongTable->open())return false;
	
	m_MemberTable = new ZDBTable("TongMemberDB","MemberTable");	//帮众表
	m_MemberTable->addIndex(get_TongName);
	if(!m_MemberTable->open())
	{
		m_TongTable->close();
		return false;
	}
	return true;
}

void CTongDB::Close()
{
	m_TongTable->close();
	m_MemberTable->close();
	delete m_TongTable;
	delete m_MemberTable;
}

bool CTongDB::ChangeTong(CTongControl& aTongControl)
{//创建和改变帮派
	bool aResult = true;
	int aKeySize = strlen(aTongControl.m_szName) + 1;
	int aSize = sizeof(TTongStruct);
	TTongStruct aTongStruct;
	memset(&aTongStruct, 0, aSize);
	aTongStruct.nCamp = aTongControl.m_nCamp;
	aTongStruct.dwMoney = aTongControl.m_dwMoney;
	aTongStruct.dwTotalEff = aTongControl.m_dwTotalEff;
	aTongStruct.nCredit = aTongControl.m_nCredit;
	aTongStruct.btLevel = aTongControl.m_btLevel;
	aTongStruct.bRecruit = aTongControl.m_bRecruit;
	aTongStruct.nTongParam = aTongControl.m_nTongParam;
	strcpy(aTongStruct.szName, aTongControl.m_szName);
	strcpy(aTongStruct.szMasterAgname, aTongControl.m_szMasterAgname);
	strcpy(aTongStruct.szNormalAgname, aTongControl.m_szNormalAgname);
	aTongStruct.nTongJiyuParam = aTongControl.m_nTongJiyuParam;
	strcpy(aTongStruct.szTongJiyuNotify, aTongControl.m_szTongJiyuNotify);
	int i;
	for(i=0;i<defTONG_MAX_DIRECTOR;++i)
	{
		strcpy(aTongStruct.szDirectorAgname[i], aTongControl.m_szDirectorAgname[i]);
	}
	for(i=0;i<defTONG_MAX_MANAGER;++i)
	{
		strcpy(aTongStruct.szManagerAgname[i], aTongControl.m_szManagerAgname[i]);
	}
	aResult = m_TongTable->add(aTongControl.m_szName, aKeySize,(char*)(&aTongStruct), aSize);

	return aResult;
}

bool CTongDB::DelTong(char* aTongName)
{//删除帮派
	bool aResult = true;
	int aKeySize = strlen(aTongName) + 1;

	aResult = m_TongTable->remove(aTongName, aKeySize);
	if(!aResult) return aResult;

	while(!m_MemberTable->remove(aTongName, aKeySize, 0))
		{}
	return true;
}

bool CTongDB::SearchTong(char* aTongName, CTongControl& aTongControl)
{//从数据库中取得帮派
	bool aResult = true;
	int aSize = sizeof(TTongStruct);
	int aKeySize = strlen(aTongName) + 1;
	char* aBuffer = m_TongTable->search(aTongName, aKeySize, aSize);
	if(!aBuffer) return false;	//找不到帮派

	TTongStruct* pTongStruct = (TTongStruct*)aBuffer;
	aTongControl.m_nCamp = pTongStruct->nCamp;
	aTongControl.m_btLevel = pTongStruct->btLevel;
	aTongControl.m_dwMoney = pTongStruct->dwMoney;
	aTongControl.m_dwTotalEff = pTongStruct->dwTotalEff;
	aTongControl.m_bRecruit = pTongStruct->bRecruit;
	aTongControl.m_nTongParam = pTongStruct->nTongParam;
	strcpy(aTongControl.m_szName, pTongStruct->szName);
	strcpy(aTongControl.m_szMasterAgname, pTongStruct->szMasterAgname);
	strcpy(aTongControl.m_szNormalAgname, pTongStruct->szNormalAgname);
	aTongControl.m_nTongJiyuParam = pTongStruct->nTongJiyuParam;
	strcpy(aTongControl.m_szTongJiyuNotify, pTongStruct->szTongJiyuNotify);
	for(int i=0;i<defTONG_MAX_DIRECTOR;++i)
	{
		strcpy(aTongControl.m_szDirectorAgname[i], pTongStruct->szDirectorAgname[i]);
	}
	for(int i=0;i<defTONG_MAX_MANAGER;++i)
	{
		strcpy(aTongControl.m_szManagerAgname[i], pTongStruct->szManagerAgname[i]);
	}

	int aDirectorNum = 0;
	int aManagerNum = 0;
	int aMemberNum = 0;

	aBuffer = m_MemberTable->search(aTongName, aKeySize, aSize, 0);
	while(aBuffer)
	{
		TMemberStruct* pMemberStruct = (TMemberStruct*)aBuffer;
		switch(pMemberStruct->MemberClass)
		{
		case enumTONG_FIGURE_MASTER:
			strcpy(aTongControl.m_szMasterName, pMemberStruct->szName);
			aTongControl.m_dwMasterID = g_String2Id(pMemberStruct->szName);
			aTongControl.m_nMasterJoinTm, pMemberStruct->nJoinTm;
			aTongControl.m_nMasterSaveEff = pMemberStruct->nSaveEff;
			break;
			
		case enumTONG_FIGURE_DIRECTOR:
			if (pMemberStruct->nTitleIndex < 0 || pMemberStruct->nTitleIndex >= defTONG_MAX_DIRECTOR)
				break;
			strcpy(aTongControl.m_szDirectorName[pMemberStruct->nTitleIndex], pMemberStruct->szName);
			aTongControl.m_dwDirectorID[pMemberStruct->nTitleIndex] = g_String2Id(pMemberStruct->szName);
			aTongControl.m_nDirectorJoinTm[pMemberStruct->nTitleIndex] = pMemberStruct->nJoinTm;
			aTongControl.m_nDirectorSaveEff[pMemberStruct->nTitleIndex] = pMemberStruct->nSaveEff;
			aDirectorNum++;
			break;
			
		case enumTONG_FIGURE_MANAGER:
			if (pMemberStruct->nTitleIndex < 0 || pMemberStruct->nTitleIndex >= defTONG_MAX_MANAGER)
				break;
			strcpy(aTongControl.m_szManagerName[pMemberStruct->nTitleIndex], pMemberStruct->szName);
			aTongControl.m_dwManagerID[pMemberStruct->nTitleIndex] = g_String2Id(pMemberStruct->szName);
			aTongControl.m_nManagerJoinTm[pMemberStruct->nTitleIndex] = pMemberStruct->nJoinTm;
			aTongControl.m_nManagerSaveEff[pMemberStruct->nTitleIndex] = pMemberStruct->nSaveEff;
			aManagerNum++;
			break;
			
		case enumTONG_FIGURE_MEMBER:
			aTongControl.AddMember(pMemberStruct->szName, pMemberStruct->nSex, pMemberStruct->nJoinTm, pMemberStruct->nSaveEff);
			aMemberNum++;
			break;
			
		}
		aBuffer = m_MemberTable->next(aSize);
	}
	aTongControl.m_nDirectorNum = aDirectorNum;
	aTongControl.m_nManagerNum = aManagerNum;
	aTongControl.m_nMemberNum = aMemberNum;

	delete [] aBuffer;

	return aResult;
}

int CTongDB::GetTongCount()
{//取得门派数
	int aCount = 0;
	int aTongSize;
	int aTongKeySize;
	char aTongKeyBuffer[defTONG_NAME_LENGTH_32];
	char aTongBuffer[sizeof(TTongStruct)];
	bool aResult = m_TongTable->GetRecordEx(aTongBuffer, aTongSize, aTongKeyBuffer, aTongKeySize,ZDBTable::cpFirst);
	while(aResult)
	{
		aCount++;
		aResult = m_TongTable->GetRecordEx(aTongBuffer, aTongSize, aTongKeyBuffer, aTongKeySize,ZDBTable::cpNext);
	}

	return aCount;
}

int CTongDB::GetTongList(TTongList* aTongList, int aMaxNum)
{//取得帮会列表
		int aCount = 0;
	int aTongSize;
	int aTongKeySize;
	char aTongKeyBuffer[defTONG_NAME_LENGTH_32];
	char aTongBuffer[sizeof(TTongStruct)];
	bool aResult = m_TongTable->GetRecordEx(aTongBuffer, aTongSize, aTongKeyBuffer, aTongKeySize,ZDBTable::cpFirst);
	while(aResult)
	{
		TTongStruct* pTong = (TTongStruct*)aTongBuffer;
		strcpy(aTongList[aCount].szName, pTong->szName);

		int aMemCount = 0;
		int aMemSize;
		char* aMemBuffer;
		aMemBuffer = m_MemberTable->search(aTongKeyBuffer, aTongKeySize, aMemSize, 0);
		while(aMemBuffer)
		{
			TMemberStruct* pMember = (TMemberStruct*)aMemBuffer;
			if(pMember->MemberClass == enumTONG_FIGURE_MEMBER)
					aMemCount++;
			delete [] aMemBuffer;
			aMemBuffer = m_MemberTable->next(aMemSize);
		}

		aTongList[aCount].MemberCount = aMemCount;

		aCount++;
		if( aCount>= aMaxNum ) break;
		aResult = m_TongTable->GetRecordEx(aTongBuffer, aTongSize, aTongKeyBuffer, aTongKeySize,ZDBTable::cpNext);
	}

	return aCount;
}

bool CTongDB::ChangeMember(TMemberStruct aMember)
{//增加帮众
	return m_MemberTable->add(aMember.szName, strlen(aMember.szName) + 1, (char*)(&aMember), sizeof(TMemberStruct));
}


bool CTongDB::DelMember(char* aMemberName)
{//删除帮众
	int aKeySize = strlen(aMemberName) + 1;

	return m_MemberTable->remove(aMemberName, aKeySize);
}