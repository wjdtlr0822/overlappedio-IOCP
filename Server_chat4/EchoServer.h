#pragma once

#include "IOCPServer.h"
#include "Packet.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>


class EchoServer : public IOCPServer
{
public:

	EchoServer() = default;
	virtual ~EchoServer() = default;

	virtual void OnConnect(const UINT32 clientInedx_) override {
		printf("[OnConnect] Index : %d\n", clientInedx_);
	}

	virtual void OnClose(const UINT32 clientIndex_) override {
		printf("[OnClose] Index : %d\n", clientIndex_);
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pMsg) override {
		printf("[OnReceive] Index : %d, dataSize : %d\n", clientIndex_, size_);
		PacketData packet;
		packet.Set(clientIndex_, size_, pMsg);
		std::lock_guard<std::mutex> guard(mLock);
		mPacketDataQueue.push_back(packet);
	}

	void Run(const UINT32 maxClient) {
		mIsRunProcessThread = true;
		mProcessThread = std::thread([this]() {ProcessPacket(); }); //packetData에 패킷이 쌓였을 경우 sendMSG호출
		StartServer(maxClient);
	}

	void End() {
		mIsRunProcessThread = false;
		if (mProcessThread.joinable()) {
			mProcessThread.join();
		}
		DestroyThread();
	}

private:

	void ProcessPacket() {
		while (mIsRunProcessThread) {
			auto packetData = DequePacketData();
			if (packetData.DataSize != 0) {
				SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData);
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	PacketData DequePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty()) {
			return PacketData();
		}
		packetData.Set(mPacketDataQueue.front());

		mPacketDataQueue.front().Release(); // release는 왜??
		mPacketDataQueue.pop_front();

		return packetData;
	}


	bool mIsRunProcessThread = false;
	std::thread mProcessThread;
	std::mutex mLock;
	std::deque<PacketData> mPacketDataQueue;
};