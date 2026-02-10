#include "StdAfx.h"
#include "PythonNetworkStream.h"
#include "PythonApplication.h"
#include "Packet.h"
#include "../eterpack/EterPackManager.h"
// HandShake ---------------------------------------------------------------------------
void CPythonNetworkStream::HandShakePhase()
{
	TPacketHeader header;

	if (!CheckPacket(&header))
		return;

	switch (header)
	{
		case HEADER_GC_PHASE:
			if (RecvPhasePacket())
				return;
			break;


		case HEADER_GC_HANDSHAKE:
			{
				if (!Recv(sizeof(TPacketGCHandshake), &m_HandshakeData))
					return;

				Tracenf("HANDSHAKE RECV %u %d", m_HandshakeData.dwTime, m_HandshakeData.lDelta);

				ELTimer_SetServerMSec(m_HandshakeData.dwTime+ m_HandshakeData.lDelta);

				//m_dwBaseServerTime = m_HandshakeData.dwTime+ m_HandshakeData.lDelta;
				//m_dwBaseClientTime = ELTimer_GetMSec();

				m_HandshakeData.dwTime = m_HandshakeData.dwTime + m_HandshakeData.lDelta + m_HandshakeData.lDelta;
				m_HandshakeData.lDelta = 0;

				Tracenf("HANDSHAKE SEND %u", m_HandshakeData.dwTime);

				if (!Send(sizeof(TPacketGCHandshake), &m_HandshakeData))
				{
					assert(!"Failed Sending Handshake");
					return;
				}

				CTimer::Instance().SetBaseTime();
				return;
			}
			break;
		case HEADER_GC_PING:
			RecvPingPacket();
			return;
			break;

		case HEADER_GC_HYBRIDCRYPT_KEYS:
			RecvHybridCryptKeyPacket();
			return;
			break;

		case HEADER_GC_HYBRIDCRYPT_SDB:
			RecvHybridCryptSDBPacket();
			return;
			break;
	}

	RecvErrorPacket(header);
}

void CPythonNetworkStream::SetHandShakePhase()
{
	if ("HandShake"!=m_strPhase)
		m_phaseLeaveFunc.Run();

	Tracen("");
	Tracen("## Network - Hand Shake Phase ##");
	Tracen("");

	m_strPhase = "HandShake";

	m_dwChangingPhaseTime = ELTimer_GetMSec();
	m_phaseProcessFunc.Set(this, &CPythonNetworkStream::HandShakePhase);
	m_phaseLeaveFunc.Set(this, &CPythonNetworkStream::__LeaveHandshakePhase);

	SetGameOnline();

	if (__DirectEnterMode_IsSet())
	{
		// None
	}
	else
	{
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_LOGIN], "OnHandShake", Py_BuildValue("()"));
	}
}

bool CPythonNetworkStream::RecvHandshakePacket()
{
	TPacketGCHandshake kHandshakeData;
	if (!Recv(sizeof(TPacketGCHandshake), &kHandshakeData))
		return false;

	Tracenf("HANDSHAKE RECV %u %d", kHandshakeData.dwTime, kHandshakeData.lDelta);

	m_kServerTimeSync.m_dwChangeServerTime = kHandshakeData.dwTime + kHandshakeData.lDelta;
	m_kServerTimeSync.m_dwChangeClientTime = ELTimer_GetMSec();
	
	kHandshakeData.dwTime = kHandshakeData.dwTime + kHandshakeData.lDelta + kHandshakeData.lDelta;
	kHandshakeData.lDelta = 0;

	Tracenf("HANDSHAKE SEND %u", kHandshakeData.dwTime);

	kHandshakeData.bHeader = HEADER_CG_TIME_SYNC;
	if (!Send(sizeof(TPacketGCHandshake), &kHandshakeData))
	{
		assert(!"Failed Sending Handshake");
		return false;
	}

	SendSequence();

	return true;
}

bool CPythonNetworkStream::RecvHandshakeOKPacket()
{
	TPacketGCBlank kBlankPacket;
	if (!Recv(sizeof(TPacketGCBlank), &kBlankPacket))
		return false;

	DWORD dwDelta=ELTimer_GetMSec()-m_kServerTimeSync.m_dwChangeClientTime;
	ELTimer_SetServerMSec(m_kServerTimeSync.m_dwChangeServerTime+dwDelta);

	Tracenf("HANDSHAKE OK RECV %u %u", m_kServerTimeSync.m_dwChangeServerTime, dwDelta);

	return true;
}

bool CPythonNetworkStream::RecvHybridCryptKeyPacket()
{
	int iFixedHeaderSize = TPacketGCHybridcryptKeys::GetFixedHeaderSize();

	TDynamicSizePacketHeader header;
	if( !Peek( sizeof(header), &header) )
		return false;

	TPacketGCHybridcryptKeys kPacket(header.size-iFixedHeaderSize);

	if (!Recv(iFixedHeaderSize, &kPacket))
		return false;

	if (!Recv(kPacket.KeyStreamLen, kPacket.m_pStream))
		return false;

	CEterPackManager::Instance().RetrieveHybridCryptPackKeys( kPacket.m_pStream ); 
	return true;
}

bool CPythonNetworkStream::RecvHybridCryptSDBPacket()
{
	int iFixedHeaderSize = TPacketGCHybridcryptSdb::GetFixedHeaderSize();

	TDynamicSizePacketHeader header;
	if( !Peek( sizeof(header), &header) )
		return false;

	TPacketGCHybridcryptSdb kPacket(header.size-iFixedHeaderSize);

	if (!Recv(iFixedHeaderSize, &kPacket))
		return false;

	if (!Recv(kPacket.iStreamLen, kPacket.m_pStream))
		return false;

	CEterPackManager::Instance().RetrieveHybridCryptPackSDB( kPacket.m_pStream ); 
	return true;
}



